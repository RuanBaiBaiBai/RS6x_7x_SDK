#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Export saved HIF collection sessions to DebugTool-style CSV tables.

The exporter reads manifests written by AgentAuto's SPI/UART HIF collectors:

  manifest_messages.csv
  manifest_datacube.csv
  manifest_pointcloud.csv

It writes three table files with the same naming convention used by the radar
debug tool:

  <prefix>_1D_frame_info.csv
  <prefix>_motion_point_cloud.csv
  <prefix>_presence_point_cloud.csv

Point cloud rows are headerless by default and use the column-major layout
shared by the reference files:

  time, epoch_ms, x[0..N-1], y[0..N-1], z[0..N-1], v[0..N-1], snr[0..N-1]

Example:

  python agent_auto/tools/hif/export_tables.py --session captures/spi --prefix capture
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


MANIFEST_NAMES = (
    "manifest_messages.csv",
    "manifest_datacube.csv",
    "manifest_pointcloud.csv",
    "manifest_frame_info.csv",
)

POINT_MANIFEST_HEADER = (
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
)

FRAME_HEADER = (
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
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Export AgentAuto HIF session manifests to DebugTool-style CSV tables."
    )
    parser.add_argument("--session", type=Path, help="saved HIF session directory")
    parser.add_argument(
        "--input",
        nargs="*",
        default=[],
        metavar="PATH",
        help="manifest file, session directory, or wildcard path",
    )
    parser.add_argument("--prefix", default="", help="output file prefix")
    parser.add_argument("--out", type=Path, help="output directory")
    parser.add_argument("--header", action="store_true", dest="header")
    parser.add_argument("--no-header", action="store_false", dest="header")
    parser.set_defaults(header=False)
    parser.add_argument("--frame-info", action="store_true", dest="frame_info")
    parser.add_argument("--no-frame-info", action="store_false", dest="frame_info")
    parser.set_defaults(frame_info=True)
    parser.add_argument("--motion", action="store_true", dest="motion")
    parser.add_argument("--no-motion", action="store_false", dest="motion")
    parser.set_defaults(motion=True)
    parser.add_argument("--presence", action="store_true", dest="presence")
    parser.add_argument("--no-presence", action="store_false", dest="presence")
    parser.set_defaults(presence=True)
    parser.add_argument(
        "--motion-signal",
        default="motion,movement,move",
        help="comma-separated signal_name substrings for motion point clouds",
    )
    parser.add_argument(
        "--presence-signal",
        default="presence,human,occupied",
        help="comma-separated signal_name substrings for presence point clouds",
    )
    parser.add_argument(
        "--point-fields",
        default="x,y,z,w,u",
        help="comma-separated manifest fields to export per point",
    )
    parser.add_argument(
        "--point-labels",
        default="x,y,z,v,snr",
        help="comma-separated output column labels for --point-fields",
    )
    parser.add_argument("--presence-match-seconds", type=float, default=1.0)
    parser.add_argument("--frame-type", type=int, default=4)
    parser.add_argument("--chirp-loops", type=int, default=50)
    parser.add_argument("--range-samples", type=int, default=6400)
    parser.add_argument("--doppler-samples", type=int, default=1600)
    parser.add_argument("--range-fft", type=int, default=256)
    parser.add_argument("--doppler-fft", type=int, default=32)
    parser.add_argument("--antenna-count", type=int, default=4)
    parser.add_argument("--extra1", type=int, default=0)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="resolve and validate inputs without writing files",
    )
    return parser


def read_manifest(path: Path) -> List[Dict[str, str]]:
    if not path.is_file():
        return []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def resolve_session_dirs(
    session: Optional[Path], inputs: Sequence[str]
) -> List[Path]:
    if session is not None:
        session = Path(session).expanduser()
        if not session.is_dir():
            raise FileNotFoundError(f"session directory not found: {session}")
        return [session.resolve()]

    paths: List[Path] = []
    for item in inputs:
        expanded = os.path.expanduser(item)
        if not Path(expanded).exists() and any(char in expanded for char in "*?["):
            import glob

            paths.extend(Path(path) for path in glob.glob(expanded))
            continue
        path = Path(expanded)
        if not path.exists():
            raise FileNotFoundError(f"input not found: {path}")
        if path.is_dir():
            paths.append(path.resolve())
        elif path.name in MANIFEST_NAMES:
            paths.append(path.parent.resolve())
        else:
            raise ValueError(f"expected manifest file or session directory: {path}")

    unique = sorted({str(path) for path in paths})
    if not unique:
        raise ValueError("provide --session or --input")
    return [Path(item) for item in unique]


def display_time(row: Dict[str, str]) -> str:
    raw_time = str(row.get("time", "")).strip()
    if not raw_time:
        raw_time = str(row.get("time_stamp", "")).strip()
    if not raw_time:
        return ""
    try:
        seconds = float(raw_time)
        return datetime.fromtimestamp(seconds).strftime("%Y_%m_%d_%H:%M:%S:%f")[:-3]
    except ValueError:
        return raw_time.replace("T", "_").replace("-", "_").replace(".", ":")


def epoch_ms(row: Dict[str, str]) -> str:
    raw_time = str(row.get("time", "")).strip()
    if not raw_time:
        return ""
    try:
        return str(int(round(float(raw_time) * 1000.0)))
    except ValueError:
        return ""


def signal_matches(signal_name: str, patterns: str) -> bool:
    lower = signal_name.lower()
    return any(
        pattern.strip() and pattern.strip().lower() in lower
        for pattern in patterns.split(",")
    )


def group_point_rows(
    rows: List[Dict[str, str]],
) -> List[Dict[str, object]]:
    groups: Dict[tuple, List[Tuple[int, Dict[str, str]]]] = {}
    order: List[tuple] = []
    for row in rows:
        try:
            idx = int(row.get("idx", 0) or 0)
        except ValueError:
            idx = 0
        key = (
            str(row.get("time", "")),
            str(row.get("frame_index", "")),
            str(row.get("signal_name", "")),
            str(row.get("dim", "")),
        )
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append((idx, row))

    result: List[Dict[str, object]] = []
    for key in order:
        entries = groups[key]
        point_num = max((idx for idx, _ in entries), default=-1) + 1
        first = entries[0][1]
        result.append(
            {
                "time": first.get("time", ""),
                "time_stamp": first.get("time_stamp", ""),
                "frame_index": first.get("frame_index", ""),
                "signal_name": first.get("signal_name", ""),
                "dim": first.get("dim", ""),
                "point_num": point_num,
                "entries": entries,
            }
        )
    return result


def flatten_point_group(
    group: Dict[str, object],
    fields: Sequence[str],
) -> List[str]:
    point_num = int(group["point_num"])
    values: Dict[str, List[str]] = {field: [""] * point_num for field in fields}
    for idx, row in group["entries"]:  # type: ignore[assignment]
        for field in fields:
            if idx < len(values[field]):
                values[field][idx] = str(row.get(field, ""))

    row = [display_time(group), epoch_ms(group)]  # type: ignore[arg-type]
    for field in fields:
        row.extend(values[field])
    return row


def nearest_point_count(
    groups: List[Dict[str, object]],
    reference: float,
    frame_index: str,
    max_match_seconds: float,
) -> int:
    same_frame = [group for group in groups if str(group["frame_index"]) == str(frame_index)]
    candidates = same_frame or groups
    best: Optional[Dict[str, object]] = None
    best_delta: Optional[float] = None
    for group in candidates:
        try:
            seconds = float(str(group.get("time", "")))
        except ValueError:
            continue
        delta = abs(seconds - reference)
        if best_delta is None or delta < best_delta:
            best_delta = delta
            best = group
    if best is None or best_delta is None or best_delta > max_match_seconds:
        return 0
    return int(best["point_num"])


def frame_record_time(row: Dict[str, str]) -> float:
    raw_time = str(row.get("time", "")).strip()
    try:
        return float(raw_time)
    except ValueError:
        return 0.0


def frame_source_rows(session_dir: Path) -> Tuple[str, List[Dict[str, str]]]:
    frame_info = read_manifest(session_dir / "manifest_frame_info.csv")
    if frame_info:
        return "frame_info", frame_info
    datacube = read_manifest(session_dir / "manifest_datacube.csv")
    if datacube:
        return "datacube", datacube
    messages = read_manifest(session_dir / "manifest_messages.csv")
    if messages:
        return "messages", messages
    return "none", []


def frame_table_rows(
    args: argparse.Namespace,
    source_rows: List[Dict[str, str]],
    motion_groups: List[Dict[str, object]],
    presence_groups: List[Dict[str, object]],
) -> List[List[str]]:
    if not source_rows and not motion_groups:
        return []

    base_rows = source_rows
    if not base_rows:
        base_rows = [
            {
                "time": str(group.get("time", "")),
                "time_stamp": str(group.get("time_stamp", "")),
                "frame_index": str(group.get("frame_index", "")),
            }
            for group in motion_groups
        ]

    rows: List[List[str]] = []
    for row in base_rows:
        reference = frame_record_time(row)
        frame_index = str(row.get("frame_index", ""))
        motion_count = nearest_point_count(
            motion_groups,
            reference,
            frame_index,
            args.presence_match_seconds,
        )
        presence_count = nearest_point_count(
            presence_groups,
            reference,
            frame_index,
            args.presence_match_seconds,
        )
        def value_or_default(key: str, default: object) -> object:
            value = row.get(key, "")
            if value in ("", None):
                value = default
            return value

        frame_type = value_or_default("type", args.frame_type)
        chirp_loops = value_or_default("chirp_loops", args.chirp_loops)
        range_samples = value_or_default("range_samples", args.range_samples)
        range_fft = value_or_default("range_fft", args.range_fft)
        doppler_samples = value_or_default("doppler_samples", args.doppler_samples)
        doppler_fft = value_or_default("doppler_fft", args.doppler_fft)
        extra1 = value_or_default("extra1", args.extra1)
        antenna_count = value_or_default(
            "antenna_count", row.get("rx", args.antenna_count)
        )
        if antenna_count in ("", None):
            antenna_count = args.antenna_count
        rows.append(
            [
                display_time(row),
                epoch_ms(row),
                frame_index,
                str(frame_type),
                str(chirp_loops),
                str(range_samples),
                str(range_fft),
                str(doppler_samples),
                str(doppler_fft),
                str(extra1),
                str(antenna_count),
                str(motion_count),
                str(presence_count),
            ]
        )
    return rows


def point_header(labels: Sequence[str], point_num: int) -> List[str]:
    header = ["time", "epoch_ms"]
    for label in labels:
        for idx in range(point_num):
            header.append(f"{label}[{idx}]")
    return header


def write_table(
    path: Path,
    rows: Iterable[Sequence[str]],
    args: argparse.Namespace,
    header_name: Optional[str],
) -> None:
    if args.dry_run:
        print(f"plan: {path.resolve()}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        if args.header and header_name:
            writer.writerow(header_name)
        writer.writerows(rows)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    session_dirs = resolve_session_dirs(args.session, args.input)
    point_fields = [item.strip() for item in args.point_fields.split(",") if item.strip()]
    if not point_fields:
        raise ValueError("--point-fields cannot be empty")
    point_labels = [item.strip() for item in args.point_labels.split(",") if item.strip()]
    if len(point_labels) != len(point_fields):
        raise ValueError("--point-labels must have the same count as --point-fields")

    total_frames = 0
    total_motion = 0
    total_presence = 0
    outputs: List[Path] = []
    sources: List[str] = []

    for session_dir in session_dirs:
        rows = read_manifest(session_dir / "manifest_pointcloud.csv")
        groups = group_point_rows(rows)
        motion_groups = [
            group
            for group in groups
            if signal_matches(str(group["signal_name"]), args.motion_signal)
        ]
        presence_groups = [
            group
            for group in groups
            if signal_matches(str(group["signal_name"]), args.presence_signal)
        ]

        source_type, source_rows = frame_source_rows(session_dir)
        sources.append(source_type)
        frame_rows = frame_table_rows(
            args,
            source_rows if args.frame_info else [],
            motion_groups if args.motion else [],
            presence_groups if args.presence else [],
        )

        out_dir = Path(args.out).expanduser() if args.out else session_dir
        prefix = args.prefix or session_dir.name
        frame_path = out_dir / f"{prefix}_1D_frame_info.csv"
        motion_path = out_dir / f"{prefix}_motion_point_cloud.csv"
        presence_path = out_dir / f"{prefix}_presence_point_cloud.csv"

        if args.frame_info:
            write_table(frame_path, frame_rows, args, FRAME_HEADER)
            if not args.dry_run:
                outputs.append(frame_path)
            else:
                outputs.append(frame_path)
            total_frames += len(frame_rows)
        if args.motion:
            motion_rows = [flatten_point_group(group, point_fields) for group in motion_groups]
            motion_count = max(
                (int(group["point_num"]) for group in motion_groups),
                default=0,
            )
            motion_header = point_header(point_labels, motion_count)
            write_table(motion_path, motion_rows, args, motion_header)
            if motion_rows or args.dry_run:
                outputs.append(motion_path)
            total_motion += len(motion_rows)
        if args.presence:
            presence_rows = [flatten_point_group(group, point_fields) for group in presence_groups]
            presence_count = max(
                (int(group["point_num"]) for group in presence_groups),
                default=0,
            )
            presence_header = point_header(point_labels, presence_count)
            write_table(presence_path, presence_rows, args, presence_header)
            if presence_rows or args.dry_run:
                outputs.append(presence_path)
            total_presence += len(presence_rows)

    print(
        f"done: frames={total_frames} motion={total_motion} "
        f"presence={total_presence} source={','.join(dict.fromkeys(sources))}"
    )
    for path in outputs:
        print(f"output: {path}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
