#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Collect HIF messages from an RS6x/7x radar over the CH347T SPI bridge.

The tool uses HifMsgDataCollectionLib instead of calling CH347DLL directly:

  OpenSpiDevice(index, freqMode) -> StartCollectingData() -> callback

The callback payload is saved to disk. C1 and C2 DataCube reports are also
converted through the DLL and exported as NumPy .npz files when possible.

Examples:

  python agent_auto/tools/hif/spi_collect.py --check
  python agent_auto/tools/hif/spi_collect.py --duration 30 --out captures/spi
  python agent_auto/tools/hif/spi_collect.py --freq-mode 1 --tx 2 --rx 4 --rfft 256 --dfft 32
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import queue
import signal
import struct
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from hif_parser import parse_frame_info  # noqa: E402

DATA_TYPE_NAMES = {
    0: "psic_debug",
    1: "string",
    2: "datacube_0xC1",
    3: "datacube_0xC2",
    4: "point_cloud",
}

FREQ_MODE_LABELS = {
    0: "60 MHz",
    1: "30 MHz",
    2: "15 MHz",
    3: "7.5 MHz",
    4: "3.75 MHz",
    5: "1.875 MHz",
    6: "937.5 kHz",
    7: "468.75 kHz",
}

ERROR_CODES = {
    -8: "data/file empty",
    -7: "configuration failed",
    -6: "start radar failed",
    -5: "stop radar failed",
    -4: "wakeup failed",
    -3: "ack timeout",
    -2: "ack error/checksum",
    -1: "collection not stopped",
    0: "success",
    1: "command not supported",
    5: "invalid parameter",
    9: "device busy",
}

MESSAGE_FIELDS = [
    "seq",
    "time",
    "time_stamp",
    "data_type",
    "type_name",
    "frame_index",
    "frame_len",
    "payload_len",
    "payload_file",
    "extra_file",
    "status",
]

DATACUBE_FIELDS = [
    "seq",
    "time",
    "data_type",
    "type_name",
    "frame_index",
    "frame_len",
    "tx",
    "rx",
    "range_fft",
    "doppler_fft",
    "antenna_count",
    "samples_per_antenna",
    "payload_file",
    "iq_file",
    "npz_file",
    "status",
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



@dataclass
class LiveCubeEvent:
    """Plain Python snapshot of one converted C1/C2 DataCube."""

    seq: int
    received_at: float
    frame_index: int
    data_type: int
    type_name: str
    tx_num: int
    rx_num: int
    range_fft: int
    doppler_fft: int
    antenna_tx: List[int]
    antenna_rx: List[int]
    real: List[List[float]]
    imag: List[List[float]]


@dataclass
class LivePointCloudEvent:
    """Plain Python snapshot of one converted point cloud report."""

    seq: int
    received_at: float
    frame_index: int
    signal_name: str
    dim: int
    point_num: int
    x: List[float]
    y: List[float]
    z: List[float]
    w: List[float]
    u: List[float]
    v: List[float]


def default_dll_path() -> Path:
    """Locate the bundled HifMsgDataCollectionLib DLL for this SDK layout."""
    env = os.environ.get("HIF_DLL")
    if env:
        return Path(env)

    # AgentAuto copies this script under tools/hif. Check the original SDK
    # tool folder, the caller's working directory, and the local script folder.
    tool_roots = []
    if len(SCRIPT_DIR.parents) > 4:
        tool_roots.append(
            SCRIPT_DIR.parents[4] / "03_Tool" / "HifMsgDataCollectionLib"
        )
    tool_roots.append(Path.cwd() / "03_Tool" / "HifMsgDataCollectionLib")
    tool_roots.append(SCRIPT_DIR)
    for root in tool_roots:
        if not root.is_dir():
            continue
        candidates = sorted(root.rglob("HifMsgDataCollectionLib*.dll"))
        if candidates:
            return candidates[-1]
    return SCRIPT_DIR / "HifMsgDataCollectionLib.dll"


def load_api(dll_path: Path) -> Tuple[Any, Any]:
    """Load the .NET assembly and return (HifMsgDataCollectionApi, module)."""
    import clr

    dll_path = Path(dll_path).resolve()
    if not dll_path.is_file():
        raise FileNotFoundError(f"Hif DLL not found: {dll_path}")

    dll_dir = str(dll_path.parent)
    if dll_dir not in sys.path:
        sys.path.insert(0, dll_dir)

    clr.AddReference(str(dll_path))
    import HifMsgDataCollectionLib as lib

    return lib.HifMsgDataCollectionApi(), lib


def list_ch347_devices() -> str:
    """Return present CH347 USB devices using Windows PnP."""
    ps = (
        "$ErrorActionPreference='SilentlyContinue'; "
        "Get-CimInstance Win32_PnPEntity | "
        "Where-Object { $_.PNPDeviceID -like 'USB\\VID_1A86&PID_55DB*' -and "
        "($_.Name -match 'CH347|SPI|I2C') } | "
        "Select-Object Status,Name,PNPDeviceID | "
        "Format-List | Out-String -Width 240"
    )
    try:
        proc = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", ps],
            capture_output=True,
            text=True,
            encoding="gbk",
            errors="replace",
            timeout=15,
        )
        return proc.stdout.strip()
    except Exception as exc:
        return f"<failed to query PnP devices: {exc}>"


def list_known_host_tools() -> str:
    tool_names = {
        "RadarDebugTool_EN_V1.0.0.9.exe",
        "RadarAnalysisTool.exe",
        "RadarKnow-CS.exe",
        "sscom5.13.1.exe",
    }
    try:
        proc = subprocess.run(
            ["tasklist", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            encoding="gbk",
            errors="replace",
            timeout=15,
        )
        found = []
        for line in proc.stdout.splitlines():
            parts = line.split('","')
            if len(parts) >= 1:
                name = parts[0].strip('"').lower()
                if any(name == item.lower() for item in tool_names):
                    found.append(parts[0].strip('"'))
        return ", ".join(sorted(set(found))) or "none"
    except Exception:
        return "unknown"


def to_bytes(value: Any) -> Optional[bytes]:
    if value is None:
        return None
    if isinstance(value, bytes):
        return value
    if isinstance(value, bytearray):
        return bytes(value)
    try:
        return bytes(value)
    except Exception:
        return str(value).encode("utf-8", "replace")


def to_u32_bytes(value: Any) -> Optional[bytes]:
    if value is None:
        return None
    out = bytearray()
    try:
        iterator = iter(value)
    except TypeError:
        return None
    for item in iterator:
        out += struct.pack("<I", int(item) & 0xFFFFFFFF)
    return bytes(out)


def make_u32_array(values: Any) -> Any:
    from System import Array, UInt32

    return Array[UInt32]([int(v) & 0xFFFFFFFF for v in values])


def parse_hex(text: str) -> Optional[bytes]:
    parts = text.replace(",", " ").replace("0x", "").split()
    if not parts:
        return None
    try:
        return bytes(int(item, 16) for item in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid hex: {exc}") from exc


class Collector:
    """Thread-safe SPI HIF collector backed by HifMsgDataCollectionLib."""

    def __init__(
        self,
        api: Any,
        index: int,
        freq_mode: int,
        out_root: Path,
        tx: int = 1,
        rx: int = 3,
        rfft: int = 256,
        dfft: int = 32,
        save_payload: bool = True,
        save_npz: bool = True,
        save_point_cloud: bool = True,
        save_cube_files: bool = True,
        save_manifest: bool = True,
        verbose: bool = False,
        on_datacube: Optional[Callable[[LiveCubeEvent], None]] = None,
        on_point_cloud: Optional[Callable[[LivePointCloudEvent], None]] = None,
    ) -> None:
        self.api = api
        self.index = index
        self.freq_mode = freq_mode
        self.out_root = Path(out_root)
        self.tx = tx
        self.rx = rx
        self.rfft = rfft
        self.dfft = dfft
        self.save_payload = save_payload
        self.save_npz = save_npz
        self.save_point_cloud = save_point_cloud
        self.save_cube_files = save_cube_files
        self.save_manifest = save_manifest
        self.verbose = verbose
        self.on_datacube = on_datacube
        self.on_point_cloud = on_point_cloud

        self.queue: "queue.Queue[Tuple[float, Any]]" = queue.Queue(maxsize=2048)
        self.stop_event = threading.Event()
        self.lock = threading.Lock()
        self.worker: Optional[threading.Thread] = None
        self.started = False
        self.stopped = False

        self.seq = 0
        self.cube_seq = 0
        self.message_count = 0
        self.cube_count = 0
        self.point_cloud_count = 0
        self.frame_info_count = 0
        self.total_bytes = 0
        self.error_count = 0

        self.session_dir: Optional[Path] = None
        self.msg_file = None
        self.cube_file = None
        self.point_file = None
        self.frame_info_file = None
        self._msg_csv = None
        self._cube_csv = None
        self._point_csv = None
        self._frame_info_csv = None
        self._info = {}

        self.api.callback += self._on_event

    def _on_event(self, sender: Any, receive_data: Any) -> None:
        try:
            self.queue.put_nowait((time.time(), receive_data))
        except queue.Full:
            with self.lock:
                self.error_count += 1
            print("WARNING: event queue full, dropped one event", flush=True)

    def start_session(self) -> Path:
        if not (
            self.save_manifest
            or self.save_payload
            or self.save_cube_files
            or self.save_npz
            or self.save_point_cloud
        ):
            self.started = True
            self.worker = threading.Thread(target=self._worker_loop, daemon=True)
            self.worker.start()
            return Path("")

        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.session_dir = self.out_root / f"session_{stamp}"
        raw_dir = self.session_dir / "raw_payload"
        cube_dir = self.session_dir / "datacube"
        point_dir = self.session_dir / "pointcloud"
        for path in (raw_dir, cube_dir, point_dir):
            path.mkdir(parents=True, exist_ok=True)

        if self.save_manifest:
            self.msg_file = open(self.session_dir / "manifest_messages.csv", "w", newline="", encoding="utf-8")
            self.cube_file = open(self.session_dir / "manifest_datacube.csv", "w", newline="", encoding="utf-8")
            self.point_file = open(self.session_dir / "manifest_pointcloud.csv", "w", newline="", encoding="utf-8")
            self.frame_info_file = open(
                self.session_dir / "manifest_frame_info.csv",
                "w",
                newline="",
                encoding="utf-8",
            )

        if self.msg_file is not None:
            self._msg_csv = csv.DictWriter(self.msg_file, fieldnames=MESSAGE_FIELDS)
            self._msg_csv.writeheader()
        if self.cube_file is not None:
            self._cube_csv = csv.DictWriter(self.cube_file, fieldnames=DATACUBE_FIELDS)
            self._cube_csv.writeheader()
        if self.point_file is not None:
            self._point_csv = csv.DictWriter(self.point_file, fieldnames=POINT_CLOUD_FIELDS)
            self._point_csv.writeheader()
        if self.frame_info_file is not None:
            self._frame_info_csv = csv.DictWriter(
                self.frame_info_file,
                fieldnames=FRAME_INFO_FIELDS,
            )
            self._frame_info_csv.writeheader()

        self._info = {
            "started_at": datetime.now().isoformat(timespec="seconds"),
            "spi_index": self.index,
            "spi_freq_mode": self.freq_mode,
            "spi_freq_mode_label": FREQ_MODE_LABELS.get(self.freq_mode, "unknown"),
            "c1_shape": [self.tx, self.rx, self.dfft, self.rfft],
        }
        if self.save_manifest:
            self._write_info()

        self.worker = threading.Thread(target=self._worker_loop, daemon=True)
        self.worker.start()
        self.started = True
        print(f"session: {self.session_dir}", flush=True)
        return self.session_dir

    def _write_info(self) -> None:
        if self.session_dir is None:
            return
        info = dict(self._info)
        info["ended_at"] = datetime.now().isoformat(timespec="seconds")
        info.update(
            {
                "messages": self.message_count,
                "cubes": self.cube_count,
                "point_clouds": self.point_cloud_count,
                "frame_infos": self.frame_info_count,
                "total_payload_bytes": self.total_bytes,
                "errors": self.error_count,
            }
        )
        with (self.session_dir / "session_info.json").open("w", encoding="utf-8") as fp:
            json.dump(info, fp, indent=2, ensure_ascii=True)

    def _worker_loop(self) -> None:
        while not self.stop_event.is_set():
            try:
                received_at, data = self.queue.get(timeout=0.2)
            except queue.Empty:
                continue
            try:
                self._handle_event(received_at, data)
            except Exception as exc:
                with self.lock:
                    self.error_count += 1
                print(f"ERROR while handling event: {exc}", flush=True)
            finally:
                self.queue.task_done()

        # Drain events already delivered by the DLL.
        while True:
            try:
                received_at, data = self.queue.get_nowait()
            except queue.Empty:
                break
            try:
                self._handle_event(received_at, data)
            except Exception as exc:
                with self.lock:
                    self.error_count += 1
                print(f"ERROR while draining event: {exc}", flush=True)

    def _progress(self, show: bool = False) -> None:
        if self.verbose or show or self.seq % 20 == 0:
            print(
                f"events={self.seq} payload_bytes={self.total_bytes} "
                f"cubes={self.cube_count} point_clouds={self.point_cloud_count} "
                f"frame_infos={self.frame_info_count} "
                f"errors={self.error_count}",
                flush=True,
            )

    def _handle_event(self, received_at: float, data: Any) -> None:
        with self.lock:
            self.seq += 1
            seq = self.seq

        data_type = int(data.dataType)
        type_name = DATA_TYPE_NAMES.get(data_type, f"unknown_{data_type}")
        frame_index = int(data.frameIndex)
        frame_len = int(data.frameLen)
        time_stamp = str(data.timeStamp)

        payload = to_bytes(getattr(data, "payloadData", None))
        if payload is None:
            payload = to_bytes(getattr(data, "pointCloudRawDataOrStringData", None))

        payload_file = ""
        extra_file = ""
        status = "saved"

        if self.save_payload and payload is not None:
            name = f"msg_{seq:06d}_t{data_type}_f{frame_index:08d}.bin"
            payload_file = str(self.session_dir / "raw_payload" / name)
            Path(payload_file).write_bytes(payload)

        if data_type == 1 and payload is not None and self.session_dir is not None:
            text = self._decode_text(payload)
            if text is not None:
                name = f"string_{seq:06d}_f{frame_index:08d}.txt"
                text_file = self.session_dir / "raw_payload" / name
                text_file.write_text(text, encoding="utf-8", errors="replace")
                extra_file = str(text_file)

        if data_type in (2, 3):
            status = self._handle_datacube(seq, received_at, data, data_type, payload, payload_file)
        elif data_type == 4:
            self._handle_point_cloud(seq, received_at, data, payload)
        elif data_type == 0:
            self._handle_frame_info(received_at, payload)

        with self.lock:
            self.message_count += 1
            if payload is not None:
                self.total_bytes += len(payload)

        if self._msg_csv is not None:
            self._msg_csv.writerow(
                {
                    "seq": seq,
                    "time": received_at,
                    "time_stamp": time_stamp,
                    "data_type": data_type,
                    "type_name": type_name,
                    "frame_index": frame_index,
                    "frame_len": frame_len,
                    "payload_len": len(payload) if payload is not None else 0,
                    "payload_file": payload_file,
                    "extra_file": extra_file,
                    "status": status,
                }
            )
        self._progress()

    def _decode_text(self, payload: bytes) -> Optional[str]:
        for encoding in ("utf-8", "gbk", "ascii"):
            try:
                return payload.decode(encoding).strip("\x00\r\n ")
            except UnicodeDecodeError:
                continue
        return None

    def _handle_frame_info(
        self,
        received_at: float,
        payload: Optional[bytes],
    ) -> None:
        if payload is None:
            return
        info = parse_frame_info(payload)
        if info is None:
            return
        with self.lock:
            self.frame_info_count += 1
        if self._frame_info_csv is not None:
            time_value = received_at
            self._frame_info_csv.writerow(
                {
                    "time": time_value,
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

    def _handle_datacube(
        self,
        seq: int,
        received_at: float,
        data: Any,
        data_type: int,
        payload: Optional[bytes],
        payload_file: str,
    ) -> str:
        cube = None
        error = ""
        try:
            if data_type == 2:
                source = getattr(data, "datacubeRawData", None)
                if source is None and payload is not None:
                    if len(payload) % 4:
                        error = "C1 payload length is not a multiple of 4"
                    else:
                        values = [
                            int.from_bytes(payload[i : i + 4], "little")
                            for i in range(0, len(payload), 4)
                        ]
                        source = make_u32_array(values)
                if source is None:
                    error = "missing C1 datacubeRawData"
                else:
                    cube = self.api.DatacubeConversion(
                        source,
                        int(data.frameIndex),
                        int(data.frameLen),
                        int(self.rfft),
                        int(self.dfft),
                        int(self.tx),
                        int(self.rx),
                    )
            else:
                if payload is None:
                    error = "missing C2 payloadData"
                else:
                    cube = self.api.DatacubeConversion(
                        payload,
                        int(data.frameIndex),
                        int(data.frameLen),
                    )
        except Exception as exc:
            error = f"{type(exc).__name__}: {exc}"

        if cube is None:
            with self.lock:
                self.error_count += 1
            return "conversion_error" if error else "unsupported"

        with self.lock:
            self.cube_seq += 1
            cube_seq = self.cube_seq
            self.cube_count += 1

        ants = list(cube.antennaData) if cube.antennaData is not None else []
        if self.on_datacube is not None:
            self._emit_datacube(seq, received_at, data, data_type, cube, ants)

        prefix = "c1" if data_type == 2 else "c2"
        base = None
        if self.session_dir is not None and (self.save_cube_files or self.save_npz):
            base = self.session_dir / "datacube" / f"cube_{prefix}_{cube_seq:06d}_f{int(data.frameIndex):08d}"
        raw_file = ""
        iq_file = ""
        if base is not None and self.save_cube_files and payload is not None:
            raw_file = base.with_suffix(".bin")
            raw_file.write_bytes(payload)
        raw_u32 = to_u32_bytes(getattr(data, "datacubeRawData", None))
        if base is not None and self.save_cube_files and raw_u32:
            iq_file = base.with_name(base.stem + ".iq.bin")
            iq_file.write_bytes(raw_u32)
        if payload_file:
            extra_file = payload_file
        else:
            extra_file = str(raw_file)

        npz_file = ""
        if base is not None and self.save_npz:
            try:
                npz_file = self._save_datacube_npz(base, cube, ants, data_type, frame_index=int(data.frameIndex))
            except Exception as exc:
                print(f"WARNING: npz save failed for frame {int(data.frameIndex)}: {exc}", flush=True)
                npz_file = ""

        if self._cube_csv is not None:
            self._cube_csv.writerow(
                {
                    "seq": cube_seq,
                    "time": received_at,
                    "data_type": data_type,
                    "type_name": DATA_TYPE_NAMES.get(data_type, str(data_type)),
                    "frame_index": int(data.frameIndex),
                    "frame_len": int(data.frameLen),
                    "tx": int(cube.txAntNum),
                    "rx": int(cube.rxAntNum),
                    "range_fft": int(cube.rangeFftPointNum),
                    "doppler_fft": int(cube.dopFftPointNum),
                    "antenna_count": len(ants),
                    "samples_per_antenna": len(ants[0].real) if ants else 0,
                    "payload_file": payload_file,
                    "iq_file": iq_file,
                    "npz_file": npz_file,
                    "status": "ok",
                }
            )
        print(
            f"cube frame={int(data.frameIndex)} type={DATA_TYPE_NAMES.get(data_type)} "
            f"tx={int(cube.txAntNum)} rx={int(cube.rxAntNum)} "
            f"rfft={int(cube.rangeFftPointNum)} dfft={int(cube.dopFftPointNum)} "
            f"antennas={len(ants)}",
            flush=True,
        )
        return "ok"

    def _emit_datacube(
        self,
        seq: int,
        received_at: float,
        data: Any,
        data_type: int,
        cube: Any,
        ants: List[Any],
    ) -> None:
        tx_ids: List[int] = []
        rx_ids: List[int] = []
        real: List[List[float]] = []
        imag: List[List[float]] = []
        for ant in ants:
            tx_ids.append(int(ant.txAntId))
            rx_ids.append(int(ant.rxAntId))
            real.append([float(value) for value in ant.real] if ant.real is not None else [])
            imag.append([float(value) for value in ant.imag] if ant.imag is not None else [])

        event = LiveCubeEvent(
            seq=seq,
            received_at=received_at,
            frame_index=int(data.frameIndex),
            data_type=data_type,
            type_name=DATA_TYPE_NAMES.get(data_type, str(data_type)),
            tx_num=int(cube.txAntNum),
            rx_num=int(cube.rxAntNum),
            range_fft=int(cube.rangeFftPointNum),
            doppler_fft=int(cube.dopFftPointNum),
            antenna_tx=tx_ids,
            antenna_rx=rx_ids,
            real=real,
            imag=imag,
        )
        try:
            self.on_datacube(event)  # type: ignore[misc]
        except Exception as exc:
            print(f"WARNING: live datacube callback failed: {exc}", flush=True)

    def _save_datacube_npz(
        self,
        base: Path,
        cube: Any,
        ants: List[Any],
        data_type: int,
        frame_index: int,
    ) -> str:
        import numpy as np

        real_list: List[Any] = []
        imag_list: List[Any] = []
        tx_ids: List[int] = []
        rx_ids: List[int] = []
        for ant in ants:
            tx_ids.append(int(ant.txAntId))
            rx_ids.append(int(ant.rxAntId))
            real_list.append(list(ant.real) if ant.real is not None else [])
            imag_list.append(list(ant.imag) if ant.imag is not None else [])

        arrays: Dict[str, Any] = {
            "frame_index": np.uint32(frame_index),
            "data_type": np.uint8(data_type),
            "tx_num": np.uint8(int(cube.txAntNum)),
            "rx_num": np.uint8(int(cube.rxAntNum)),
            "range_fft": np.uint16(int(cube.rangeFftPointNum)),
            "doppler_fft": np.uint16(int(cube.dopFftPointNum)),
            "antenna_tx": np.asarray(tx_ids, dtype=np.uint8),
            "antenna_rx": np.asarray(rx_ids, dtype=np.uint8),
        }

        if ants:
            real = np.asarray(real_list, dtype=np.float64)
            imag = np.asarray(imag_list, dtype=np.float64)
            arrays["real"] = real
            arrays["imag"] = imag
            arrays["complex"] = real + 1j * imag

            tx_num = int(cube.txAntNum)
            rx_num = int(cube.rxAntNum)
            dfft = int(cube.dopFftPointNum)
            rfft = int(cube.rangeFftPointNum)
            expected = dfft * rfft
            if (
                tx_num > 0
                and rx_num > 0
                and len(ants) == tx_num * rx_num
                and all(len(row) == expected for row in real_list)
            ):
                real_cube = np.zeros((tx_num, rx_num, dfft, rfft), dtype=np.float64)
                imag_cube = np.zeros_like(real_cube)
                for idx, ant in enumerate(ants):
                    t = int(ant.txAntId)
                    r = int(ant.rxAntId)
                    if 0 <= t < tx_num and 0 <= r < rx_num:
                        real_cube[t, r] = np.asarray(real_list[idx]).reshape(dfft, rfft)
                        imag_cube[t, r] = np.asarray(imag_list[idx]).reshape(dfft, rfft)
                arrays["real_cube"] = real_cube
                arrays["imag_cube"] = imag_cube
                arrays["complex_cube"] = real_cube + 1j * imag_cube

        npz_path = base.with_suffix(".npz")
        np.savez(npz_path, **arrays)
        return str(npz_path)

    def _handle_point_cloud(self, seq: int, received_at: float, data: Any, payload: Optional[bytes]) -> None:
        if payload is None or (not self.save_point_cloud and self.on_point_cloud is None):
            return
        try:
            clouds = self.api.ConvertPointCloudData(
                payload,
                int(data.frameIndex),
                int(data.frameLen),
            )
        except Exception as exc:
            with self.lock:
                self.error_count += 1
            print(f"WARNING: point cloud conversion failed: {exc}", flush=True)
            return
        if clouds is None:
            return

        clouds = list(clouds)
        with self.lock:
            self.point_cloud_count += len(clouds)

        for pc in clouds:
            name = str(pc.signalName)
            dim = int(pc.dim)
            point_num = int(pc.pointNum)
            xyz = [self._coords(pc.x, point_num), self._coords(pc.y, point_num)]
            z = self._coords(pc.z, point_num)
            xyz.append(z)
            w = self._coords(pc.w, point_num)
            u = self._coords(pc.u, point_num)
            v = self._coords(pc.v, point_num)

            if self.save_point_cloud:
                for idx in range(point_num):
                    row = {
                        "seq": seq,
                        "time": received_at,
                        "frame_index": int(data.frameIndex),
                        "signal_name": name,
                        "dim": dim,
                        "point_num": point_num,
                        "idx": idx,
                        "x": self._value_or_empty(pc.x, idx),
                        "y": self._value_or_empty(pc.y, idx),
                        "z": self._value_or_empty(pc.z, idx),
                        "w": self._value_or_empty(pc.w, idx),
                        "u": self._value_or_empty(pc.u, idx),
                        "v": self._value_or_empty(pc.v, idx),
                    }
                    self._point_csv.writerow(row)

            if self.on_point_cloud is not None:
                event = LivePointCloudEvent(
                    seq=seq,
                    received_at=received_at,
                    frame_index=int(data.frameIndex),
                    signal_name=name,
                    dim=dim,
                    point_num=point_num,
                    x=xyz[0],
                    y=xyz[1],
                    z=xyz[2],
                    w=w,
                    u=u,
                    v=v,
                )
                try:
                    self.on_point_cloud(event)
                except Exception as exc:
                    print(f"WARNING: live point cloud callback failed: {exc}", flush=True)

    @staticmethod
    def _coords(values: Any, point_num: int) -> List[float]:
        if values is None:
            return []
        out: List[float] = []
        for idx in range(point_num):
            try:
                out.append(float(values[idx]))
            except Exception:
                out.append(0.0)
        return out

    @staticmethod
    def _value_or_empty(values: Any, idx: int) -> Any:
        if values is None:
            return ""
        try:
            return values[idx]
        except Exception:
            return ""

    def stop(self, interrupted: bool = False) -> None:
        if self.stopped:
            return
        self.stopped = True
        self.stop_event.set()
        if self.worker is not None and self.worker.is_alive():
            self.worker.join(timeout=5)
        if self.api is not None:
            try:
                self.api.callback -= self._on_event
            except Exception:
                pass
        self._info["interrupted"] = interrupted
        if self.save_manifest:
            self._write_info()
        for fh in (
            self.msg_file,
            self.cube_file,
            self.point_file,
            self.frame_info_file,
        ):
            if fh is not None and not fh.closed:
                fh.close()


def run_check(args: argparse.Namespace) -> int:
    print("HIF DLL:", Path(args.dll).resolve())
    api, _ = load_api(args.dll)
    print("Library version:", api.GetLibVersion())
    print("CH347 devices:")
    devices = list_ch347_devices()
    print(devices if devices else "  (not found)")
    print("Known host tools running:", list_known_host_tools())
    if not devices:
        print("No online CH347 SPI device was found.", file=sys.stderr)
        return 2
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Collect HIF messages from RS6x/7x radar over CH347T SPI."
    )
    parser.add_argument("--dll", type=Path, default=default_dll_path(), help="HifMsgDataCollectionLib DLL path")
    parser.add_argument("--index", type=int, default=0, help="CH347 device index (default 0)")
    parser.add_argument("--freq-mode", type=int, default=0, choices=sorted(FREQ_MODE_LABELS), help="SPI clock mode")
    parser.add_argument("--config", type=Path, help="radar config file to send through SendConfigFile")
    parser.add_argument("--send-hex", type=parse_hex, metavar="HEX", help="send a raw HIF command before collecting, e.g. A5 C0 15 ...")
    parser.add_argument("--out", type=Path, default=Path("hif_spi_capture"), help="output root directory")
    parser.add_argument("--duration", type=float, default=0, help="collect for N seconds (0=until Ctrl+C)")
    parser.add_argument("--max-frames", type=int, default=0, help="stop after N callback messages")
    parser.add_argument("--max-bytes", type=int, default=0, help="stop after N payload bytes")
    parser.add_argument("--tx", type=int, default=1, help="C1 TX antenna count (default P1812 1T3R)")
    parser.add_argument("--rx", type=int, default=3, help="C1 RX antenna count")
    parser.add_argument("--rfft", type=int, default=256, help="C1 range FFT bins")
    parser.add_argument("--dfft", type=int, default=32, help="C1 Doppler FFT bins")
    parser.add_argument("--retransmission", action="store_true", help="enable DataCube retransmission")
    parser.add_argument("--no-payload", action="store_true", help="do not save raw payloads")
    parser.add_argument("--no-cube-files", action="store_true", help="do not save C1/C2 .bin/.iq.bin files")
    parser.add_argument("--no-npz", action="store_true", help="do not write NumPy .npz files")
    parser.add_argument("--no-pointcloud", action="store_true", help="do not export point cloud CSV")
    parser.add_argument("--no-manifest", action="store_true", help="do not write manifests or session info")
    parser.add_argument("--verbose", action="store_true", help="print every event")
    parser.add_argument("--check", action="store_true", help="check DLL/CH347 environment without opening SPI")
    return parser


def run_collect(args: argparse.Namespace) -> int:
    api, _ = load_api(args.dll)
    collector = Collector(
        api=api,
        index=args.index,
        freq_mode=args.freq_mode,
        out_root=args.out,
        tx=args.tx,
        rx=args.rx,
        rfft=args.rfft,
        dfft=args.dfft,
        save_payload=not args.no_payload,
        save_cube_files=not args.no_cube_files,
        save_npz=not args.no_npz,
        save_point_cloud=not args.no_pointcloud,
        save_manifest=not args.no_manifest,
        verbose=args.verbose,
    )
    collector.start_session()

    opened = api.OpenSpiDevice(args.index, args.freq_mode)
    if not opened:
        print(
            f"OpenSpiDevice({args.index}, {args.freq_mode}) failed. "
            "Close RadarDebugTool/RadarAnalysisTool/RadarKnow-CS/sscom and retry.",
            file=sys.stderr,
        )
        collector.stop()
        return 2
    print(
        f"SPI opened: index={args.index} freq_mode={args.freq_mode} "
        f"({FREQ_MODE_LABELS.get(args.freq_mode, 'unknown')})",
        flush=True,
    )

    interrupted = False
    try:
        api.StopCollectingData()
        if args.config is not None:
            if not args.config.is_file():
                raise FileNotFoundError(f"config file not found: {args.config}")
            ret = int(api.SendConfigFile(str(args.config)))
            print(f"SendConfigFile -> {ret} ({ERROR_CODES.get(ret, 'unknown')})", flush=True)
            if ret != 0:
                raise RuntimeError(f"SendConfigFile failed with code {ret}")

        if args.send_hex:
            from System import Array, Byte

            arr = Array[Byte](list(args.send_hex))
            ret = int(api.SendData(arr))
            print(f"SendData -> {ret} ({ERROR_CODES.get(ret, 'unknown')})", flush=True)

        if args.retransmission:
            api.EnableRetransmission()

        if not api.StartCollectingData():
            raise RuntimeError("StartCollectingData returned False")
        print("HIF collection started. Press Ctrl+C to stop.", flush=True)

        deadline = time.monotonic() + args.duration if args.duration > 0 else None
        while not collector.stop_event.is_set():
            if deadline is not None and time.monotonic() >= deadline:
                break
            if args.max_frames and collector.message_count >= args.max_frames:
                break
            if args.max_bytes and collector.total_bytes >= args.max_bytes:
                break
            time.sleep(0.2)
    except KeyboardInterrupt:
        interrupted = True
        print("\nInterrupted by user.", flush=True)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        try:
            api.StopCollectingData()
        except Exception:
            pass
        try:
            api.CloseSpiDevice(args.index)
        except Exception:
            pass
        collector.stop(interrupted=interrupted)

    print(
        f"done: messages={collector.message_count} cubes={collector.cube_count} "
        f"point_clouds={collector.point_cloud_count} "
        f"frame_infos={collector.frame_info_count} bytes={collector.total_bytes} "
        f"errors={collector.error_count}",
        flush=True,
    )
    print(f"output: {collector.session_dir}", flush=True)
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    signal.signal(signal.SIGINT, signal.default_int_handler)
    if args.check:
        return run_check(args)
    return run_collect(args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        sys.exit(130)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
