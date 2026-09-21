#!/usr/bin/env python3
"""Standalone 2D-FFT HIF parser and analyzer named agentauto.

This script is independent from the AgentAuto CLI/runtime. It decodes 0xC2 HIF
radar_framework/ReportDataCube2D messages, reassembles a complete radar frame,
builds a range-Doppler map, detects strong peaks, and writes CSV/PNG/ASCII
output.

Layout used by the SDK:
    flat_index = (((tx_idx * rx_num + rx_idx) * dop_num + dop_idx)
                  * range_num + range_idx)
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import re
from pathlib import Path


HIF_HEAD_MAGIC = 0xA5
HIF_MSG_TYPE_TO_HOST = 0x02
HIF_MSG_FLAG_CHECK_BIT = 0x04
HIF_MSG_FLAG_EXTEND_BIT = 0x10
HIF_MSG_FLAG_MAC32_BIT = 0x20
HIF_MSG_ID_FFT_DATA = 0xC2
FRAME_UPLOAD_LEN = 12
TL_LEN = 12
REPORT_HEAD_LEN = FRAME_UPLOAD_LEN + TL_LEN
C2_TL_LEN = TL_LEN


class AgentautoError(RuntimeError):
    """Expected user-facing failure for this standalone tool."""


def _s16(value: int) -> int:
    return value - 0x10000 if value >= 0x8000 else value


def _hif_checksum32(data: bytes) -> int:
    total = 0
    offset = 0
    while offset + 4 <= len(data):
        total = (total + int.from_bytes(data[offset:offset + 4], "little")) & 0xFFFFFFFF
        offset += 4
    remaining = len(data) - offset
    if remaining:
        mask = (1 << (remaining * 8)) - 1
        total = (
            total + (int.from_bytes(data[offset:offset + 4], "little") & mask)
        ) & 0xFFFFFFFF
    return total


def _read_hif_capture(path: Path, raw: bool = False) -> bytes:
    if not path.is_file():
        raise AgentautoError(f"HIF capture file does not exist: {path}")
    if path.stat().st_size == 0:
        raise AgentautoError(
            f"HIF capture is empty: {path} has 0 bytes."
        )
    if raw or path.suffix.lower() in (".bin", ".dat"):
        return path.read_bytes()

    text = path.read_text(encoding="utf-8", errors="replace")
    if not text.replace("\ufeff", "").strip():
        raise AgentautoError(
            f"HIF capture is empty: {path} has no bytes."
        )

    tokens: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("serial monitor summary"):
            continue
        if stripped.startswith("RX_HEX="):
            stripped = stripped[len("RX_HEX="):].strip()
        timestamp = re.match(
            r"^\[[0-9]{1,2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]{1,6})?\]\s*(.*)$",
            stripped,
        )
        if timestamp:
            stripped = timestamp.group(1).strip()
        if not stripped:
            continue
        if not re.fullmatch(
            r"(?:[0-9A-Fa-f]{2}\s+)*[0-9A-Fa-f]{2}\s*", stripped
        ):
            continue
        tokens.extend(re.findall(r"[0-9A-Fa-f]{2}", stripped))

    if not tokens:
        raise AgentautoError(
            f"{path} does not contain monitor hex bytes."
        )
    try:
        return bytes.fromhex("".join(tokens))
    except ValueError as exc:
        raise AgentautoError(f"Capture contains invalid hex bytes: {exc}")


def _parse_hif_frames(data: bytes) -> tuple[list[dict[str, object]], int, int]:
    frames: list[dict[str, object]] = []
    invalid_candidates = 0
    partial_frames = 0
    index = 0
    while index + 5 < len(data):
        if data[index] != HIF_HEAD_MAGIC:
            index += 1
            continue

        header = data[index + 2:index + 6]
        expected_phy = (~((HIF_HEAD_MAGIC + sum(header)) & 0xFF)) & 0xFF
        if data[index + 1] != expected_phy:
            index += 1
            continue

        control = int.from_bytes(header, "little")
        msg_type = control & 0x03
        flags = (control >> 2) & 0x3F
        msg_id = (control >> 8) & 0xFF
        length = (control >> 16) & 0xFFF
        seq = (control >> 28) & 0x7
        frag = (control >> 31) & 0x1
        ext_len = 4 if flags & HIF_MSG_FLAG_EXTEND_BIT else 0
        tail_len = 4 if flags & HIF_MSG_FLAG_CHECK_BIT else 0
        total_len = 6 + ext_len + length + tail_len
        if index + total_len > len(data):
            partial_frames += 1
            index += 1
            continue

        body_start = index + 6 + ext_len
        body = data[body_start:body_start + length]
        tail = data[body_start + length:body_start + length + tail_len]
        check32 = None
        check32_ok = None
        check32_note = ""
        if tail_len:
            check32 = int.from_bytes(tail, "little")
            if flags & HIF_MSG_FLAG_MAC32_BIT:
                check32_note = "MAC32"
            else:
                checksum_input = header + data[index + 6:body_start] + body
                check32_ok = (
                    (check32 + _hif_checksum32(checksum_input)) & 0xFFFFFFFF
                ) == 0xFFFFFFFF
                if not check32_ok:
                    invalid_candidates += 1
                    index += 1
                    continue

        frames.append(
            {
                "index": index,
                "raw_hex": " ".join(
                    f"{byte:02X}" for byte in data[index:index + total_len]
                ),
                "header": header,
                "type": msg_type,
                "flags": flags,
                "msg_id": msg_id,
                "length": length,
                "seq": seq,
                "frag": frag,
                "body": body,
                "check8": "OK" if data[index + 1] == expected_phy else "FAIL",
                "check32": check32,
                "check32_ok": check32_ok,
                "check32_note": check32_note,
            }
        )
        index += total_len
    return frames, invalid_candidates, partial_frames


def _decode_c2_fragment(body: bytes) -> dict[str, object]:
    result: dict[str, object] = {
        "valid": False,
        "reason": "",
    }
    if len(body) < FRAME_UPLOAD_LEN:
        result["reason"] = "datacube payload is shorter than MMW_FRAME_UPLOAD"
        return result

    frame_idx = int.from_bytes(body[0:4], "little")
    raw_frame_len = int.from_bytes(body[4:8], "little")
    data_offset = int.from_bytes(body[8:12], "little")
    frame_len = raw_frame_len
    is_first = data_offset == 0
    result.update(
        {
            "msg_id": HIF_MSG_ID_FFT_DATA,
            "byte_units": True,
            "frame_idx": frame_idx,
            "frame_len": frame_len,
            "frame_len_raw": raw_frame_len,
            "data_offset": data_offset,
            "data_offset_raw": data_offset,
        }
    )

    fft_type = None
    tl_total_length = None
    tx_num = None
    rx_num = None
    range_num = None
    dop_num = None
    data_start = FRAME_UPLOAD_LEN
    placement = data_offset

    if is_first:
        if len(body) < REPORT_HEAD_LEN:
            result["reason"] = (
                "first 0xC2 FFT fragment is missing the 12-byte MMW_FRAME_TL"
            )
            return result
        tl_word = int.from_bytes(body[FRAME_UPLOAD_LEN:REPORT_HEAD_LEN], "little")
        fft_type = tl_word & 0xFF
        tl_total_length = (tl_word >> 8) & 0xFFFFFF
        tx_num = body[16]
        rx_num = body[17]
        range_num = int.from_bytes(body[20:22], "little")
        dop_num = int.from_bytes(body[22:24], "little")
        data_start = REPORT_HEAD_LEN
        placement = REPORT_HEAD_LEN

    fft_data = body[data_start:]
    bins: list[dict[str, int]] = []
    for pos in range(0, len(fft_data) - 3, 4):
        dword = int.from_bytes(fft_data[pos:pos + 4], "little")
        bin_offset = (placement + pos - REPORT_HEAD_LEN) // 4
        bins.append(
            {
                "bin_offset": bin_offset,
                "offset_bytes": placement + pos,
                "imag_u16": dword & 0xFFFF,
                "real_u16": (dword >> 16) & 0xFFFF,
                "imag_s16": _s16(dword & 0xFFFF),
                "real_s16": _s16((dword >> 16) & 0xFFFF),
            }
        )

    cumulative_incl_tl = data_offset + len(fft_data)
    if is_first:
        cumulative_incl_tl += TL_LEN
    complete = frame_len > 0 and cumulative_incl_tl == frame_len

    result.update(
        {
            "format": "0xC2-radar-framework",
            "fft_type": fft_type,
            "tl_total_length": tl_total_length,
            "tx_num": tx_num,
            "rx_num": rx_num,
            "range_num": range_num,
            "dop_num": dop_num,
            "data_start": data_start,
            "fft_data_bytes": len(fft_data),
            "bin_count": len(bins),
            "bins": bins,
            "cumulative_incl_tl": cumulative_incl_tl,
            "complete": complete,
            "valid": True,
        }
    )
    if is_first:
        result["expected_tl_total_length"] = (
            (range_num * dop_num * tx_num * rx_num * 4 + 8)
            if all(v is not None for v in (range_num, dop_num, tx_num, rx_num))
            else None
        )
    return result


def db_amplitude(value):
    """Convert linear amplitude to dBFS using int16 full scale as 0 dB."""
    return 20.0 * math.log10(max(abs(value), 1e-9) / 32768.0)


def percentile(values, fraction):
    sorted_values = sorted(values)
    idx = int(len(sorted_values) * fraction)
    idx = max(0, min(idx, len(sorted_values) - 1))
    return sorted_values[idx]


def velocity_for_doppler(dop_idx, dop_num, vel_res_mm):
    half = dop_num // 2
    return (dop_idx - half) * vel_res_mm / 1000.0


def collect_c2_frames(decoded_entries):
    groups = {}
    order = []
    for entry in decoded_entries:
        frame_idx = int(entry.get("frame_idx"))
        if frame_idx not in groups:
            groups[frame_idx] = []
            order.append(frame_idx)
        groups[frame_idx].append(entry)
    return groups, order


def reassemble_frame(entries):
    meta = None
    for entry in entries:
        if int(entry.get("data_offset") or 0) == 0:
            meta = entry
            break
    if meta is None:
        raise RuntimeError("frame is missing the first fragment (TL header)")

    range_num = int(meta.get("range_num"))
    dop_num = int(meta.get("dop_num"))
    tx_num = int(meta.get("tx_num"))
    rx_num = int(meta.get("rx_num"))
    expected_bins = range_num * dop_num * tx_num * rx_num
    samples = [None] * expected_bins

    for entry in entries:
        for item in entry.get("bins", []):
            bin_idx = int(item["bin_offset"])
            if 0 <= bin_idx < expected_bins:
                samples[bin_idx] = complex(
                    int(item["real_s16"]),
                    int(item["imag_s16"]),
                )

    present = sum(1 for item in samples if item is not None)
    complete = present == expected_bins
    return {
        "range_num": range_num,
        "dop_num": dop_num,
        "tx_num": tx_num,
        "rx_num": rx_num,
        "samples": samples,
        "complete": complete,
        "present_bins": present,
        "expected_bins": expected_bins,
        "fft_type": int(meta.get("fft_type") or 0),
        "frame_len": int(meta.get("frame_len") or 0),
    }


def build_amplitude_matrix(meta, tx_select, rx_select, combine):
    range_num = meta["range_num"]
    dop_num = meta["dop_num"]
    tx_num = meta["tx_num"]
    rx_num = meta["rx_num"]
    samples = meta["samples"]
    matrix = [[0.0] * dop_num for _ in range(range_num)]

    for range_idx in range(range_num):
        for dop_idx in range(dop_num):
            if combine:
                total_power = 0.0
                channel_count = 0
                for tx_idx in range(tx_num):
                    for rx_idx in range(rx_num):
                        flat = (
                            (tx_idx * rx_num + rx_idx) * dop_num + dop_idx
                        ) * range_num + range_idx
                        value = samples[flat]
                        if value is not None:
                            total_power += abs(value) ** 2
                            channel_count += 1
                if channel_count:
                    matrix[range_idx][dop_idx] = math.sqrt(
                        total_power / channel_count
                    )
            else:
                flat = (
                    (tx_select * rx_num + rx_select) * dop_num + dop_idx
                ) * range_num + range_idx
                value = samples[flat]
                if value is not None:
                    matrix[range_idx][dop_idx] = abs(value)
    return matrix


def detect_peaks(amp_db, range_num, dop_num, margin_db):
    flat = [amp_db[r][d] for r in range(range_num) for d in range(dop_num)]
    noise_db = percentile(flat, 0.5)
    threshold_db = noise_db + margin_db
    peaks = []

    for range_idx in range(range_num):
        for dop_idx in range(dop_num):
            value = amp_db[range_idx][dop_idx]
            if value < threshold_db:
                continue

            neighbors = []
            for rr in range(max(0, range_idx - 1), min(range_num, range_idx + 2)):
                for dd in range(max(0, dop_idx - 1), min(dop_num, dop_idx + 2)):
                    neighbors.append(amp_db[rr][dd])
            if value < max(neighbors):
                continue

            duplicate = False
            for rr in range(max(0, range_idx - 1), min(range_num, range_idx + 2)):
                for dd in range(max(0, dop_idx - 1), min(dop_num, dop_idx + 2)):
                    if (
                        amp_db[rr][dd] == value
                        and (rr < range_idx or (rr == range_idx and dd < dop_idx))
                    ):
                        duplicate = True
                        break
                if duplicate:
                    break
            if duplicate:
                continue

            peaks.append(
                {
                    "range_idx": range_idx,
                    "dop_idx": dop_idx,
                    "power_db": value,
                    "snr_db": value - noise_db,
                }
            )

    peaks.sort(key=lambda item: item["power_db"], reverse=True)
    return noise_db, peaks


def sample_matrix(matrix, max_rows=32, max_cols=64):
    rows = len(matrix)
    cols = len(matrix[0]) if rows else 0
    row_step = max(1, math.ceil(rows / max_rows))
    col_step = max(1, math.ceil(cols / max_cols))
    result = []
    for r in range(0, rows, row_step):
        row = []
        for c in range(0, cols, col_step):
            row.append(matrix[r][c])
        result.append(row)
    return result


def print_ascii_heatmap(amp_db, range_num, dop_num):
    chars = " .:-=+*#%@"
    rows = sample_matrix(amp_db, max_rows=32, max_cols=64)
    lo = min(min(row) for row in rows)
    hi = max(max(row) for row in rows)
    span = max(hi - lo, 1e-9)
    print("ASCII_HEATMAP_START")
    for row in rows:
        print(
            "".join(
                chars[min(len(chars) - 1, int((value - lo) / span * (len(chars) - 1)))]
                for value in row
            )
        )
    print("ASCII_HEATMAP_END")


def write_png(path, amp_db):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as exc:
        print(f"PNG_SKIPPED=matplotlib unavailable: {exc}")
        return False

    fig, ax = plt.subplots(figsize=(10, 7), dpi=150)
    mesh = ax.imshow(
        amp_db,
        aspect="auto",
        origin="lower",
        cmap="turbo",
        interpolation="nearest",
    )
    fig.colorbar(mesh, ax=ax, label="Power dBFS")
    ax.set_xlabel("Doppler bin")
    ax.set_ylabel("Range bin")
    ax.set_title("2D-FFT Range-Doppler Heatmap")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
    return True


def write_targets_csv(path, frame_idx, peaks, range_res_mm, dop_num, vel_res_mm):
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "frame_idx",
                "range_idx",
                "dop_idx",
                "range_m",
                "velocity_m/s",
                "power_db",
                "snr_db",
            ]
        )
        for peak in peaks:
            writer.writerow(
                [
                    frame_idx,
                    peak["range_idx"],
                    peak["dop_idx"],
                    round(peak["range_idx"] * range_res_mm / 1000.0, 4),
                    round(
                        velocity_for_doppler(
                            peak["dop_idx"], dop_num, vel_res_mm
                        ),
                        4,
                    ),
                    round(peak["power_db"], 3),
                    round(peak["snr_db"], 3),
                ]
            )
    print(f"TARGETS_CSV={Path(path).resolve()}")


def write_bins_csv(path, frame_idx, amp_db, range_res_mm, dop_num, vel_res_mm):
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "frame_idx",
                "range_idx",
                "dop_idx",
                "range_m",
                "velocity_m/s",
                "power_db",
            ]
        )
        for range_idx, row in enumerate(amp_db):
            for dop_idx, value in enumerate(row):
                writer.writerow(
                    [
                        frame_idx,
                        range_idx,
                        dop_idx,
                        round(range_idx * range_res_mm / 1000.0, 4),
                        round(
                            velocity_for_doppler(dop_idx, dop_num, vel_res_mm),
                            4,
                        ),
                        round(value, 3),
                    ]
                )
    print(f"BINS_CSV={Path(path).resolve()}")


def build_hif_frame(body):
    control = (
        HIF_MSG_TYPE_TO_HOST
        | (HIF_MSG_FLAG_CHECK_BIT << 2)
        | (HIF_MSG_ID_FFT_DATA << 8)
        | (len(body) << 16)
    )
    header = control.to_bytes(4, "little")
    check8 = (~((HIF_HEAD_MAGIC + sum(header)) & 0xFF)) & 0xFF
    check32 = (~_hif_checksum32(header + body)) & 0xFFFFFFFF
    return (
        bytes((HIF_HEAD_MAGIC, check8))
        + header
        + body
        + check32.to_bytes(4, "little")
    )


def make_demo_capture():
    range_num = 8
    dop_num = 4
    tx_num = 1
    rx_num = 1
    data_len = range_num * dop_num * tx_num * rx_num * 4
    tl_length = data_len + 8
    frame_len = data_len + C2_TL_LEN
    rng = random.Random(42)

    wave = []
    target_dop = dop_num // 2 + 1
    for tx_idx in range(tx_num):
        for rx_idx in range(rx_num):
            for dop_idx in range(dop_num):
                for range_idx in range(range_num):
                    amplitude = 22000.0
                    range_gain = math.exp(
                        -((range_idx - 5) ** 2) / (2.0 * 0.5)
                    )
                    dop_gain = 1.0 if dop_idx == target_dop else 0.04
                    noise = rng.uniform(-260, 260)
                    real = int(amplitude * range_gain * dop_gain + noise)
                    imag = int(
                        amplitude * range_gain * dop_gain * 0.35 + noise * 0.3
                    )
                    wave.append((real, imag))

    tl_word = (3 & 0xFF) | ((tl_length & 0xFFFFFF) << 8)
    first_body = bytearray()
    first_body += (7).to_bytes(4, "little")
    first_body += frame_len.to_bytes(4, "little")
    first_body += (0).to_bytes(4, "little")
    first_body += tl_word.to_bytes(4, "little")
    first_body += bytes((tx_num, rx_num, 0, 0))
    first_body += range_num.to_bytes(2, "little")
    first_body += dop_num.to_bytes(2, "little")
    for real, imag in wave:
        first_body += (imag & 0xFFFF).to_bytes(2, "little")
        first_body += (real & 0xFFFF).to_bytes(2, "little")

    return build_hif_frame(bytes(first_body))


def analyze_capture(data):
    frames, invalid, partial = _parse_hif_frames(data)
    decoded = []
    for frame in frames:
        if int(frame["msg_id"]) != HIF_MSG_ID_FFT_DATA:
            continue
        if not isinstance(frame["body"], bytes):
            continue
        entry = _decode_c2_fragment(frame["body"])
        if entry.get("valid"):
            decoded.append(entry)

    groups, order = collect_c2_frames(decoded)
    complete_ids = {
        frame_idx
        for frame_idx, entries in groups.items()
        if any(entry.get("complete") for entry in entries)
    }
    return frames, invalid, partial, groups, order, complete_ids


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Decode a 0xC2 radar_framework HIF capture into useful "
            "range-Doppler targets."
        )
    )
    parser.add_argument("capture", nargs="?", help="AgentAuto monitor hex log or raw file")
    parser.add_argument("--raw", action="store_true", help="capture is a raw binary file")
    parser.add_argument("--demo", action="store_true", help="run on a synthetic 2D-FFT frame")
    parser.add_argument("--frame-idx", type=int, default=None)
    parser.add_argument("--range-res-mm", type=float, default=80.0)
    parser.add_argument("--vel-res-mm", type=float, default=250.0)
    parser.add_argument("--threshold-db", type=float, default=12.0)
    parser.add_argument("--top", type=int, default=5)
    parser.add_argument("--tx", type=int, default=0)
    parser.add_argument("--rx", type=int, default=0)
    parser.add_argument("--combine", action="store_true", help="average magnitude power over all channels")
    parser.add_argument("--out", help="targets CSV path")
    parser.add_argument("--bins-out", help="full bins CSV path")
    parser.add_argument("--png", help="PNG heatmap path")
    parser.add_argument("--ascii", action="store_true", help="print ASCII heatmap")
    parser.add_argument("--json", action="store_true", help="print machine-readable summary")
    args = parser.parse_args()

    if args.demo:
        data = make_demo_capture()
        input_name = "demo"
    else:
        if not args.capture:
            parser.error("capture path or --demo is required")
        data = _read_hif_capture(Path(args.capture), raw=bool(args.raw))
        input_name = str(Path(args.capture).resolve())

    frames, invalid, partial, groups, order, complete_ids = analyze_capture(data)
    if not decoded_frames(groups):
        print("ERROR: no valid 0xC2 datacube frames in capture.")
        return 1

    frame_idx = args.frame_idx
    if frame_idx is not None:
        if frame_idx not in groups:
            print(f"ERROR: frame_idx {frame_idx} was not found.")
            return 1
        if frame_idx not in complete_ids:
            print(f"ERROR: frame_idx {frame_idx} is not complete.")
            return 1
    else:
        for candidate in order:
            if candidate in complete_ids:
                frame_idx = candidate
                break
        if frame_idx is None:
            print("ERROR: no complete 0xC2 datacube frame in capture; capture longer.")
            return 1

    meta = reassemble_frame(groups[frame_idx])
    if not meta["complete"]:
        print(
            f"ERROR: frame {frame_idx} incomplete "
            f"({meta['present_bins']}/{meta['expected_bins']} bins)."
        )
        return 1

    if args.tx >= meta["tx_num"] or args.rx >= meta["rx_num"]:
        print(f"ERROR: channel TX{args.tx}/RX{args.rx} out of range.")
        return 1

    amplitude = build_amplitude_matrix(
        meta, args.tx, args.rx, bool(args.combine)
    )
    amp_db = [
        [db_amplitude(value) for value in row]
        for row in amplitude
    ]
    noise_db, peaks = detect_peaks(
        amp_db, meta["range_num"], meta["dop_num"], float(args.threshold_db)
    )

    summary = {
        "input": input_name,
        "frame_idx": frame_idx,
        "fft_type": meta["fft_type"],
        "tx_num": meta["tx_num"],
        "rx_num": meta["rx_num"],
        "range_bins": meta["range_num"],
        "dop_bins": meta["dop_num"],
        "mimo_shown": "combined" if args.combine else f"TX{args.tx}-RX{args.rx}",
        "channel_bins": meta["range_num"] * meta["dop_num"],
        "noise_db": round(noise_db, 3),
        "threshold_db": round(noise_db + args.threshold_db, 3),
        "peak_count": len(peaks),
        "peaks": [],
    }
    for peak in peaks[: args.top]:
        record = {
            "range_idx": peak["range_idx"],
            "dop_idx": peak["dop_idx"],
            "range_m": round(peak["range_idx"] * args.range_res_mm / 1000.0, 4),
            "velocity_m_s": round(
                velocity_for_doppler(
                    peak["dop_idx"], meta["dop_num"], args.vel_res_mm
                ),
                4,
            ),
            "power_db": round(peak["power_db"], 3),
            "snr_db": round(peak["snr_db"], 3),
        }
        summary["peaks"].append(record)

    if args.json:
        print(json.dumps(summary, indent=2, ensure_ascii=False))
    else:
        print("2DFFT_ANALYZE_OK=1")
        print(f"INPUT={input_name}")
        print(f"VALID_FRAMES={len(frames)}")
        print(f"INVALID_CANDIDATES={invalid}")
        print(f"PARTIAL_FRAMES={partial}")
        print(f"C2_MESSAGES={sum(len(entries) for entries in groups.values())}")
        print(f"C2_COMPLETE_FRAMES={len(complete_ids)}")
        print(f"FRAME_IDX={frame_idx}")
        print(f"TL_TYPE={meta['fft_type']}")
        print(f"TX_NUM={meta['tx_num']}")
        print(f"RX_NUM={meta['rx_num']}")
        print(f"RANGE_BINS={meta['range_num']}")
        print(f"DOP_BINS={meta['dop_num']}")
        print(f"PRESENT_CHANNEL_BINS={meta['present_bins']}")
        print(f"NOISE_DB={round(noise_db, 3)}")
        print(f"THRESHOLD_DB={round(noise_db + args.threshold_db, 3)}")
        print(f"PEAK_COUNT={len(peaks)}")
        for index, peak in enumerate(summary["peaks"], start=1):
            print(
                f"PEAK_{index}=range={peak['range_m']}m "
                f"vel={peak['velocity_m_s']}m/s "
                f"power_db={peak['power_db']} snr_db={peak['snr_db']} "
                f"bin={peak['range_idx']}/{peak['dop_idx']}"
            )

    if args.out:
        write_targets_csv(
            args.out,
            frame_idx,
            peaks,
            args.range_res_mm,
            meta["dop_num"],
            args.vel_res_mm,
        )
    if args.bins_out:
        write_bins_csv(
            args.bins_out,
            frame_idx,
            amp_db,
            args.range_res_mm,
            meta["dop_num"],
            args.vel_res_mm,
        )
    if args.png:
        path = Path(args.png)
        path.parent.mkdir(parents=True, exist_ok=True)
        if write_png(str(path), amp_db):
            print(f"PNG={path.resolve()}")
    if args.ascii:
        print_ascii_heatmap(amp_db, meta["range_num"], meta["dop_num"])

    return 0


def decoded_frames(groups):
    return bool(groups)


if __name__ == "__main__":
    raise SystemExit(main())
