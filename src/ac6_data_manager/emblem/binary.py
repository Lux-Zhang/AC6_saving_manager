from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import UTC, datetime
import hashlib
import struct
import zlib
from typing import Iterable, Sequence

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


USER_DATA007_AES_KEY = bytes.fromhex("B156879F134897987005C48700AEF879")
USER_DATA_FILE_SENTINEL = 0x00291222
GROUP_NODE_MASK = 0x3F00
GROUP_NODE_VALUE = 0x3F00
INVERTED_DECAL_BIT = 0x4000
EMBLEM_USER_CATEGORY = 1
EMBLEM_IMPORTED_USER_CATEGORY = 2


class EmblemFormatError(ValueError):
    """Raised when emblem or USER_DATA007 bytes do not match the proven format."""


class _Cursor:
    def __init__(self, data: bytes) -> None:
        self._view = memoryview(data)
        self.offset = 0

    def tell(self) -> int:
        return self.offset

    def seek(self, offset: int) -> None:
        if offset < 0 or offset > len(self._view):
            raise EmblemFormatError(f"Cursor seek out of bounds: {offset}")
        self.offset = offset

    def read(self, size: int) -> bytes:
        if size < 0 or self.offset + size > len(self._view):
            raise EmblemFormatError(
                f"Tried to read {size} bytes at offset {self.offset}, beyond payload boundary"
            )
        start = self.offset
        self.offset += size
        return self._view[start : start + size].tobytes()

    def read_struct(self, fmt: str) -> tuple[object, ...]:
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.read(size))

    def remaining(self) -> int:
        return len(self._view) - self.offset



def _fixed_cstring(value: str, size: int = 16) -> bytes:
    raw = value.encode("ascii")
    if len(raw) >= size:
        raise EmblemFormatError(f"String too long for fixed buffer: {value!r}")
    return raw + b"\x00" * (size - len(raw))



def _read_fixed_cstring(cursor: _Cursor, size: int = 16) -> str:
    raw = cursor.read(size)
    terminator = raw.find(b"\x00")
    if terminator < 0:
        raise EmblemFormatError("Fixed string missing null terminator")
    return raw[:terminator].decode("ascii")



def _encode_wstring(value: str) -> bytes:
    return value.encode("utf-16-le") + b"\x00\x00"



def _decode_wstring(payload: bytes) -> str:
    if len(payload) % 2:
        raise EmblemFormatError("UTF-16LE payload must have even length")
    units: list[bytes] = []
    for index in range(0, len(payload), 2):
        unit = payload[index : index + 2]
        if unit == b"\x00\x00":
            return b"".join(units).decode("utf-16-le")
        units.append(unit)
    raise EmblemFormatError("Wide string missing null terminator")



def _filetime_to_datetime(filetime: int) -> datetime:
    unix_seconds = (filetime - 116444736000000000) / 10_000_000
    return datetime.fromtimestamp(unix_seconds, tz=UTC)



def _datetime_to_filetime(value: datetime) -> int:
    if value.tzinfo is None:
        value = value.replace(tzinfo=UTC)
    value = value.astimezone(UTC)
    return int(value.timestamp() * 10_000_000) + 116444736000000000



def _pack_system_time(value: datetime) -> int:
    value = value.astimezone(UTC)
    cst = value.second & 0x3F
    cst = (value.minute & 0x3F) | (cst << 0x06)
    cst = (value.hour & 0x1F) | (cst << 0x05)
    cst = (value.day & 0x1F) | (cst << 0x05)
    cst = (value.weekday() & 0x07) | (cst << 0x03)
    cst = (value.month & 0x0F) | (cst << 0x04)
    cst = (int(value.microsecond / 1000) & 0x3FF) | (cst << 0x0A)
    cst = (value.year & 0x0FFF) | (cst << 0x0C)
    return cst


@dataclass(frozen=True)
class DateTimeBlock:
    filetime: int
    packed_system_time: int

    @classmethod
    def from_datetime(cls, value: datetime) -> "DateTimeBlock":
        return cls(
            filetime=_datetime_to_filetime(value),
            packed_system_time=_pack_system_time(value),
        )

    @classmethod
    def now(cls) -> "DateTimeBlock":
        return cls.from_datetime(datetime.now(tz=UTC))

    @classmethod
    def deserialize(cls, payload: bytes) -> "DateTimeBlock":
        if len(payload) != 16:
            raise EmblemFormatError("DateTime block must be 16 bytes")
        filetime, packed = struct.unpack("<QQ", payload)
        return cls(filetime=filetime, packed_system_time=packed)

    def serialize(self) -> bytes:
        return struct.pack("<QQ", self.filetime, self.packed_system_time)

    def as_datetime(self) -> datetime:
        return _filetime_to_datetime(self.filetime)


@dataclass(frozen=True)
class GroupData:
    decal_id: int
    pos_x: int
    pos_y: int
    scale_x: int
    scale_y: int
    angle: int
    rgba: tuple[int, int, int, int]
    mask_mode: int = 0
    pad: int = 0

    @property
    def base_decal_id(self) -> int:
        return self.decal_id & ~INVERTED_DECAL_BIT

    @property
    def is_group(self) -> bool:
        return (self.decal_id & GROUP_NODE_MASK) == GROUP_NODE_VALUE

    def serialize(self) -> bytes:
        return struct.pack(
            "<hhhhhhBBBBhh",
            self.decal_id,
            self.pos_x,
            self.pos_y,
            self.scale_x,
            self.scale_y,
            self.angle,
            *self.rgba,
            self.mask_mode,
            self.pad,
        )

    @classmethod
    def deserialize(cls, cursor: _Cursor) -> "GroupData":
        values = cursor.read_struct("<hhhhhhBBBBhh")
        return cls(
            decal_id=int(values[0]),
            pos_x=int(values[1]),
            pos_y=int(values[2]),
            scale_x=int(values[3]),
            scale_y=int(values[4]),
            angle=int(values[5]),
            rgba=(int(values[6]), int(values[7]), int(values[8]), int(values[9])),
            mask_mode=int(values[10]),
            pad=int(values[11]),
        )


@dataclass(frozen=True)
class EmblemGroup:
    data: GroupData
    children: tuple["EmblemGroup", ...] = ()

    def serialize(self) -> bytes:
        buffer = bytearray(self.data.serialize())
        if self.children:
            buffer.extend(struct.pack("<i", len(self.children)))
            for child in self.children:
                buffer.extend(child.serialize())
        return bytes(buffer)

    @classmethod
    def deserialize(cls, cursor: _Cursor) -> "EmblemGroup":
        data = GroupData.deserialize(cursor)
        children: tuple[EmblemGroup, ...] = ()
        if data.is_group:
            (child_count,) = cursor.read_struct("<i")
            children = tuple(cls.deserialize(cursor) for _ in range(child_count))
        return cls(data=data, children=children)


@dataclass(frozen=True)
class EmblemLayer:
    group: EmblemGroup

    def serialize(self) -> bytes:
        return struct.pack("<hh", 3, 1) + self.group.serialize()

    @classmethod
    def deserialize(cls, cursor: _Cursor) -> "EmblemLayer":
        header = cursor.read_struct("<hh")
        if header != (3, 1):
            raise EmblemFormatError(f"Unexpected layer header: {header!r}")
        return cls(group=EmblemGroup.deserialize(cursor))


@dataclass(frozen=True)
class EmblemImage:
    layers: tuple[EmblemLayer, ...] = ()

    def serialize(self) -> bytes:
        buffer = bytearray(struct.pack("<hh", 0, len(self.layers)))
        for layer in self.layers:
            buffer.extend(layer.serialize())
        return bytes(buffer)

    @classmethod
    def deserialize(cls, payload: bytes) -> "EmblemImage":
        cursor = _Cursor(payload)
        header = cursor.read_struct("<hh")
        if header[0] != 0:
            raise EmblemFormatError(f"Unexpected image header marker: {header[0]}")
        layer_count = int(header[1])
        layers = tuple(EmblemLayer.deserialize(cursor) for _ in range(layer_count))
        if cursor.remaining() != 0:
            raise EmblemFormatError("Trailing bytes remained after Image parse")
        return cls(layers=layers)


@dataclass(frozen=True)
class EmblemProvenance:
    source_bucket: str
    file_index: int
    file_type: str
    note: str = ""


@dataclass(frozen=True)
class EmblemRecord:
    category: int
    ugc_id: str
    creator_id: int | None
    datetime_block: DateTimeBlock
    image: EmblemImage
    provenance: EmblemProvenance | None = None

    def serialize(self) -> bytes:
        blocks: list[tuple[str, bytes]] = [
            ("Category", struct.pack("<b", self.category)),
            ("UgcID", _encode_wstring(self.ugc_id)),
        ]
        if self.creator_id is not None:
            blocks.append(("CreatorID", struct.pack("<q", self.creator_id)))
        blocks.append(("DateTime", self.datetime_block.serialize()))
        blocks.append(("Image", self.image.serialize()))
        return _serialize_block_container(blocks)

    @classmethod
    def deserialize(
        cls,
        payload: bytes,
        *,
        provenance: EmblemProvenance | None = None,
    ) -> "EmblemRecord":
        blocks = _deserialize_block_container(payload)
        category: int | None = None
        ugc_id = ""
        creator_id: int | None = None
        datetime_block: DateTimeBlock | None = None
        image: EmblemImage | None = None
        for name, data in blocks:
            if name == "Category":
                if len(data) != 1:
                    raise EmblemFormatError("Category block must contain exactly one byte")
                (category,) = struct.unpack("<b", data)
            elif name == "UgcID":
                ugc_id = _decode_wstring(data)
            elif name == "CreatorID":
                if len(data) != 8:
                    raise EmblemFormatError("CreatorID block must be 8 bytes")
                (creator_id,) = struct.unpack("<q", data)
            elif name == "DateTime":
                datetime_block = DateTimeBlock.deserialize(data)
            elif name == "Image":
                image = EmblemImage.deserialize(data)
        if category is None or datetime_block is None or image is None:
            raise EmblemFormatError("EMBC payload is missing one of Category/DateTime/Image blocks")
        return cls(
            category=category,
            ugc_id=ugc_id,
            creator_id=creator_id,
            datetime_block=datetime_block,
            image=image,
            provenance=provenance,
        )

    def as_user_editable(self, *, target_category: int = EMBLEM_IMPORTED_USER_CATEGORY) -> "EmblemRecord":
        return replace(self, category=target_category)


@dataclass(frozen=True)
class UserDataFile:
    file_type: str
    data: bytes

    def serialize(self) -> bytes:
        if len(self.file_type) != 4:
            raise EmblemFormatError("USER_DATA007 file type must be exactly four ASCII characters")
        compressed = zlib.compress(self.data)
        header = struct.pack(
            "<4siii",
            self.file_type.encode("ascii"),
            USER_DATA_FILE_SENTINEL,
            len(compressed),
            len(self.data),
        )
        return header + compressed

    @classmethod
    def create(cls, file_type: str, data: bytes) -> "UserDataFile":
        if len(file_type) != 4:
            raise EmblemFormatError("USER_DATA007 file type must be exactly four ASCII characters")
        return cls(file_type=file_type, data=bytes(data))

    @classmethod
    def deserialize(cls, cursor: _Cursor) -> "UserDataFile":
        magic, sentinel, deflated_size, inflated_size = cursor.read_struct("<4siii")
        if sentinel != USER_DATA_FILE_SENTINEL:
            raise EmblemFormatError(f"Unexpected USER_DATA007 file sentinel: {sentinel:#x}")
        compressed = cursor.read(deflated_size)
        data = zlib.decompress(compressed)
        if len(data) != inflated_size:
            raise EmblemFormatError(
                f"Inflated payload length mismatch: expected {inflated_size}, got {len(data)}"
            )
        return cls(file_type=magic.decode("ascii"), data=data)


@dataclass(frozen=True)
class UserDataContainer:
    iv: bytes
    unk1: int
    unk2: int
    unk3: int
    unk4: int
    files: tuple[UserDataFile, ...] = ()
    extra_files: tuple[UserDataFile, ...] = ()

    @classmethod
    def empty(cls, *, iv: bytes | None = None) -> "UserDataContainer":
        iv_bytes = iv if iv is not None else (b"\x00" * 16)
        if len(iv_bytes) != 16:
            raise EmblemFormatError("USER_DATA007 IV must be 16 bytes")
        return cls(iv=bytes(iv_bytes), unk1=0, unk2=0, unk3=0, unk4=0)

    def clone(self) -> "UserDataContainer":
        return replace(self)

    def with_files(
        self,
        *,
        files: Sequence[UserDataFile] | None = None,
        extra_files: Sequence[UserDataFile] | None = None,
    ) -> "UserDataContainer":
        return replace(
            self,
            files=tuple(self.files if files is None else files),
            extra_files=tuple(self.extra_files if extra_files is None else extra_files),
        )

    def insert_user_file(self, user_file: UserDataFile) -> "UserDataContainer":
        return self.with_files(files=(*self.files, user_file))

    def serialize_plaintext(self) -> bytes:
        if len(self.iv) != 16:
            raise EmblemFormatError("USER_DATA007 IV must be 16 bytes")
        body = bytearray()
        for user_file in self.files:
            body.extend(user_file.serialize())
        body.extend(struct.pack("<i", len(self.extra_files)))
        for extra_file in self.extra_files:
            body.extend(extra_file.serialize())
        header_rest = struct.pack(
            "<iiiii",
            self.unk1,
            self.unk2,
            self.unk3,
            self.unk4,
            len(self.files),
        )
        size_field = len(self.iv) + len(header_rest) + len(body)
        checksum = hashlib.md5(header_rest + body).digest()
        plaintext = bytearray(self.iv)
        plaintext.extend(struct.pack("<i", size_field))
        plaintext.extend(header_rest)
        plaintext.extend(body)
        plaintext.extend(checksum)
        while len(plaintext) % 16:
            plaintext.append(0)
        return bytes(plaintext)

    def serialize_encrypted(self, *, key: bytes = USER_DATA007_AES_KEY) -> bytes:
        plaintext = bytearray(self.serialize_plaintext())
        if len(key) != 16:
            raise EmblemFormatError("AES key must be 16 bytes for USER_DATA007")
        if (len(plaintext) - 16) % 16:
            raise EmblemFormatError("Encrypted USER_DATA007 payload must align to AES block size")
        cipher = Cipher(algorithms.AES(key), modes.CBC(bytes(self.iv)))
        encryptor = cipher.encryptor()
        ciphertext = encryptor.update(bytes(plaintext[16:])) + encryptor.finalize()
        plaintext[16:] = ciphertext
        return bytes(plaintext)

    @classmethod
    def deserialize_plaintext(cls, payload: bytes, *, validate_checksum: bool = True) -> "UserDataContainer":
        if len(payload) < 40:
            raise EmblemFormatError("USER_DATA007 payload is too small")
        cursor = _Cursor(payload)
        iv = cursor.read(16)
        (size_field, unk1, unk2, unk3, unk4, file_count) = cursor.read_struct("<iiiiii")
        files = tuple(UserDataFile.deserialize(cursor) for _ in range(file_count))
        (extra_count,) = cursor.read_struct("<i")
        extra_files = tuple(UserDataFile.deserialize(cursor) for _ in range(extra_count))
        checksum_offset = 4 + size_field
        if cursor.tell() > checksum_offset:
            raise EmblemFormatError("USER_DATA007 payload overran checksum boundary")
        if checksum_offset + 16 > len(payload):
            raise EmblemFormatError("USER_DATA007 header.size points beyond the payload boundary")
        expected_checksum = payload[checksum_offset : checksum_offset + 16]
        if validate_checksum:
            observed_checksum = hashlib.md5(payload[20:checksum_offset]).digest()
            if observed_checksum != expected_checksum:
                raise EmblemFormatError(
                    "USER_DATA007 checksum mismatch; fail-closed before using emblem data"
                )
        return cls(
            iv=iv,
            unk1=unk1,
            unk2=unk2,
            unk3=unk3,
            unk4=unk4,
            files=files,
            extra_files=extra_files,
        )

    @classmethod
    def deserialize_encrypted(cls, payload: bytes, *, key: bytes = USER_DATA007_AES_KEY) -> "UserDataContainer":
        if len(payload) < 16:
            raise EmblemFormatError("Encrypted USER_DATA007 payload is too small")
        if len(key) != 16:
            raise EmblemFormatError("AES key must be 16 bytes for USER_DATA007")
        if (len(payload) - 16) % 16:
            raise EmblemFormatError("Encrypted USER_DATA007 payload length must align to AES-CBC blocks")
        iv = payload[:16]
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        decryptor = cipher.decryptor()
        plaintext_tail = decryptor.update(payload[16:]) + decryptor.finalize()
        return cls.deserialize_plaintext(iv + plaintext_tail)


@dataclass(frozen=True)
class EmblemSelection:
    file: UserDataFile
    record: EmblemRecord
    source_bucket: str
    file_index: int



def iter_emblem_selections(container: UserDataContainer) -> Iterable[EmblemSelection]:
    for bucket_name, files in (("files", container.files), ("extraFiles", container.extra_files)):
        for index, user_file in enumerate(files):
            if user_file.file_type != "EMBC":
                continue
            provenance = EmblemProvenance(
                source_bucket=bucket_name,
                file_index=index,
                file_type=user_file.file_type,
            )
            yield EmblemSelection(
                file=user_file,
                record=EmblemRecord.deserialize(user_file.data, provenance=provenance),
                source_bucket=bucket_name,
                file_index=index,
            )



def _serialize_block_container(blocks: Sequence[tuple[str, bytes]]) -> bytes:
    buffer = bytearray()
    buffer.extend(_fixed_cstring("---- begin ----"))
    buffer.extend(struct.pack("<IIQ", 0, 0, 0))
    for name, payload in blocks:
        buffer.extend(_fixed_cstring(name))
        buffer.extend(struct.pack("<IIQ", len(payload), 0, 0))
        buffer.extend(payload)
    buffer.extend(_fixed_cstring("----  end  ----"))
    buffer.extend(struct.pack("<IIQ", 0, 0, 0))
    return bytes(buffer)



def _deserialize_block_container(payload: bytes) -> tuple[tuple[str, bytes], ...]:
    cursor = _Cursor(payload)
    blocks: list[tuple[str, bytes]] = []
    first_block = True
    while cursor.remaining() > 0:
        name = _read_fixed_cstring(cursor)
        size, unk, pad = cursor.read_struct("<IIQ")
        if unk != 0 or pad != 0:
            raise EmblemFormatError(f"Unexpected block control fields for {name!r}")
        if first_block:
            if name != "---- begin ----" or size != 0:
                raise EmblemFormatError("EMBC block container missing begin delimiter")
            first_block = False
            continue
        if name == "----  end  ----":
            if size != 0:
                raise EmblemFormatError("EMBC end delimiter must have zero payload size")
            break
        blocks.append((name, cursor.read(size)))
    else:
        raise EmblemFormatError("EMBC block container missing end delimiter")
    if cursor.remaining() != 0:
        raise EmblemFormatError("Trailing bytes remained after EMBC block parse")
    return tuple(blocks)
