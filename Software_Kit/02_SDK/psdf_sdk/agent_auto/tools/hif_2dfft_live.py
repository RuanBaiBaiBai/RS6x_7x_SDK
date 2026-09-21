#!/usr/bin/env python3
"""Live 2D-FFT HIF example for AgentAuto.

This script starts `AgentAuto monitor --hex --no-timestamp`, streams the raw
UART hex lines, reassembles complete HIF 0xC2 radar_framework datacube frames,
builds a range-Doppler amplitude map per frame, and updates a live heatmap.

The radar frame layout is:
    flat_index = (((tx_idx * rx_num + rx_idx) * dop_num + dop_idx)
                  * range_num + range_idx)
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import random
import re
import subprocess
import sys
import time
from pathlib import Path


def find_agentauto_engine():
    """Locate the AgentAuto runtime engine without mixing tool folders."""
    env_value = os.environ.get("AGENT_AUTO_ENGINE")
    if env_value:
        env_path = Path(env_value).expanduser()
        if env_path.is_file() and env_path.name == "agentauto_engine.py":
            return env_path.resolve()
        if (env_path / "agentauto_engine.py").is_file():
            return (env_path / "agentauto_engine.py").resolve()

    here = Path(__file__).resolve().parent
    roots = [Path.cwd().resolve(), here, *here.parents]
    seen = set()
    for root in roots:
        key = str(root).lower()
        if key in seen:
            continue
        seen.add(key)
        candidate = root / "runtime" / "agentauto_engine.py"
        if candidate.is_file():
            return candidate.resolve()
        candidate = root / "agent_auto" / "runtime" / "agentauto_engine.py"
        if candidate.is_file():
            return candidate.resolve()
    return None


ENGINE_PATH = find_agentauto_engine()
if ENGINE_PATH is None:
    print(
        "ERROR: agentauto_engine.py was not found. Set "
        "AGENT_AUTO_ENGINE to the AgentAuto runtime engine path.",
        file=sys.stderr,
    )
    raise SystemExit(1)
RUNTIME_DIR = ENGINE_PATH.parent
sys.path.insert(0, str(RUNTIME_DIR))

from agentauto_engine import (  # noqa: E402
    C2_TL_LEN,
    HIF_HEAD_MAGIC,
    HIF_MSG_FLAG_CHECK_BIT,
    HIF_MSG_FLAG_EXTEND_BIT,
    HIF_MSG_FLAG_MAC32_BIT,
    HIF_MSG_TYPE_TO_HOST,
    HIF_MSG_ID_FFT_DATA,
    _decode_c2_fragment,
    _hif_checksum32,
)


def configure_console() -> None:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass


HEX_LINE_RE = re.compile(r"(?:[0-9A-Fa-f]{2}(?:\s+|$))+")


def hex_bytes_from_line(line: str):
    stripped = line.strip()
    if not stripped:
        return None
    if stripped.startswith("RX_HEX="):
        stripped = stripped[len("RX_HEX="):].strip()
    if not HEX_LINE_RE.fullmatch(stripped):
        return None
    try:
        return bytes.fromhex("".join(re.findall(r"[0-9A-Fa-f]{2}", stripped)))
    except ValueError:
        return None


def percentile(values, fraction):
    sorted_values = sorted(values)
    idx = int(len(sorted_values) * fraction)
    idx = max(0, min(idx, len(sorted_values) - 1))
    return sorted_values[idx]


def db_amplitude(value):
    return 20.0 * math.log10(max(abs(value), 1e-9) / 32768.0)


def velocity_for_doppler(dop_idx, dop_num, vel_res_mm):
    half = dop_num // 2
    return (dop_idx - half) * vel_res_mm / 1000.0


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
    return {
        "range_num": range_num,
        "dop_num": dop_num,
        "tx_num": tx_num,
        "rx_num": rx_num,
        "samples": samples,
        "complete": present == expected_bins,
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


class HifStreamDecoder:
    """Incremental HIF frame parser for continuous UART data."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.invalid_candidates = 0

    def feed(self, chunk: bytes) -> list[dict]:
        self.buffer.extend(chunk)
        frames: list[dict] = []

        while True:
            if len(self.buffer) < 2:
                break
            if self.buffer[0] != HIF_HEAD_MAGIC:
                try:
                    magic_index = self.buffer.index(HIF_HEAD_MAGIC)
                except ValueError:
                    self.buffer.clear()
                    break
                if magic_index:
                    del self.buffer[:magic_index]
                continue
            if len(self.buffer) < 6:
                break

            header = bytes(self.buffer[2:6])
            expected_phy = (~((HIF_HEAD_MAGIC + sum(header)) & 0xFF)) & 0xFF
            if self.buffer[1] != expected_phy:
                del self.buffer[:1]
                continue

            control = int.from_bytes(header, "little")
            flags = (control >> 2) & 0x3F
            length = (control >> 16) & 0xFFF
            ext_len = 4 if flags & HIF_MSG_FLAG_EXTEND_BIT else 0
            tail_len = 4 if flags & HIF_MSG_FLAG_CHECK_BIT else 0
            total_len = 6 + ext_len + length + tail_len
            if len(self.buffer) < total_len:
                break

            ext_bytes = bytes(self.buffer[6:6 + ext_len])
            body_start = 6 + ext_len
            body = bytes(self.buffer[body_start:body_start + length])
            tail = bytes(
                self.buffer[body_start + length:body_start + length + tail_len]
            )
            check32 = None
            check32_ok = None
            if tail_len:
                check32 = int.from_bytes(tail, "little")
                if not (flags & HIF_MSG_FLAG_MAC32_BIT):
                    checksum_input = header + ext_bytes + body
                    check32_ok = (
                        (check32 + _hif_checksum32(checksum_input)) & 0xFFFFFFFF
                    ) == 0xFFFFFFFF
                    if not check32_ok:
                        self.invalid_candidates += 1
                        del self.buffer[:1]
                        continue

            frames.append(
                {
                    "raw_hex": " ".join(
                        f"{byte:02X}" for byte in self.buffer[:total_len]
                    ),
                    "header": header,
                    "type": control & 0x03,
                    "flags": flags,
                    "msg_id": (control >> 8) & 0xFF,
                    "length": length,
                    "seq": (control >> 28) & 0x7,
                    "frag": (control >> 31) & 0x1,
                    "body": body,
                    "check8": "OK",
                    "check32": check32,
                    "check32_ok": check32_ok,
                }
            )
            del self.buffer[:total_len]

        return frames


class HeatmapView:
    """Owns the optional live matplotlib figure and PNG output."""

    def __init__(self, headless: bool, png: str) -> None:
        self.headless = headless
        self.png = png
        self.figure = None
        self.axis = None
        self.mesh = None

    def update(self, amp_db, range_num: int, dop_num: int, peaks: list) -> None:
        if not self.headless:
            try:
                import matplotlib.pyplot as plt
            except Exception:
                plt = None
            if plt is not None:
                if self.figure is None:
                    plt.ion()
                    self.figure, self.axis = plt.subplots(figsize=(10, 7))
                    self.mesh = self.axis.imshow(
                        amp_db,
                        aspect="auto",
                        origin="lower",
                        cmap="turbo",
                        interpolation="nearest",
                    )
                    self.figure.colorbar(
                        self.mesh, ax=self.axis, label="Power dBFS"
                    )
                    self.axis.set_xlabel("Doppler bin")
                    self.axis.set_ylabel("Range bin")
                else:
                    self.mesh.set_data(amp_db)

                values = [
                    item for row in amp_db for item in row
                ]
                if values:
                    self.mesh.set_clim(min(values), max(values))
                top_text = ""
                if peaks:
                    first = peaks[0]
                    top_text = f" | range_bin={first['range_idx']} dop_bin={first['dop_idx']}"
                self.axis.set_title(
                    f"2D-FFT Range-Doppler Live | R={range_num} D={dop_num}"
                    f"{top_text}"
                )
                self.figure.canvas.draw_idle()
                self.figure.canvas.flush_events()

        if self.png:
            path = Path(self.png).expanduser()
            path.parent.mkdir(parents=True, exist_ok=True)
            write_png(str(path), amp_db)


class Live2DFft:
    """Streams 0xC2 datacube fragments and emits useful range-Doppler results."""

    def __init__(self, args) -> None:
        self.args = args
        self.decoder = HifStreamDecoder()
        self.entries_by_frame: dict[int, list] = {}
        self.complete_frames: list[dict] = []
        self.fft_messages = 0
        self.invalid_candidates = 0
        self.frame_count = 0
        self.view = HeatmapView(bool(args.headless), args.png or "")

    def feed_bytes(self, data: bytes) -> None:
        for frame in self.decoder.feed(data):
            self.handle_frame(frame)

    def handle_frame(self, frame: dict) -> None:
        if int(frame.get("msg_id") or 0) != HIF_MSG_ID_FFT_DATA:
            return
        body = frame.get("body")
        if not isinstance(body, bytes):
            return
        entry = _decode_c2_fragment(body)
        if not entry.get("valid"):
            self.invalid_candidates += 1
            return
        self.fft_messages += 1
        frame_idx = int(entry["frame_idx"])
        if frame_idx not in self.entries_by_frame:
            self.entries_by_frame[frame_idx] = []
        self.entries_by_frame[frame_idx].append(entry)
        self._try_finish_frame(frame_idx)

    def _try_finish_frame(self, frame_idx: int) -> None:
        entries = self.entries_by_frame.get(frame_idx)
        if not entries:
            return
        if not any(bool(entry.get("complete")) for entry in entries):
            return
        try:
            meta = reassemble_frame(entries)
        except RuntimeError:
            return
        if not meta["complete"]:
            return

        meta["lfd_frame_idx"] = frame_idx
        self.entries_by_frame.pop(frame_idx, None)
        self.complete_frames.append(meta)
        self.frame_count += 1
        self.analyze_frame(meta)

    def analyze_frame(self, meta: dict) -> None:
        args = self.args
        tx_num = meta["tx_num"]
        rx_num = meta["rx_num"]
        if args.tx >= tx_num or args.rx >= rx_num:
            print(
                f"LIVE_CHANNEL_ERROR=TX{args.tx}/RX{args.rx} out of "
                f"range TX{tx_num}/RX{rx_num}"
            )
            return

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

        print("LIVE_FRAME_READY=1", flush=True)
        print(f"LIVE_FRAME_COUNT={self.frame_count}", flush=True)
        print(f"LIVE_FRAME_IDX={meta.get('lfd_frame_idx', self.frame_count)}", flush=True)
        print(f"LIVE_FFT_MESSAGES={self.fft_messages}", flush=True)
        print(f"LIVE_RANGE_BINS={meta['range_num']}", flush=True)
        print(f"LIVE_DOP_BINS={meta['dop_num']}", flush=True)
        print(f"LIVE_CHANNELS=TX{meta['tx_num']}RX{meta['rx_num']}", flush=True)
        print(f"LIVE_NOISE_DB={round(noise_db, 3)}", flush=True)
        print(
            f"LIVE_THRESHOLD_DB={round(noise_db + args.threshold_db, 3)}",
            flush=True,
        )
        print(f"LIVE_PEAK_COUNT={len(peaks)}", flush=True)

        for index, peak in enumerate(peaks[: args.top], start=1):
            print(
                f"LIVE_PEAK_{index}=range={round(peak['range_idx'] * args.range_res_mm / 1000.0, 4)}m "
                f"vel={round(velocity_for_doppler(peak['dop_idx'], meta['dop_num'], args.vel_res_mm), 4)}m/s "
                f"power_db={round(peak['power_db'], 3)} "
                f"snr_db={round(peak['snr_db'], 3)} "
                f"bin={peak['range_idx']}/{peak['dop_idx']}",
                flush=True,
            )

        if args.ascii:
            print_ascii_heatmap(amp_db, meta["range_num"], meta["dop_num"])
        if args.out:
            out_path = Path(args.out).expanduser().resolve()
            out_path.parent.mkdir(parents=True, exist_ok=True)
            write_targets_csv(
                str(out_path),
                int(meta.get("lfd_frame_idx", self.frame_count)),
                peaks,
                args.range_res_mm,
                meta["dop_num"],
                args.vel_res_mm,
            )
        if args.bins_out:
            bins_path = Path(args.bins_out).expanduser().resolve()
            bins_path.parent.mkdir(parents=True, exist_ok=True)
            write_bins_csv(
                str(bins_path),
                int(meta.get("lfd_frame_idx", self.frame_count)),
                amp_db,
                args.range_res_mm,
                meta["dop_num"],
                args.vel_res_mm,
            )

        self.view.update(amp_db, meta["range_num"], meta["dop_num"], peaks)

    def summary(self) -> None:
        print(
            "LIVE_SUMMARY="
            f"complete_frames={self.frame_count} "
            f"fft_messages={self.fft_messages} "
            f"invalid_candidates={self.invalid_candidates}",
            flush=True,
        )


def print_ascii_heatmap(amp_db, range_num: int, dop_num: int) -> None:
    chars = " .:-=+*#%@"
    max_rows = 32
    max_cols = 64
    rows = len(amp_db)
    cols = len(amp_db[0]) if rows else 0
    row_step = max(1, rows // max_rows + (1 if rows % max_rows else 0))
    col_step = max(1, cols // max_cols + (1 if cols % max_cols else 0))
    sampled = [
        [amp_db[r][c] for c in range(0, cols, col_step)]
        for r in range(0, rows, row_step)
    ]
    lo = min(min(row) for row in sampled)
    hi = max(max(row) for row in sampled)
    span = max(hi - lo, 1e-9)
    print("ASCII_HEATMAP_START", flush=True)
    for row in sampled:
        print(
            "".join(
                chars[
                    min(
                        len(chars) - 1,
                        int((value - lo) / span * (len(chars) - 1)),
                    )
                ]
                for value in row
            ),
            flush=True,
        )
    print("ASCII_HEATMAP_END", flush=True)


def resolve_agent_auto(path_value: str) -> Path:
    path = Path(path_value).expanduser()
    if not path.is_absolute():
        path = Path.cwd() / path
    return path.resolve()


def run_monitor_stream(args, live: Live2DFft) -> int:
    script = resolve_agent_auto(args.agent_auto)
    cmd = [
        sys.executable,
        "-u",
        str(script),
        "monitor",
        "--port",
        str(args.port),
        "--baud",
        str(args.baud),
        "--duration",
        str(args.duration),
        "--hex",
        "--no-timestamp",
        "--idle-timeout",
        "0",
    ]
    if args.max_bytes > 0:
        cmd.extend(["--max-bytes", str(args.max_bytes)])

    print(
        f"LIVE_MONITOR_START=port={args.port} baud={args.baud} "
        f"duration={args.duration}s agent_auto={script}",
        flush=True,
    )
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    try:
        for raw_line in proc.stdout:
            line = raw_line.rstrip("\r\n")
            data = hex_bytes_from_line(line)
            if data is not None:
                live.feed_bytes(data)
            elif line.strip():
                print(line, flush=True)
            if (
                args.max_frames > 0
                and live.frame_count >= args.max_frames
            ):
                break
    finally:
        if proc.poll() is None:
            try:
                proc.terminate()
                proc.wait(timeout=3)
            except (OSError, subprocess.TimeoutExpired):
                pass
        if proc.poll() is None and os.name == "nt":
            try:
                subprocess.run(
                    ["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=False,
                )
            except OSError:
                pass
    return int(proc.poll() or 0)


def run_demo(args, live: Live2DFft) -> int:
    target = args.max_frames if args.max_frames > 0 else 3
    interval = max(0.0, args.demo_interval)
    print(
        f"LIVE_DEMO=1 frames={target} interval={interval}s",
        flush=True,
    )
    for _ in range(target):
        data = make_demo_capture()
        if interval <= 0:
            live.feed_bytes(data)
            continue

        chunk_size = max(1, int(interval * 100))
        for offset in range(0, len(data), chunk_size):
            live.feed_bytes(data[offset:offset + chunk_size])
            time.sleep(interval / 20.0)
        time.sleep(interval)
        if live.frame_count >= target:
            break
    return 0


def main(argv=None) -> int:
    configure_console()
    parser = argparse.ArgumentParser(
        description="Real-time 2D-FFT heatmap from the AgentAuto HIF monitor.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--port", help="HIF UART COM port, for example COM101")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument(
        "--agent-auto",
        default=str(RUNTIME_DIR.parent / "modules" / "core" / "cli.py"),
        help="Path to the AgentAuto CLI script.",
    )
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--demo-interval", type=float, default=0.15)
    parser.add_argument("--range-res-mm", type=float, default=80.0)
    parser.add_argument("--vel-res-mm", type=float, default=250.0)
    parser.add_argument("--threshold-db", type=float, default=12.0)
    parser.add_argument("--top", type=int, default=5)
    parser.add_argument("--tx", type=int, default=0)
    parser.add_argument("--rx", type=int, default=0)
    parser.add_argument("--combine", action="store_true")
    parser.add_argument("--out", help="Latest targets CSV path")
    parser.add_argument("--bins-out", help="Latest full bins CSV path")
    parser.add_argument("--png", help="Latest PNG heatmap path")
    parser.add_argument("--ascii", action="store_true")
    parser.add_argument("--headless", "--no-gui", dest="headless", action="store_true")
    parser.add_argument("--max-frames", type=int, default=0)
    parser.add_argument("--max-bytes", type=int, default=0)
    args = parser.parse_args(argv)

    if not args.demo and not args.port:
        parser.error("--port is required unless --demo is used")
    if args.duration <= 0:
        parser.error("--duration must be greater than 0")
    if args.demo_interval < 0:
        parser.error("--demo-interval cannot be negative")
    if args.top <= 0:
        parser.error("--top must be greater than 0")

    live = Live2DFft(args)
    try:
        if args.demo:
            result = run_demo(args, live)
        else:
            result = run_monitor_stream(args, live)
    finally:
        live.summary()
    return result


if __name__ == "__main__":
    raise SystemExit(main())
