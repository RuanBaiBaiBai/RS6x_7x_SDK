"""Shared HIF frame and radar DataCube parsing helpers.

The implementation follows the HIF message layer and RS6x/7x C1/C2 report
format used by the SDK in this workspace. It is intentionally dependency-free
so it can be reused by both the raw stream saver and the offline parser.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import BinaryIO, Iterator, List, Optional, Tuple


HIF_MSG_MAGIC = 0xA5
HIF_MSG_TYPE_TO_HOST = 2
HIF_MSG_FLAG_REQ_BIT = 0x01
HIF_MSG_FLAG_ENCRYPT_BIT = 0x02
HIF_MSG_FLAG_CHECK_BIT = 0x04
HIF_MSG_FLAG_MORE_DATA_BIT = 0x08
HIF_MSG_FLAG_EXTEND_BIT = 0x10
HIF_MSG_FLAG_MAC32_BIT = 0x20

HIF_PHY_HEAD_LEN = 2
HIF_MSG_HEAD_LEN = 4
HIF_EXT_HEAD_LEN = 4
HIF_FRAG_HEAD_LEN = 8
HIF_CHECK_LEN = 4
HIF_HEAD_TOTAL_LEN = HIF_PHY_HEAD_LEN + HIF_MSG_HEAD_LEN

MMW_FRAME_UPLOAD_LEN = 12
MMW_FRAME_TL_LEN = 12
MMW_MSG_C1 = 0xC1
MMW_MSG_C2 = 0xC2
MMW_MSG_C3 = 0xC3
MMW_MSG_C6 = 0xC6
MMW_POINT_TL_TYPE = 4
MMW_TL_FLAG_CART = 0x01
MMW_TL_FLAG_MICRO = 0x02
MMW_POINT_RECORD_BYTES = 12
GENERAL_PROTO_HEADER_LEN = 5

MAX_HIF_MSG_LENGTH = 0x0FFF
MAX_HIF_FRAME_LENGTH = (
    HIF_HEAD_TOTAL_LEN + HIF_EXT_HEAD_LEN + MAX_HIF_MSG_LENGTH + HIF_CHECK_LEN
)
MAX_DATACUBE_BYTES = 64 * 1024 * 1024


@dataclass(frozen=True)
class HifMsgHeader:
    type_: int
    flag: int
    msg_id: int
    length: int
    seq: int
    frag: int
    raw: int

    @property
    def check_enabled(self) -> bool:
        return bool(self.flag & HIF_MSG_FLAG_CHECK_BIT)

    @property
    def extend_enabled(self) -> bool:
        return bool(self.flag & HIF_MSG_FLAG_EXTEND_BIT)


@dataclass(frozen=True)
class FragmentHeader:
    total_len: int
    offset: int
    flow_seq: int
    reserved: int
    raw: int


@dataclass
class HifFrame:
    offset: int
    raw: bytes
    header: HifMsgHeader
    payload: bytes
    ext: bytes
    fragment: Optional[FragmentHeader]
    checksum_ok: Optional[bool]
    status: str
    notes: str = ""


@dataclass(frozen=True)
class DataFrameHeader:
    frame_idx: int
    frame_len: int
    data_offset: int
    raw: bytes


@dataclass(frozen=True)
class TLHeader:
    type_: int
    total_length: int
    tx_num: int
    rx_num: int
    rsv1: int
    rsv2: int
    range_bin_num: int
    dop_bin_num: int
    raw: bytes


@dataclass(frozen=True)
class PointRecord:
    x: float
    y: float
    z: float
    w: float
    u: float
    v: float


@dataclass(frozen=True)
class PointCloudFrame:
    frame_idx: int
    frame_len: int
    data_offset: int
    signal_name: str
    dim: int
    records: Tuple[PointRecord, ...]


@dataclass(frozen=True)
class GeneralData:
    signal_name: str
    dim: int
    data_format: int
    signed: int
    align_mode: int
    q_value: int
    values: Tuple[float, ...]


@dataclass(frozen=True)
class FrameInfoRecord:
    frame_idx: int
    type: int
    period_ms: int
    range_mm: int
    range_fft: int
    velocity_mm: int
    doppler_fft: int
    extra1: int
    antenna_count: int
    motion_points: int
    presence_points: int


@dataclass
class DataCubeFrame:
    msg_id: int
    frame_idx: int
    header: DataFrameHeader
    raw: bytes
    iq: bytes
    tl: Optional[TLHeader]
    tl_bytes: bytes
    n_samples: int
    piece_count: int

    @property
    def name(self) -> str:
        if self.msg_id == MMW_MSG_C1:
            return "C1"
        if self.msg_id == MMW_MSG_C2:
            return "C2"
        return f"0x{self.msg_id:02X}"


@dataclass
class _ActivePiece:
    key: Tuple[int, int]
    msg_id: int
    header: DataFrameHeader
    total_bytes: int
    buffer: bytearray
    got: bytearray
    filled: int
    piece_count: int

    def add(self, offset: int, data: bytes) -> bool:
        end = offset + len(data)
        if offset < 0 or end > self.total_bytes:
            return False
        self.buffer[offset:end] = data
        for i in range(len(data)):
            if not self.got[offset + i]:
                self.got[offset + i] = 1
                self.filled += 1
        self.piece_count += 1
        return True

    def is_complete(self) -> bool:
        return self.total_bytes > 0 and self.filled == self.total_bytes


@dataclass
class _ActiveFragment:
    key: Tuple[int, int]
    total_len: int
    buffer: bytearray
    got: bytearray
    filled: int
    piece_count: int

    def add(self, offset: int, data: bytes) -> bool:
        end = offset + len(data)
        if offset < 0 or end > self.total_len:
            return False
        self.buffer[offset:end] = data
        for i in range(len(data)):
            if not self.got[offset + i]:
                self.got[offset + i] = 1
                self.filled += 1
        self.piece_count += 1
        return True

    def is_complete(self) -> bool:
        return self.total_len > 0 and self.filled == self.total_len


def sum8(data: bytes) -> int:
    total = 0
    for value in data:
        total = (total + value) & 0xFF
    return total


def compute_check8(magic: int, msg_header: bytes) -> int:
    return (~sum8(bytes((magic,)) + msg_header)) & 0xFF


def checksum32_expected(header_bytes: bytes, payload_bytes: bytes) -> int:
    data = header_bytes + payload_bytes
    total = 0
    for i in range(0, len(data), 4):
        word = int.from_bytes(data[i : i + 4].ljust(4, b"\x00"), "little")
        total = (total + word) & 0xFFFFFFFF
    return (~total) & 0xFFFFFFFF


def pack_msg_header(
    msg_id: int,
    length: int,
    type_: int = HIF_MSG_TYPE_TO_HOST,
    flag: int = HIF_MSG_FLAG_CHECK_BIT,
    seq: int = 0,
    frag: int = 0,
) -> Tuple[bytes, int]:
    raw = (
        (type_ & 0x03)
        | ((flag & 0x3F) << 2)
        | ((msg_id & 0xFF) << 8)
        | ((length & 0x0FFF) << 16)
        | ((seq & 0x07) << 28)
        | ((frag & 0x01) << 31)
    )
    return raw.to_bytes(4, "little"), raw


def pack_fragment_header(total_len: int, offset: int, flow_seq: int) -> bytes:
    packed = (offset & 0xFFFFFF) | ((flow_seq & 0x3F) << 24)
    return struct.pack("<II", total_len & 0xFFFFFFFF, packed & 0xFFFFFFFF)


def pack_hif_frame(
    msg_id: int,
    payload: bytes,
    type_: int = HIF_MSG_TYPE_TO_HOST,
    flag: int = HIF_MSG_FLAG_CHECK_BIT,
    seq: int = 0,
    frag: int = 0,
) -> bytes:
    msg_header, _ = pack_msg_header(msg_id, len(payload), type_, flag, seq, frag)
    check8 = compute_check8(HIF_MSG_MAGIC, msg_header)
    check32 = checksum32_expected(msg_header, payload)
    return (
        bytes((HIF_MSG_MAGIC, check8))
        + msg_header
        + payload
        + check32.to_bytes(4, "little")
    )


def parse_msg_header(data: bytes) -> Optional[HifMsgHeader]:
    if len(data) < HIF_MSG_HEAD_LEN:
        return None
    raw = int.from_bytes(data[:HIF_MSG_HEAD_LEN], "little")
    return HifMsgHeader(
        type_=raw & 0x03,
        flag=(raw >> 2) & 0x3F,
        msg_id=(raw >> 8) & 0xFF,
        length=(raw >> 16) & 0x0FFF,
        seq=(raw >> 28) & 0x07,
        frag=(raw >> 31) & 0x01,
        raw=raw,
    )


def parse_fragment_header(data: bytes) -> Optional[FragmentHeader]:
    if len(data) < HIF_FRAG_HEAD_LEN:
        return None
    total_len = struct.unpack_from("<I", data, 0)[0]
    packed = struct.unpack_from("<I", data, 4)[0]
    return FragmentHeader(
        total_len=total_len,
        offset=packed & 0xFFFFFF,
        flow_seq=(packed >> 24) & 0x3F,
        reserved=(packed >> 30) & 0x03,
        raw=packed,
    )


def parse_upload_header(data: bytes) -> Optional[DataFrameHeader]:
    if len(data) < MMW_FRAME_UPLOAD_LEN:
        return None
    frame_idx, frame_len, data_offset = struct.unpack_from("<III", data, 0)
    return DataFrameHeader(
        frame_idx=frame_idx,
        frame_len=frame_len,
        data_offset=data_offset,
        raw=bytes(data[:MMW_FRAME_UPLOAD_LEN]),
    )


def parse_tl_header(data: bytes) -> Optional[TLHeader]:
    if len(data) < MMW_FRAME_TL_LEN:
        return None
    first = struct.unpack_from("<I", data, 0)[0]
    tx_num, rx_num, rsv1, rsv2 = data[4:8]
    range_bin_num, dop_bin_num = struct.unpack_from("<HH", data, 8)
    return TLHeader(
        type_=first & 0xFF,
        total_length=(first >> 8) & 0xFFFFFF,
        tx_num=tx_num,
        rx_num=rx_num,
        rsv1=rsv1,
        rsv2=rsv2,
        range_bin_num=range_bin_num,
        dop_bin_num=dop_bin_num,
        raw=bytes(data[:MMW_FRAME_TL_LEN]),
    )


def frame_length_from_header(header: HifMsgHeader) -> int:
    length = HIF_HEAD_TOTAL_LEN + header.length
    if header.extend_enabled:
        length += HIF_EXT_HEAD_LEN
    if header.check_enabled:
        length += HIF_CHECK_LEN
    return length


def parse_frame_bytes(
    raw: bytes,
    offset: int,
    accept_bad_checksum: bool = False,
) -> HifFrame:
    header_bytes = raw[HIF_PHY_HEAD_LEN : HIF_PHY_HEAD_LEN + HIF_MSG_HEAD_LEN]
    header = parse_msg_header(header_bytes)
    if header is None:
        return HifFrame(
            offset=offset,
            raw=raw,
            header=HifMsgHeader(0, 0, 0, 0, 0, 0, 0),
            payload=b"",
            ext=b"",
            fragment=None,
            checksum_ok=False,
            status="bad_header",
            notes="message header is shorter than 4 bytes",
        )

    ext = b""
    ext_len = HIF_EXT_HEAD_LEN if header.extend_enabled else 0
    payload_start = HIF_HEAD_TOTAL_LEN + ext_len
    payload_end = payload_start + header.length
    if payload_end > len(raw):
        return HifFrame(
            offset=offset,
            raw=raw,
            header=header,
            payload=b"",
            ext=ext,
            fragment=None,
            checksum_ok=False,
            status="bad_length",
            notes="raw frame is shorter than MsgHeader.length",
        )

    ext = raw[HIF_HEAD_TOTAL_LEN:payload_start]
    payload = raw[payload_start:payload_end]

    fragment = None
    if header.frag:
        fragment = parse_fragment_header(payload)

    checksum_ok: Optional[bool] = None
    notes = ""
    if header.check_enabled:
        expected = checksum32_expected(header_bytes + ext, payload)
        received = int.from_bytes(raw[payload_end : payload_end + HIF_CHECK_LEN], "little")
        checksum_ok = expected == received
        if not checksum_ok:
            notes = "Check32 mismatch"
            if header.frag:
                notes += ", fragment layout may be stale or frame is corrupted"
            if not accept_bad_checksum:
                notes += "; skipped for datacube reassembly"

    status = "ok"
    if checksum_ok is False:
        status = "bad_checksum"

    return HifFrame(
        offset=offset,
        raw=raw,
        header=header,
        payload=payload,
        ext=ext,
        fragment=fragment,
        checksum_ok=checksum_ok,
        status=status,
        notes=notes,
    )


def iter_hif_frames(data: bytes, accept_bad_checksum: bool = False) -> Iterator[HifFrame]:
    if isinstance(data, bytearray):
        data = bytes(data)
    pos = 0
    while pos < len(data):
        idx = data.find(bytes((HIF_MSG_MAGIC,)), pos)
        if idx < 0:
            break
        if len(data) - idx < HIF_HEAD_TOTAL_LEN:
            break
        header = parse_msg_header(data[idx + HIF_PHY_HEAD_LEN : idx + HIF_HEAD_TOTAL_LEN])
        if header is None:
            pos = idx + 1
            continue
        expected_check8 = compute_check8(
            HIF_MSG_MAGIC,
            data[idx + HIF_PHY_HEAD_LEN : idx + HIF_HEAD_TOTAL_LEN],
        )
        if data[idx + 1] != expected_check8:
            pos = idx + 1
            continue
        frame_len = frame_length_from_header(header)
        if idx + frame_len > len(data):
            break
        frame = parse_frame_bytes(
            bytes(data[idx : idx + frame_len]),
            idx,
            accept_bad_checksum=accept_bad_checksum,
        )
        if frame.checksum_ok is False and not accept_bad_checksum:
            pos = idx + 1
            yield frame
        else:
            pos = idx + frame_len
            yield frame


def stream_hif_frames(
    stream: BinaryIO,
    chunk_size: int = 65536,
    accept_bad_checksum: bool = False,
) -> Iterator[HifFrame]:
    buffer = bytearray()
    base_offset = 0
    eof = False

    while True:
        if not eof:
            chunk = stream.read(chunk_size)
            if not chunk:
                eof = True
            else:
                buffer.extend(chunk)

        while True:
            if len(buffer) < HIF_HEAD_TOTAL_LEN:
                break
            idx = buffer.find(bytes((HIF_MSG_MAGIC,)))
            if idx < 0:
                base_offset += len(buffer)
                buffer.clear()
                break
            if idx > 0:
                base_offset += idx
                del buffer[:idx]
            if len(buffer) < HIF_HEAD_TOTAL_LEN:
                break

            header = parse_msg_header(bytes(buffer[HIF_PHY_HEAD_LEN : HIF_HEAD_TOTAL_LEN]))
            if header is None:
                base_offset += 1
                del buffer[0]
                continue
            expected_check8 = compute_check8(
                HIF_MSG_MAGIC,
                bytes(buffer[HIF_PHY_HEAD_LEN : HIF_HEAD_TOTAL_LEN]),
            )
            if buffer[1] != expected_check8:
                base_offset += 1
                del buffer[0]
                continue

            frame_len = frame_length_from_header(header)
            if frame_len > MAX_HIF_FRAME_LENGTH:
                base_offset += 1
                del buffer[0]
                continue
            if len(buffer) < frame_len:
                if eof:
                    return
                break

            frame = parse_frame_bytes(
                bytes(buffer[:frame_len]),
                base_offset,
                accept_bad_checksum=accept_bad_checksum,
            )
            if frame.checksum_ok is False and not accept_bad_checksum:
                base_offset += 1
                del buffer[0]
            else:
                base_offset += frame_len
                del buffer[:frame_len]
            yield frame

            if eof and len(buffer) < HIF_HEAD_TOTAL_LEN:
                return

        if eof:
            return


def default_cube_shape(
    msg_id: int,
    tl: Optional[TLHeader],
) -> Optional[Tuple[int, int, int, int]]:
    if msg_id == MMW_MSG_C2 and tl is not None:
        return (tl.tx_num, tl.rx_num, tl.dop_bin_num, tl.range_bin_num)
    return None


def parse_point_cloud_data(payload: bytes) -> List[PointCloudFrame]:
    """Parse complete C3 point cloud payloads into motion/presence frames."""
    if len(payload) < MMW_FRAME_UPLOAD_LEN + 4:
        return []
    frame_idx, frame_len, data_offset = struct.unpack_from("<III", payload, 0)

    frames: List[PointCloudFrame] = []
    pos = MMW_FRAME_UPLOAD_LEN
    while pos + 4 <= len(payload):
        type_, flag, length = struct.unpack_from("<BBH", payload, pos)
        pos += 4
        if length <= 0 or pos + length > len(payload):
            break
        if type_ != MMW_POINT_TL_TYPE:
            pos += length
            continue
        if length % MMW_POINT_RECORD_BYTES:
            break

        record_count = length // MMW_POINT_RECORD_BYTES
        values = struct.unpack_from(f"<{record_count * 6}h", payload, pos)
        records: List[PointRecord] = []
        for index in range(record_count):
            base = index * 6
            x, y, z, w, u, v = values[base : base + 6]
            records.append(
                PointRecord(
                    x=x / 100.0,
                    y=y / 100.0,
                    z=z / 100.0,
                    w=w / 100.0,
                    u=u / 100.0,
                    v=v / 100.0,
                )
            )
        pos += length
        signal_name = "presence" if flag & MMW_TL_FLAG_MICRO else "motion"
        frames.append(
            PointCloudFrame(
                frame_idx=frame_idx,
                frame_len=frame_len,
                data_offset=data_offset,
                signal_name=signal_name,
                dim=3,
                records=tuple(records),
            )
        )
    return frames


def parse_general_data(payload: bytes) -> Optional[GeneralData]:
    """Parse a C6 general/debug payload into one named data channel."""
    if len(payload) < GENERAL_PROTO_HEADER_LEN:
        return None

    dim = payload[0]
    data_format = payload[1]
    align_mode = payload[2]
    length = payload[3] | (payload[4] << 8)
    name_end = payload.find(b"\x00", GENERAL_PROTO_HEADER_LEN)
    if name_end < 0:
        return None
    signal_name = payload[GENERAL_PROTO_HEADER_LEN:name_end].decode(
        "utf-8", errors="replace"
    )
    pos = name_end + 1

    signed = (data_format >> 2) & 0x01
    q_value = (data_format >> 3) & 0x1F
    base_format = data_format & 0x03
    if base_format == 0:
        unit_size = 1
        fmt = "b" if signed else "B"
    elif base_format == 1:
        unit_size = 2
        fmt = "h" if signed else "H"
    elif base_format == 2:
        unit_size = 4
        fmt = "i" if signed else "I"
    else:
        unit_size = 4
        fmt = "f" if not signed else "d"

    value_count = length * dim
    if value_count <= 0 or pos + value_count * unit_size > len(payload):
        return GeneralData(
            signal_name=signal_name,
            dim=dim,
            data_format=data_format,
            signed=signed,
            align_mode=align_mode,
            q_value=q_value,
            values=(),
        )
    values = struct.unpack_from(f"<{value_count}{fmt}", payload, pos)

    # Header align_mode=0 stores channels column-major: x0..xN, y0..yN.
    if dim > 1 and align_mode == 0:
        channels = [values[channel * length : (channel + 1) * length] for channel in range(dim)]
        values = tuple(value for channel in channels for value in channel)

    return GeneralData(
        signal_name=signal_name,
        dim=dim,
        data_format=data_format,
        signed=signed,
        align_mode=align_mode,
        q_value=q_value,
        values=tuple(float(value) for value in values),
    )


def parse_frame_info(payload: bytes) -> Optional[FrameInfoRecord]:
    """Decode the radar_framework `frame_info` C6 channel."""
    data = parse_general_data(payload)
    if data is None or data.signal_name != "frame_info" or len(data.values) < 11:
        return None
    values = data.values
    return FrameInfoRecord(
        frame_idx=int(values[0]),
        type=int(values[1]),
        period_ms=int(values[2]),
        range_mm=int(values[3]),
        range_fft=int(values[4]),
        velocity_mm=int(values[5]),
        doppler_fft=int(values[6]),
        extra1=int(values[7]),
        antenna_count=int(values[8]),
        motion_points=int(values[9]),
        presence_points=int(values[10]),
    )


class HifFragmentAssembler:
    """Reassembles HIF message-layer fragments into one app payload."""

    def __init__(self, max_active: int = 64, max_total_len: int = MAX_DATACUBE_BYTES):
        self.max_active = max_active
        self.max_total_len = max_total_len
        self._active = {}
        self._order = []
        self.stats = {
            "non_fragment": 0,
            "fragments": 0,
            "fragments_complete": 0,
            "fragment_errors": 0,
        }

    @property
    def active_count(self) -> int:
        return len(self._active)

    def add(self, frame: HifFrame) -> List[bytes]:
        if not frame.header.frag:
            self.stats["non_fragment"] += 1
            return [frame.payload]

        if frame.fragment is None or len(frame.payload) < HIF_FRAG_HEAD_LEN:
            self.stats["fragment_errors"] += 1
            return []

        frag = frame.fragment
        if frag.total_len <= 0 or frag.total_len > self.max_total_len:
            self.stats["fragment_errors"] += 1
            return []

        key = (frame.header.msg_id, frag.flow_seq)
        state = self._active.get(key)
        if state is None:
            if len(self._active) >= self.max_active:
                old_key = self._order.pop(0)
                self._active.pop(old_key, None)
            state = _ActiveFragment(
                key=key,
                total_len=frag.total_len,
                buffer=bytearray(frag.total_len),
                got=bytearray(frag.total_len),
                filled=0,
                piece_count=0,
            )
            self._active[key] = state
            self._order.append(key)
            self.stats["fragments"] += 1

        fragment_data = frame.payload[HIF_FRAG_HEAD_LEN:]
        if not state.add(frag.offset, fragment_data):
            self.stats["fragment_errors"] += 1
            return []

        if not state.is_complete():
            return []

        self.stats["fragments_complete"] += 1
        self._active.pop(key)
        self._order.remove(key)
        return [bytes(state.buffer)]


class DataPieceAssembler:
    """Reassembles C1/C2 report pieces that share frame_idx/data offsets."""

    def __init__(self, max_active: int = 64, max_total_len: int = MAX_DATACUBE_BYTES):
        self.max_active = max_active
        self.max_total_len = max_total_len
        self._active = {}
        self._order = []
        self.stats = {
            "pieces": 0,
            "cubes_complete": 0,
            "invalid_pieces": 0,
            "ack_like_empty_pieces": 0,
        }

    @property
    def active_count(self) -> int:
        return len(self._active)

    def add(self, msg_id: int, payload: bytes) -> List[DataCubeFrame]:
        if msg_id not in (MMW_MSG_C1, MMW_MSG_C2):
            return []

        header = parse_upload_header(payload)
        if header is None:
            self.stats["invalid_pieces"] += 1
            return []

        data = payload[MMW_FRAME_UPLOAD_LEN:]
        if not data:
            self.stats["ack_like_empty_pieces"] += 1
            return []

        if header.frame_len == 0:
            self.stats["invalid_pieces"] += 1
            return []

        if msg_id == MMW_MSG_C2:
            data_bytes = header.frame_len
            offset_bytes = header.data_offset
            if data_bytes < MMW_FRAME_TL_LEN:
                self.stats["invalid_pieces"] += 1
                return []
        else:
            if header.frame_len > (self.max_total_len // 4):
                self.stats["invalid_pieces"] += 1
                return []
            data_bytes = header.frame_len * 4
            offset_bytes = header.data_offset * 4

        if data_bytes <= 0 or data_bytes > self.max_total_len:
            self.stats["invalid_pieces"] += 1
            return []

        key = (msg_id, header.frame_idx)
        state = self._active.get(key)
        if state is None:
            if len(self._active) >= self.max_active:
                old_key = self._order.pop(0)
                self._active.pop(old_key, None)
            state = _ActivePiece(
                key=key,
                msg_id=msg_id,
                header=header,
                total_bytes=data_bytes,
                buffer=bytearray(data_bytes),
                got=bytearray(data_bytes),
                filled=0,
                piece_count=0,
            )
            self._active[key] = state
            self._order.append(key)

        if not state.add(offset_bytes, data):
            self.stats["invalid_pieces"] += 1
            return []
        self.stats["pieces"] += 1

        if not state.is_complete():
            return []

        self.stats["cubes_complete"] += 1
        self._active.pop(key)
        self._order.remove(key)

        tl_bytes = b""
        tl = None
        if msg_id == MMW_MSG_C2:
            tl_bytes = bytes(state.buffer[:MMW_FRAME_TL_LEN])
            tl = parse_tl_header(tl_bytes)
            iq = bytes(state.buffer[MMW_FRAME_TL_LEN:])
        else:
            iq = bytes(state.buffer)

        return [
            DataCubeFrame(
                msg_id=msg_id,
                frame_idx=header.frame_idx,
                header=header,
                raw=header.raw + bytes(state.buffer),
                iq=iq,
                tl=tl,
                tl_bytes=tl_bytes,
                n_samples=len(iq) // 4,
                piece_count=state.piece_count,
            )
        ]
