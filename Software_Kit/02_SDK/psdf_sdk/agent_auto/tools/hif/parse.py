"""Parse saved HIF raw traffic and extract C1/C2 FFT DataCube reports.

Example:
  python agent_auto/tools/hif/parse.py --input raw_data/raw_20260910_121530.bin --out parsed

Outputs:
  parsed/manifest_frames.csv       every valid HIF frame
  parsed/manifest_messages.csv     complete logical HIF message payloads
  parsed/manifest_datacube.csv     completed C1/C2 cubes
  parsed/messages/msg_*.bin        payload of every valid HIF message
  parsed/datacube/cube_*.bin       complete composite frame (UPLOAD + TL + IQ)
  parsed/datacube/cube_*.iq.bin    IQ DWORD stream (imag low16, real high16)
  parsed/datacube/cube_*.npz       optional numpy arrays
  parsed/summary.json              parse statistics
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import sys
import time
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from hif_parser import (  # noqa: E402
    DataCubeFrame,
    DataPieceAssembler,
    HifFrame,
    HifFragmentAssembler,
    MMW_MSG_C1,
    MMW_MSG_C2,
    MMW_MSG_C3,
    MMW_MSG_C6,
    default_cube_shape,
    parse_frame_info,
    parse_point_cloud_data,
    stream_hif_frames,
)


FRAME_FIELDS = [
    "seq",
    "input",
    "byte_offset",
    "msg_id",
    "type",
    "flag",
    "msg_length",
    "seq_field",
    "frag",
    "checksum8",
    "checksum32",
    "status",
]

DATACUBE_FIELDS = [
    "seq",
    "input",
    "msg_id",
    "frame_idx",
    "frame_len",
    "data_offset",
    "tx",
    "rx",
    "range_bins",
    "doppler_bins",
    "tl_type",
    "tl_length",
    "iq_samples",
    "shape",
    "piece_count",
    "raw_file",
    "iq_file",
    "npz_file",
]

MESSAGE_FIELDS = [
    "seq",
    "input",
    "msg_id",
    "source_frame_seq",
    "frag",
    "payload_len",
    "payload_file",
]

POINT_CLOUD_FIELDS = [
    "seq",
    "time",
    "frame_index",
    "signal_name",
    "dim",
    "point_num",
    "idx",
    "x",
    "y",
    "z",
    "w",
    "u",
    "v",
]

FRAME_INFO_FIELDS = [
    "time",
    "epoch_ms",
    "frame_index",
    "type",
    "chirp_loops",
    "range_samples",
    "range_fft",
    "doppler_samples",
    "doppler_fft",
    "extra1",
    "antenna_count",
    "motion_points",
    "presence_points",
]


def parse_shape(text: str) -> Optional[Tuple[int, int, int, int]]:
    if not text.strip():
        return None
    parts = text.split(",")
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("--shape must be TX,RX,DFFT,RFFT")
    return tuple(int(item) for item in parts)  # type: ignore[return-value]


def frame_row(
    seq: int,
    input_name: str,
    frame: HifFrame,
) -> Dict[str, object]:
    h = frame.header
    return {
        "seq": seq,
        "input": input_name,
        "byte_offset": frame.offset,
        "msg_id": f"0x{h.msg_id:02X}",
        "type": h.type_,
        "flag": f"0x{h.flag:02X}",
        "msg_length": h.length,
        "seq_field": h.seq,
        "frag": h.frag,
        "checksum8": "ok" if frame.status in ("ok", "bad_checksum") else "",
        "checksum32": (
            "ok"
            if frame.checksum_ok is True
            else ("bad" if frame.checksum_ok is False else "none")
        ),
        "status": frame.status,
    }


def datacube_row(
    seq: int,
    input_name: str,
    shape: Optional[Tuple[int, int, int, int]],
    cube: DataCubeFrame,
    raw_file: Path,
    iq_file: Path,
    npz_file: Optional[Path],
) -> Dict[str, object]:
    row: Dict[str, object] = {
        "seq": seq,
        "input": input_name,
        "msg_id": f"0x{cube.msg_id:02X}",
        "frame_idx": cube.frame_idx,
        "frame_len": cube.header.frame_len,
        "data_offset": cube.header.data_offset,
        "tx": cube.tl.tx_num if cube.tl is not None else "",
        "rx": cube.tl.rx_num if cube.tl is not None else "",
        "range_bins": cube.tl.range_bin_num if cube.tl is not None else "",
        "doppler_bins": cube.tl.dop_bin_num if cube.tl is not None else "",
        "tl_type": cube.tl.type_ if cube.tl is not None else "",
        "tl_length": cube.tl.total_length if cube.tl is not None else "",
        "iq_samples": cube.n_samples,
        "shape": str(shape) if shape else "",
        "piece_count": cube.piece_count,
        "raw_file": str(raw_file),
        "iq_file": str(iq_file),
        "npz_file": str(npz_file) if npz_file is not None else "",
    }
    return row


def save_npz(
    cube: DataCubeFrame,
    npz_path: Path,
    shape: Optional[Tuple[int, int, int, int]],
) -> None:
    try:
        import numpy as np
    except ImportError:
        return

    if cube.n_samples == 0:
        return
    u32 = np.frombuffer(cube.iq, dtype="<u4")
    i16 = u32.view(np.int16).reshape(-1, 2)
    real = i16[:, 1].copy()
    imag = i16[:, 0].copy()
    complex_data = (real.astype(np.float32) + 1j * imag.astype(np.float32)).astype(
        np.complex64
    )

    arrays: Dict[str, object] = {
        "frame_idx": np.uint32(cube.frame_idx),
    }
    if shape is not None:
        arrays["real"] = real.reshape(shape)
        arrays["imag"] = imag.reshape(shape)
        arrays["complex"] = complex_data.reshape(shape)
    else:
        arrays["real"] = real
        arrays["imag"] = imag
        arrays["complex"] = complex_data

    np.savez(npz_path, **arrays)


def write_point_cloud_rows(
    point_csv,
    state: Dict[str, int],
    stats: Dict[str, int],
    payload: bytes,
    time_base: float,
    time_step: float,
) -> None:
    for cloud in parse_point_cloud_data(payload):
        state["point_seq"] += 1
        point_num = len(cloud.records)
        time_value = time_base + cloud.frame_idx * time_step
        for idx, record in enumerate(cloud.records):
            point_csv.writerow(
                {
                    "seq": state["point_seq"],
                    "time": f"{time_value:.6f}",
                    "frame_index": cloud.frame_idx,
                    "signal_name": cloud.signal_name,
                    "dim": cloud.dim,
                    "point_num": point_num,
                    "idx": idx,
                    "x": f"{record.x:.9f}",
                    "y": f"{record.y:.9f}",
                    "z": f"{record.z:.9f}",
                    "w": f"{record.w:.9f}",
                    "u": f"{record.u:.9f}",
                    "v": f"{record.v:.9f}",
                }
            )
        stats["point_cloud_reports"] += 1
        stats["point_cloud_points"] += point_num


def write_frame_info_row(
    frame_info_csv,
    state: Dict[str, int],
    stats: Dict[str, int],
    payload: bytes,
    time_base: float,
    time_step: float,
) -> None:
    info = parse_frame_info(payload)
    if info is None:
        return
    state["frame_info_seq"] += 1
    time_value = time_base + state["frame_info_seq"] * time_step
    frame_info_csv.writerow(
        {
            "time": f"{time_value:.6f}",
            "epoch_ms": str(int(round(time_value * 1000.0))),
            "frame_index": info.frame_idx,
            "type": info.type,
            "chirp_loops": info.period_ms,
            "range_samples": info.range_mm,
            "range_fft": info.range_fft,
            "doppler_samples": info.velocity_mm,
            "doppler_fft": info.doppler_fft,
            "extra1": info.extra1,
            "antenna_count": info.antenna_count,
            "motion_points": info.motion_points,
            "presence_points": info.presence_points,
        }
    )
    stats["frame_info_reports"] += 1


def process_one_file(
    input_path: Path,
    frame_csv,
    datacube_csv,
    point_csv,
    frame_info_csv,
    state: Dict[str, int],
    fragment_asm: HifFragmentAssembler,
    piece_asm: DataPieceAssembler,
    out_dir: Path,
    message_csv,
    messages_dir: Path,
    save_messages: bool,
    shape_override: Optional[Tuple[int, int, int, int]],
    keep_bad: bool,
    save_npz_enabled: bool,
    verbose: bool,
    time_base: float,
    time_step: float,
) -> Dict[str, object]:
    input_name = str(input_path)
    stats: Dict[str, int] = {
        "file_bytes": 0,
        "valid_frames": 0,
        "bad_checksum_frames": 0,
        "c1_c2_frames": 0,
        "hif_fragmented_frames": 0,
        "message_payloads": 0,
        "data_frames_used": 0,
        "cubes_complete": 0,
        "point_cloud_reports": 0,
        "point_cloud_points": 0,
        "frame_info_reports": 0,
    }
    stats["message_counts"] = {}
    data_dir = out_dir / "datacube"

    started = time.monotonic()
    with input_path.open("rb") as stream:
        for frame in stream_hif_frames(
            stream,
            accept_bad_checksum=keep_bad,
        ):
            state["frame_seq"] += 1
            stats["valid_frames"] += 1
            if frame.checksum_ok is False:
                stats["bad_checksum_frames"] += 1

            frame_csv.writerow(frame_row(state["frame_seq"], input_name, frame))

            if frame.checksum_ok is False and not keep_bad:
                continue

            if frame.header.msg_id in (MMW_MSG_C1, MMW_MSG_C2):
                stats["c1_c2_frames"] += 1
            if frame.header.frag:
                stats["hif_fragmented_frames"] += 1

            for payload in fragment_asm.add(frame):
                stats["data_frames_used"] += 1
                stats["message_payloads"] += 1
                count_key = f"0x{frame.header.msg_id:02X}"
                stats["message_counts"][count_key] = (
                    stats["message_counts"].get(count_key, 0) + 1
                )

                if save_messages:
                    state["message_seq"] += 1
                    payload_file = (
                        messages_dir
                        / f"msg_{frame.header.msg_id:02X}_{state['message_seq']:05d}.bin"
                    )
                    payload_file.write_bytes(payload)
                    message_csv.writerow(
                        {
                            "seq": state["message_seq"],
                            "input": input_name,
                            "msg_id": f"0x{frame.header.msg_id:02X}",
                            "source_frame_seq": state["frame_seq"],
                            "frag": frame.header.frag,
                            "payload_len": len(payload),
                            "payload_file": str(payload_file),
                        }
                    )

                if frame.header.msg_id == MMW_MSG_C3:
                    write_point_cloud_rows(
                        point_csv,
                        state,
                        stats,
                        payload,
                        time_base,
                        time_step,
                    )
                    continue
                if frame.header.msg_id == MMW_MSG_C6:
                    write_frame_info_row(
                        frame_info_csv,
                        state,
                        stats,
                        payload,
                        time_base,
                        time_step,
                    )
                    continue
                if frame.header.msg_id not in (MMW_MSG_C1, MMW_MSG_C2):
                    continue

                for cube in piece_asm.add(frame.header.msg_id, payload):
                    stats["cubes_complete"] += 1
                    state["cube_seq"] += 1
                    cube_seq = state["cube_seq"]

                    shape = shape_override
                    if shape is None:
                        shape = default_cube_shape(cube.msg_id, cube.tl)
                    if shape is not None:
                        expected = 1
                        for dim in shape:
                            expected *= dim
                        if expected != cube.n_samples:
                            if verbose:
                                print(
                                    f"warning: frame {cube.frame_idx} has "
                                    f"{cube.n_samples} IQ samples, shape {shape} "
                                    f"needs {expected}; keeping flat arrays"
                                )
                            shape = None

                    base = data_dir / f"cube_c{'1' if cube.msg_id == MMW_MSG_C1 else '2'}_{cube_seq:05d}_f{cube.frame_idx:08d}"
                    raw_file = base.with_suffix(".bin")
                    iq_file = base.with_name(base.stem + ".iq.bin")
                    npz_file = None

                    raw_file.write_bytes(cube.raw)
                    iq_file.write_bytes(cube.iq)
                    if (
                        save_npz_enabled
                        and cube.n_samples > 0
                    ):
                        npz_file = base.with_name(base.stem + ".npz")
                        save_npz(cube, npz_file, shape)
                        if not npz_file.is_file():
                            npz_file = None

                    datacube_csv.writerow(
                        datacube_row(
                            state["cube_seq"],
                            input_name,
                            shape,
                            cube,
                            raw_file,
                            iq_file,
                            npz_file,
                        )
                    )

    stats["file_bytes"] = input_path.stat().st_size
    stats["elapsed_seconds"] = round(time.monotonic() - started, 3)
    stats["hif_fragments_active"] = fragment_asm.active_count
    stats["datacube_active"] = piece_asm.active_count
    stats["fragment_stats"] = fragment_asm.stats
    stats["datacube_stats"] = piece_asm.stats
    return stats


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Parse HIF raw sessions and extract C1/C2 FFT DataCube reports."
    )
    parser.add_argument("inputs", nargs="*", metavar="RAW_BIN")
    parser.add_argument("--input", nargs="+", dest="input_opt", metavar="RAW_BIN")
    parser.add_argument("--out", type=Path, default=Path("parsed"))
    parser.add_argument(
        "--shape",
        type=parse_shape,
        default=None,
        help="manual C1 shape as TX,RX,DFFT,RFFT; C2 normally uses its TL",
    )
    parser.add_argument(
        "--max-active",
        type=int,
        default=64,
        help="max concurrently open C1/C2 frames before evicting oldest",
    )
    parser.add_argument(
        "--max-fragment-active",
        type=int,
        default=64,
        help="max concurrently open HIF fragmented messages",
    )
    parser.add_argument(
        "--keep-bad",
        action="store_true",
        help="also feed frames with Check32 mismatches into reassembly",
    )
    parser.add_argument(
        "--no-npz",
        action="store_true",
        help="do not write numpy .npz files even if numpy is installed",
    )
    parser.add_argument(
        "--no-save-messages",
        action="store_true",
        help="disable generic payload files for every HIF message",
    )
    parser.add_argument(
        "--time-base",
        type=float,
        default=None,
        help="base UNIX time for generated frame/point rows; default uses input file mtime",
    )
    parser.add_argument(
        "--time-step",
        type=float,
        default=0.0,
        help="seconds added per generated frame_info row",
    )
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    files = list(args.input_opt or [])
    files.extend(args.inputs)
    if not files:
        parser.error("provide at least one raw HIF .bin file")

    input_paths = [Path(item) for item in files]
    missing = [str(item) for item in input_paths if not item.is_file()]
    if missing:
        parser.error(f"input file(s) not found: {', '.join(missing)}")

    out_dir = args.out
    data_dir = out_dir / "datacube"
    messages_dir = out_dir / "messages"
    out_dir.mkdir(parents=True, exist_ok=True)
    data_dir.mkdir(parents=True, exist_ok=True)
    messages_dir.mkdir(parents=True, exist_ok=True)

    state = {
        "frame_seq": 0,
        "cube_seq": 0,
        "message_seq": 0,
        "point_seq": 0,
        "frame_info_seq": 0,
    }
    fragment_asm = HifFragmentAssembler(
        max_active=args.max_fragment_active,
    )
    piece_asm = DataPieceAssembler(max_active=args.max_active)

    all_stats: List[Dict[str, object]] = []
    totals: Dict[str, int] = {
        "files": 0,
        "valid_frames": 0,
        "bad_checksum_frames": 0,
        "c1_c2_frames": 0,
        "message_payloads": 0,
        "cubes_complete": 0,
        "point_cloud_reports": 0,
        "point_cloud_points": 0,
        "frame_info_reports": 0,
    }

    with (out_dir / "manifest_frames.csv").open(
        "w", newline="", encoding="utf-8"
    ) as fh, (out_dir / "manifest_datacube.csv").open(
        "w", newline="", encoding="utf-8"
    ) as dh, (out_dir / "manifest_messages.csv").open(
        "w", newline="", encoding="utf-8"
    ) as mh, (out_dir / "manifest_pointcloud.csv").open(
        "w", newline="", encoding="utf-8"
    ) as ph, (out_dir / "manifest_frame_info.csv").open(
        "w", newline="", encoding="utf-8"
    ) as ih:
        frame_csv = csv.DictWriter(fh, fieldnames=FRAME_FIELDS)
        frame_csv.writeheader()
        datacube_csv = csv.DictWriter(dh, fieldnames=DATACUBE_FIELDS)
        datacube_csv.writeheader()
        message_csv = csv.DictWriter(mh, fieldnames=MESSAGE_FIELDS)
        message_csv.writeheader()
        point_csv = csv.DictWriter(ph, fieldnames=POINT_CLOUD_FIELDS)
        point_csv.writeheader()
        frame_info_csv = csv.DictWriter(ih, fieldnames=FRAME_INFO_FIELDS)
        frame_info_csv.writeheader()

        for input_path in input_paths:
            if args.verbose:
                print(f"parsing {input_path}")
            time_base = (
                args.time_base
                if args.time_base is not None
                else input_path.stat().st_mtime
            )
            stats = process_one_file(
                input_path,
                frame_csv,
                datacube_csv,
                point_csv,
                frame_info_csv,
                state,
                fragment_asm,
                piece_asm,
                out_dir,
                message_csv,
                messages_dir,
                not args.no_save_messages,
                args.shape,
                args.keep_bad,
                not args.no_npz,
                args.verbose,
                time_base,
                args.time_step,
            )
            all_stats.append({"input": str(input_path), **stats})
            totals["files"] += 1
            totals["valid_frames"] += stats["valid_frames"]
            totals["bad_checksum_frames"] += stats["bad_checksum_frames"]
            totals["c1_c2_frames"] += stats["c1_c2_frames"]
            totals["message_payloads"] += stats["message_payloads"]
            totals["cubes_complete"] += stats["cubes_complete"]
            totals["point_cloud_reports"] += stats["point_cloud_reports"]
            totals["point_cloud_points"] += stats["point_cloud_points"]
            totals["frame_info_reports"] += stats["frame_info_reports"]

    summary = {
        "tool": "parse.py",
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "shape_override": list(args.shape) if args.shape else None,
        "keep_bad_checksum": args.keep_bad,
        "save_all_messages": not args.no_save_messages,
        "inputs": all_stats,
        "totals": totals,
    }
    with (out_dir / "summary.json").open("w", encoding="utf-8") as fp:
        json.dump(summary, fp, indent=2, ensure_ascii=True)

    print(
        f"done: {totals['valid_frames']} HIF frames, "
        f"{totals['message_payloads']} message payloads, "
        f"{totals['cubes_complete']} C1/C2 cubes, "
        f"{totals['frame_info_reports']} frame infos, "
        f"{totals['point_cloud_points']} point cloud points"
    )
    print(f"output: {out_dir}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        sys.exit(130)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
