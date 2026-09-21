"""Plot saved HIF DataCube and point cloud sessions into PNG/CSV reports.

Examples:
  python agent_auto/tools/hif/plot.py --session captures/spi_hif_ch347/session_20260912_164353
  python agent_auto/tools/hif/plot.py --input parsed/datacube/cube_*.npz --range-res-mm 80
  python agent_auto/tools/hif/plot.py --session captures/spi_agentauto --point-mode presence
"""

from __future__ import annotations

import argparse
import csv
import glob
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np


@dataclass
class CubeRecord:
    path: Path
    frame_index: int
    data: np.ndarray
    antenna_tx: np.ndarray
    antenna_rx: np.ndarray
    tx_num: int
    rx_num: int
    range_bins: int
    doppler_bins: int


@dataclass
class CloudRecord:
    frame_index: int
    signal_name: str
    dim: int
    points: np.ndarray


def discover_npz(
    inputs: Sequence[str],
    session: Optional[str],
) -> Tuple[List[Path], Optional[Path]]:
    raw: List[str] = []
    session_dir: Optional[Path] = None

    if session:
        session_dir = Path(session).expanduser()
        if not session_dir.is_dir():
            raise FileNotFoundError(f"session directory not found: {session_dir}")
        raw.append(str(session_dir))

    for item in inputs:
        expanded = os.path.expanduser(item)
        if any(char in expanded for char in "*?["):
            raw.extend(glob.glob(expanded))
            continue
        path = Path(expanded)
        if path.is_dir():
            raw.append(str(path))
        elif path.is_file() and path.suffix.lower() == ".npz":
            raw.append(str(path))
        else:
            raise FileNotFoundError(f"input not found: {path}")

    paths: List[Path] = []
    for item in raw:
        path = Path(item)
        if path.is_dir():
            datacube_dir = path / "datacube"
            if datacube_dir.is_dir():
                paths.extend(sorted(datacube_dir.glob("*.npz")))
            paths.extend(sorted(path.glob("*.npz")))
        elif path.is_file():
            paths.append(path)

    unique = sorted({str(path) for path in paths})
    return [Path(path) for path in unique], session_dir


def load_cube(path: Path) -> CubeRecord:
    with np.load(path, allow_pickle=False) as data:
        if "complex_cube" in data.files:
            cube = np.asarray(data["complex_cube"])
        elif "complex" in data.files:
            cube = np.asarray(data["complex"])
        elif "real" in data.files and "imag" in data.files:
            cube = np.asarray(data["real"]) + 1j * np.asarray(data["imag"])
        else:
            raise ValueError(f"{path.name}: no real/imag/complex arrays in .npz")

        if cube.ndim != 4:
            raise ValueError(
                f"{path.name}: expected 4D (TX, RX, DFFT, RFFT), got {cube.shape}"
            )

        frame_index = int(data["frame_index"]) if "frame_index" in data.files else -1
        tx_num = int(data["tx_num"]) if "tx_num" in data.files else cube.shape[0]
        rx_num = int(data["rx_num"]) if "rx_num" in data.files else cube.shape[1]
        range_bins = (
            int(data["range_fft"]) if "range_fft" in data.files else cube.shape[-1]
        )
        doppler_bins = (
            int(data["doppler_fft"]) if "doppler_fft" in data.files else cube.shape[2]
        )

        if "antenna_tx" in data.files:
            antenna_tx = np.asarray(data["antenna_tx"])
        else:
            antenna_tx = np.zeros(rx_num, dtype=np.uint8)
        if "antenna_rx" in data.files:
            antenna_rx = np.asarray(data["antenna_rx"])
        else:
            antenna_rx = np.arange(rx_num, dtype=np.uint8)

        return CubeRecord(
            path=path,
            frame_index=frame_index,
            data=cube.astype(np.complex128),
            antenna_tx=antenna_tx,
            antenna_rx=antenna_rx,
            tx_num=tx_num,
            rx_num=rx_num,
            range_bins=range_bins,
            doppler_bins=doppler_bins,
        )


def doppler_reduce(record: CubeRecord, method: str) -> np.ndarray:
    magnitude = np.abs(record.data)
    if method == "mean":
        return magnitude.mean(axis=2)
    if method == "max":
        return magnitude.max(axis=2)
    if method == "sum":
        return magnitude.sum(axis=2)
    raise ValueError(f"unsupported Doppler aggregation: {method}")


def pair_labels(record: CubeRecord) -> List[str]:
    labels: List[str] = []
    for tx_idx in range(record.tx_num):
        for rx_idx in range(record.rx_num):
            tx_id = int(record.antenna_tx[rx_idx]) if len(record.antenna_tx) else tx_idx
            rx_id = int(record.antenna_rx[rx_idx]) if len(record.antenna_rx) else rx_idx
            if record.tx_num > 1:
                labels.append(f"TX{tx_id}-RX{rx_id}")
            else:
                labels.append(f"RX{rx_id}")
    return labels


def to_db(magnitude: np.ndarray, reference: float, floor_db: float = -100.0) -> np.ndarray:
    floor = max(reference, 1e-30) * (10.0 ** (floor_db / 20.0))
    return 20.0 * np.log10(np.maximum(magnitude, floor))


def top_peak(
    spectrum: np.ndarray,
    range_m: np.ndarray,
    min_range_m: float,
) -> Dict[str, float]:
    values = np.asarray(spectrum, dtype=float)
    usable = np.flatnonzero(range_m >= min_range_m)
    if usable.size == 0:
        usable = np.arange(values.size)
    idx = int(usable[np.argmax(values[usable])])
    return {
        "peak_bin": idx,
        "peak_range_m": float(range_m[idx]),
        "peak_magnitude": float(values[idx]),
    }


def plot_average(
    average_spec: np.ndarray,
    range_m: np.ndarray,
    labels: List[str],
    scale: str,
    reference: float,
    out_path: Path,
) -> None:
    fig, ax = plt.subplots(figsize=(13, 6), dpi=150)
    flat_spec = average_spec.reshape(-1, range_m.size)
    for idx, label in enumerate(labels):
        values = flat_spec[idx]
        if scale == "db":
            values = to_db(values, reference)
        ax.plot(range_m, values, linewidth=1.6, label=label)

    ax.set_xlabel("Range (m)")
    ax.set_ylabel("Relative amplitude (dB)" if scale == "db" else "Magnitude")
    ax.set_title("Average Range Spectrum")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right", fontsize=9)
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)


def plot_per_frame(
    frame_specs: Sequence[np.ndarray],
    frame_indexes: Sequence[int],
    range_m: np.ndarray,
    labels: List[str],
    scale: str,
    reference: float,
    out_path: Path,
    max_subplots: int = 12,
) -> None:
    shown = list(frame_specs)[:max_subplots]
    shown_indexes = list(frame_indexes)[:max_subplots]
    cols = 3 if len(shown) > 3 else len(shown)
    cols = max(cols, 1)
    rows = (len(shown) + cols - 1) // cols
    fig, axes = plt.subplots(
        rows,
        cols,
        figsize=(5.0 * cols, 3.4 * rows),
        dpi=150,
        squeeze=False,
    )
    for plot_idx, (spec, frame_idx) in enumerate(zip(shown, shown_indexes)):
        ax = axes[plot_idx // cols][plot_idx % cols]
        flat_spec = spec.reshape(-1, range_m.size)
        for antenna_idx, label in enumerate(labels):
            values = flat_spec[antenna_idx]
            if scale == "db":
                values = to_db(values, reference)
            ax.plot(range_m, values, linewidth=1.1, label=label)
        ax.set_title(f"frame {frame_idx}")
        ax.set_xlabel("Range (m)")
        ax.set_ylabel("Relative amplitude (dB)" if scale == "db" else "Magnitude")
        ax.grid(True, alpha=0.3)
        if plot_idx == 0:
            ax.legend(loc="upper right", fontsize=7)
    for empty in range(len(shown), rows * cols):
        axes[empty // cols][empty % cols].axis("off")
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)


def read_cloud_csv(cloud_csv: Path) -> List[CloudRecord]:
    rows: List[Dict[str, str]] = []
    with cloud_csv.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            return []
        rows = list(reader)

    groups: Dict[Tuple[int, str, int], List[Tuple[int, Dict[str, str]]]] = {}
    for row in rows:
        try:
            frame = int(row["frame_index"])
            idx = int(row["idx"])
            dim = int(row["dim"]) if row.get("dim") else 0
        except (KeyError, ValueError):
            continue
        key = (frame, str(row.get("signal_name", "")), dim)
        groups.setdefault(key, []).append((idx, row))

    result: List[CloudRecord] = []
    for (frame, name, dim), entries in sorted(groups.items()):
        count = max((idx for idx, _ in entries), default=-1) + 1
        points = np.zeros((count, 6), dtype=np.float64)
        for idx, row in entries:
            for col, field in enumerate(("x", "y", "z", "w", "u", "v")):
                value = row.get(field, "")
                if value not in (None, ""):
                    try:
                        points[idx, col] = float(value)
                    except ValueError:
                        pass
        result.append(
            CloudRecord(
                frame_index=frame,
                signal_name=name,
                dim=dim,
                points=points,
            )
        )
    return result


def read_flat_cloud_csv(
    cloud_csv: Path,
    signal_name: str,
) -> List[CloudRecord]:
    with cloud_csv.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.reader(handle))
    result: List[CloudRecord] = []
    for row_index, row in enumerate(rows):
        n = (len(row) - 2) // 5
        if n <= 0 or len(row) < 2 + n * 5:
            continue
        values = np.asarray(row[2 : 2 + n * 5], dtype=float).reshape(5, n).T
        result.append(
            CloudRecord(
                frame_index=row_index,
                signal_name=signal_name,
                dim=3,
                points=values,
            )
        )
    return result


def read_point_clouds(session_dir: Optional[Path]) -> List[CloudRecord]:
    if session_dir is None:
        return []
    manifest = session_dir / "manifest_pointcloud.csv"
    if manifest.is_file():
        return read_cloud_csv(manifest)

    result: List[CloudRecord] = []
    for pattern, name in (
        ("*_motion_point_cloud.csv", "motion"),
        ("*_presence_point_cloud.csv", "presence"),
    ):
        for path in sorted(session_dir.glob(pattern)):
            result.extend(read_flat_cloud_csv(path, name))
    return result


def filter_clouds(
    clouds: Sequence[CloudRecord],
    point_mode: str,
) -> List[CloudRecord]:
    if point_mode == "none":
        return []
    if point_mode == "all":
        return list(clouds)
    if point_mode == "overview":
        return list(clouds)
    lower = point_mode.lower()
    return [
        cloud
        for cloud in clouds
        if lower in str(cloud.signal_name).lower()
    ]


def plot_cloud_overview(
    clouds: Sequence[CloudRecord],
    point_mode: str,
    out_path: Path,
) -> None:
    points = np.vstack([cloud.points[:, :5] for cloud in clouds]) if clouds else np.zeros((0, 5))
    fig, ax = plt.subplots(figsize=(11, 8), dpi=150)
    if points.shape[0] == 0:
        ax.text(
            0.5,
            0.5,
            f"No point cloud data ({point_mode})",
            ha="center",
            va="center",
            transform=ax.transAxes,
        )
        ax.set_axis_off()
    else:
        scatter = ax.scatter(
            points[:, 0],
            points[:, 1],
            c=points[:, 3],
            cmap="turbo",
            s=7,
            alpha=0.75,
        )
        ax.set_xlabel("X (m)")
        ax.set_ylabel("Y (m)")
        ax.set_title(f"Point Cloud Overview ({point_mode}, {points.shape[0]} points)")
        ax.grid(True, alpha=0.25)
        fig.colorbar(scatter, ax=ax, label="Velocity (m/s)")
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)


def plot_cloud_frames(
    clouds: Sequence[CloudRecord],
    point_mode: str,
    out_path: Path,
    max_subplots: int = 16,
) -> None:
    shown = list(clouds)[:max_subplots]
    if not shown:
        fig, ax = plt.subplots(figsize=(7, 4), dpi=150)
        ax.text(
            0.5,
            0.5,
            f"No point cloud frames ({point_mode})",
            ha="center",
            va="center",
            transform=ax.transAxes,
        )
        ax.set_axis_off()
        fig.tight_layout()
        fig.savefig(out_path)
        plt.close(fig)
        return
    cols = 4 if len(shown) > 4 else len(shown)
    cols = max(cols, 1)
    rows = (len(shown) + cols - 1) // cols
    fig, axes = plt.subplots(
        rows,
        cols,
        figsize=(4.0 * cols, 3.0 * rows),
        dpi=150,
        squeeze=False,
    )
    for plot_idx, cloud in enumerate(shown):
        ax = axes[plot_idx // cols][plot_idx % cols]
        pts = cloud.points
        if pts.shape[0] == 0:
            ax.text(
                0.5,
                0.5,
                "0 points",
                ha="center",
                va="center",
                transform=ax.transAxes,
            )
        else:
            ax.scatter(pts[:, 0], pts[:, 1], c=pts[:, 3], cmap="turbo", s=6, alpha=0.85)
        ax.set_title(f"{cloud.signal_name} f{cloud.frame_index} ({pts.shape[0]})")
        ax.set_xlabel("X (m)")
        ax.set_ylabel("Y (m)")
        ax.grid(True, alpha=0.25)
    for empty in range(len(shown), rows * cols):
        axes[empty // cols][empty % cols].axis("off")
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Plot saved HIF DataCube and point cloud sessions."
    )
    parser.add_argument("--session", default=None, metavar="DIR")
    parser.add_argument("--input", nargs="*", default=[], metavar="PATH")
    parser.add_argument("--out", default=None, metavar="DIR")
    parser.add_argument("--range-res-mm", type=float, default=80.0)
    parser.add_argument(
        "--doppler-agg",
        choices=("mean", "max", "sum"),
        default="mean",
    )
    parser.add_argument(
        "--scale",
        choices=("db", "linear"),
        default="db",
    )
    parser.add_argument("--max-range", type=float, default=None)
    parser.add_argument("--min-range", type=float, default=0.0)
    parser.add_argument("--max-frame-plots", type=int, default=12)
    parser.add_argument(
        "--point-mode",
        choices=("none", "overview", "motion", "presence", "all"),
        default="overview",
    )
    parser.add_argument("--max-point-frames", type=int, default=60)
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    npz_paths, session_dir = discover_npz(args.input, args.session)
    clouds = read_point_clouds(session_dir)
    selected_clouds = filter_clouds(clouds, args.point_mode)

    if not npz_paths and not selected_clouds:
        raise FileNotFoundError("no .npz DataCube files or point cloud manifests found")

    if args.out:
        out_dir = Path(args.out).expanduser()
    elif session_dir is not None:
        out_dir = session_dir / "plots"
    elif npz_paths:
        first = npz_paths[0]
        out_dir = first.parent.parent / "plots" if first.parent.name == "datacube" else first.parent / "plots"
    else:
        out_dir = Path("plots")
    out_dir.mkdir(parents=True, exist_ok=True)

    summary: Dict[str, object] = {
        "npz_count": len(npz_paths),
        "npz_inputs": [str(path) for path in npz_paths],
        "point_cloud_frames": len(selected_clouds),
        "plots": {},
    }

    if npz_paths:
        records = [load_cube(path) for path in npz_paths]
        shape = records[0].data.shape
        if any(record.data.shape != shape for record in records):
            raise ValueError("all .npz files must have the same DataCube shape")

        range_res_m = args.range_res_mm / 1000.0
        record = records[0]
        range_m = np.arange(record.range_bins, dtype=float) * range_res_m
        mask = np.ones(record.range_bins, dtype=bool)
        if args.max_range is not None:
            mask &= range_m <= args.max_range
        mask &= range_m >= args.min_range
        if not np.any(mask):
            raise ValueError("--min-range/--max-range removed all range bins")

        frame_specs = [doppler_reduce(item, args.doppler_agg) for item in records]
        stack = np.stack(frame_specs, axis=0)
        average_spec = stack.mean(axis=0)
        reference = float(np.max(stack) if stack.size else 1.0)
        labels = pair_labels(record)

        avg_plot = out_dir / f"range_spectrum_average_{args.doppler_agg}_{args.scale}.png"
        plot_average(
            average_spec[:, :, mask],
            range_m[mask],
            labels,
            args.scale,
            reference,
            avg_plot,
        )
        per_frame_plot = (
            out_dir / f"range_spectrum_per_frame_{args.doppler_agg}_{args.scale}.png"
        )
        plot_per_frame(
            [spec[:, :, mask] for spec in frame_specs],
            [record.frame_index for record in records],
            range_m[mask],
            labels,
            args.scale,
            reference,
            per_frame_plot,
            args.max_frame_plots,
        )

        csv_path = out_dir / f"range_spectrum_average_{args.doppler_agg}.csv"
        with csv_path.open("w", newline="", encoding="utf-8") as fh:
            writer = csv.writer(fh)
            writer.writerow(["range_m"] + labels)
            spectrum_slice = average_spec[:, :, mask]
            for bin_idx, distance in enumerate(range_m[mask]):
                writer.writerow(
                    [f"{distance:.4f}"]
                    + [
                        f"{spectrum_slice[tx, rx, bin_idx]:.8e}"
                        for tx in range(record.tx_num)
                        for rx in range(record.rx_num)
                    ]
                )

        stats: Dict[str, object] = {
            "input_count": len(records),
            "inputs": [str(path) for path in npz_paths],
            "shape": list(shape),
            "range_resolution_m": range_res_m,
            "doppler_aggregation": args.doppler_agg,
            "scale": args.scale,
            "antennas": {},
            "plots": {
                "average": str(avg_plot),
                "per_frame": str(per_frame_plot),
                "csv": str(csv_path),
            },
        }
        for pair_idx, label in enumerate(labels):
            tx = pair_idx // record.rx_num
            rx = pair_idx % record.rx_num
            spec = average_spec[tx, rx, mask]
            peak = top_peak(spec, range_m[mask], args.min_range)
            peak["relative_db"] = float(to_db(np.array([peak["peak_magnitude"]]), reference)[0])
            stats["antennas"][label] = peak

        stats_path = out_dir / f"range_spectrum_stats_{args.doppler_agg}.json"
        with stats_path.open("w", encoding="utf-8") as fh:
            json.dump(stats, fh, indent=2)
        summary["range_spectrum"] = stats
        summary["plots"]["range_spectrum_average"] = str(avg_plot)
        summary["plots"]["range_spectrum_per_frame"] = str(per_frame_plot)
        summary["plots"]["range_spectrum_csv"] = str(csv_path)

    if args.point_mode != "none":
        mode_name = "overview" if args.point_mode == "overview" else args.point_mode
        cloud_overview = out_dir / f"point_cloud_{mode_name}_overview.png"
        plot_cloud_overview(selected_clouds, args.point_mode, cloud_overview)
        summary["plots"]["point_cloud_overview"] = str(cloud_overview)

        cloud_frames = out_dir / f"point_cloud_{mode_name}_frames.png"
        plot_cloud_frames(
            selected_clouds[: args.max_point_frames],
            args.point_mode,
            cloud_frames,
            min(args.max_point_frames, 24),
        )
        summary["plots"]["point_cloud_frames"] = str(cloud_frames)

    summary_path = out_dir / "plot_summary.json"
    with summary_path.open("w", encoding="utf-8") as fh:
        json.dump(summary, fh, indent=2)

    print(
        f"done: npz={len(npz_paths)} point_cloud_frames={len(selected_clouds)} "
        f"point_mode={args.point_mode}"
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
