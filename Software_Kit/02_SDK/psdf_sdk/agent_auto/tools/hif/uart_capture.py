"""Save raw HIF UART/COM traffic to timestamped .bin sessions.

Modes:
  python agent_auto/tools/hif/uart_capture.py --port COM3 --baud 921600 --out raw_data
  python agent_auto/tools/hif/uart_capture.py --source existing.bin --out raw_data

The COM mode needs pyserial. The file-copy mode only uses the standard library.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional

try:
    import serial as pyserial
except ImportError:
    pyserial = None


def parse_size(text: str) -> int:
    text = text.strip().lower()
    if text.isdigit():
        return int(text)
    unit = 1
    if text.endswith("k"):
        unit = 1024
        text = text[:-1]
    elif text.endswith("m"):
        unit = 1024 * 1024
        text = text[:-1]
    elif text.endswith("g"):
        unit = 1024 * 1024 * 1024
        text = text[:-1]
    if not text.isdigit():
        raise argparse.ArgumentTypeError("size must be a number with optional k/m/g suffix")
    return int(text) * unit


def session_name() -> str:
    return datetime.now().strftime("raw_%Y%m%d_%H%M%S")


def write_info(out_dir: Path, name: str, info: Dict[str, object]) -> Path:
    info_path = out_dir / f"{name}_info.json"
    with info_path.open("w", encoding="utf-8") as fp:
        json.dump(info, fp, indent=2, ensure_ascii=True)
    return info_path


def progress_line(written: int, elapsed: float, quiet: bool) -> None:
    if quiet:
        return
    rate = written / elapsed if elapsed > 0 else 0.0
    print(
        f"captured {written} bytes, {rate / 1024.0:.1f} KiB/s, "
        f"elapsed {elapsed:.1f}s",
        flush=True,
    )


def copy_source_file(
    source: Path,
    out_dir: Path,
    max_bytes: int,
    quiet: bool,
) -> Path:
    if not source.is_file():
        raise FileNotFoundError(f"source file not found: {source}")

    out_dir.mkdir(parents=True, exist_ok=True)
    name = session_name()
    dest = out_dir / f"{name}.bin"
    started = datetime.now().astimezone()
    started_mono = time.monotonic()
    written = 0
    sha = hashlib.sha256()

    with source.open("rb") as src, dest.open("wb") as dst:
        while True:
            chunk = src.read(1024 * 1024)
            if not chunk:
                break
            if max_bytes > 0:
                remain = max_bytes - written
                if remain <= 0:
                    break
                chunk = chunk[:remain]
            dst.write(chunk)
            sha.update(chunk)
            written += len(chunk)

    ended = datetime.now().astimezone()
    info = {
        "tool": "uart_capture.py",
        "mode": "file_copy",
        "source": str(source),
        "started_at": started.isoformat(timespec="seconds"),
        "ended_at": ended.isoformat(timespec="seconds"),
        "bytes_written": written,
        "max_bytes": max_bytes,
        "sha256": sha.hexdigest(),
    }
    write_info(out_dir, name, info)
    progress_line(written, time.monotonic() - started_mono, quiet)
    print(f"saved: {dest}")
    return dest


def capture_serial(
    port: str,
    baud: int,
    out_dir: Path,
    max_bytes: int,
    max_time: float,
    max_idle: float,
    chunk_size: int,
    quiet: bool,
) -> Path:
    if pyserial is None:
        raise RuntimeError(
            "pyserial is not installed; use: python -m pip install pyserial"
        )

    out_dir.mkdir(parents=True, exist_ok=True)
    name = session_name()
    dest = out_dir / f"{name}.bin"

    started = datetime.now().astimezone()
    started_mono = time.monotonic()
    last_progress_mono = started_mono
    written = 0
    idle_seconds = 0.0
    last_idle_check = started_mono

    try:
        ser = pyserial.Serial(port=port, baudrate=baud, timeout=0.2)
    except Exception as exc:
        raise RuntimeError(f"failed to open {port}: {exc}") from exc

    try:
        ser.reset_input_buffer()
        with dest.open("wb") as dst:
            while True:
                now = time.monotonic()
                if max_time > 0 and now - started_mono >= max_time:
                    break
                if max_bytes > 0 and written >= max_bytes:
                    break
                if max_idle > 0 and idle_seconds >= max_idle:
                    break

                data = ser.read(chunk_size)
                if data:
                    dst.write(data)
                    dst.flush()
                    written += len(data)
                    idle_seconds = 0.0
                else:
                    idle_seconds += now - last_idle_check

                last_idle_check = now
                if (
                    not quiet
                    and (written - 0) >= 0
                    and now - last_progress_mono >= 5.0
                ):
                    progress_line(written, now - started_mono, quiet)
                    last_progress_mono = now
    except KeyboardInterrupt:
        if not quiet:
            print("")
            print("capture stopped by user")
    finally:
        ser.close()

    ended = datetime.now().astimezone()
    info = {
        "tool": "uart_capture.py",
        "mode": "serial",
        "port": port,
        "baud": baud,
        "started_at": started.isoformat(timespec="seconds"),
        "ended_at": ended.isoformat(timespec="seconds"),
        "bytes_written": written,
        "max_bytes": max_bytes,
        "max_time_seconds": max_time,
        "max_idle_seconds": max_idle,
    }
    write_info(out_dir, name, info)
    progress_line(written, time.monotonic() - started_mono, quiet)
    print(f"saved: {dest}")
    return dest


def list_ports() -> None:
    if pyserial is None:
        print("pyserial is not installed; use: python -m pip install pyserial")
        return
    ports = list(pyserial.tools.list_ports.comports())
    if not ports:
        print("no serial ports found")
        return
    for item in ports:
        print(item.device, item.description or "")


def main(argv: Optional[list] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Save HIF data received over UART/COM or copy an existing raw bin."
    )
    parser.add_argument("--port", help="serial port, e.g. COM3 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--source", type=Path, help="copy an existing raw HIF .bin file")
    parser.add_argument("--out", type=Path, default=Path("raw_data"))
    parser.add_argument(
        "--max-bytes",
        type=parse_size,
        default=0,
        help="stop after this many bytes, 0 means unlimited; supports k/m/g suffix",
    )
    parser.add_argument(
        "--max-time",
        type=float,
        default=0,
        help="stop after this many seconds, 0 means unlimited",
    )
    parser.add_argument(
        "--max-idle",
        type=float,
        default=0,
        help="stop after this many seconds without bytes, 0 disables the check",
    )
    parser.add_argument("--chunk-size", type=int, default=65536)
    parser.add_argument("--quiet", action="store_true")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args(argv)

    if args.list_ports:
        list_ports()
        return 0
    if args.source is not None and args.port is not None:
        parser.error("choose either --source or --port, not both")
    if args.source is None and args.port is None:
        parser.error("provide --port for live capture or --source FILE to copy raw data")

    if args.source is not None:
        copy_source_file(args.source, args.out, args.max_bytes, args.quiet)
    else:
        capture_serial(
            args.port,
            args.baud,
            args.out,
            args.max_bytes,
            args.max_time,
            args.max_idle,
            args.chunk_size,
            args.quiet,
        )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
