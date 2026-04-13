#!/usr/bin/env python3
"""Hide arbitrary files inside PNG ancillary chunks and extract them back."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path
from typing import Iterable, NamedTuple

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
CHUNK_TYPE = b"stEg"
PAYLOAD_MAGIC = b"ACV1"


class PngChunk(NamedTuple):
    raw: bytes
    chunk_type: bytes
    data: bytes


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pack_chunk(chunk_type: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def iter_chunks(png_bytes: bytes) -> Iterable[PngChunk]:
    if not png_bytes.startswith(PNG_SIGNATURE):
        raise ValueError("Input is not a PNG file")

    offset = len(PNG_SIGNATURE)
    while offset + 12 <= len(png_bytes):
        length = struct.unpack(">I", png_bytes[offset : offset + 4])[0]
        chunk_type = png_bytes[offset + 4 : offset + 8]
        data_start = offset + 8
        data_end = data_start + length
        crc_end = data_end + 4
        if crc_end > len(png_bytes):
            raise ValueError("PNG chunk exceeds file length")

        data = png_bytes[data_start:data_end]
        crc_read = struct.unpack(">I", png_bytes[data_end:crc_end])[0]
        crc_calc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
        if crc_read != crc_calc:
            raise ValueError(f"CRC mismatch for chunk {chunk_type!r}")

        yield PngChunk(png_bytes[offset:crc_end], chunk_type, data)
        offset = crc_end
        if chunk_type == b"IEND":
            break
    else:
        raise ValueError("PNG IEND chunk not found")


def build_payload_blob(payload_path: Path) -> tuple[dict[str, object], bytes]:
    payload_bytes = payload_path.read_bytes()
    meta = {
        "filename": payload_path.name,
        "size": len(payload_bytes),
        "sha256": sha256_hex(payload_bytes),
    }
    meta_bytes = json.dumps(meta, ensure_ascii=True, separators=(",", ":")).encode("utf-8")
    blob = PAYLOAD_MAGIC + struct.pack(">I", len(meta_bytes)) + meta_bytes + payload_bytes
    return meta, blob


def parse_payload_blob(blob: bytes) -> tuple[dict[str, object], bytes]:
    if not blob.startswith(PAYLOAD_MAGIC):
        raise ValueError("Invalid hidden payload header")
    if len(blob) < 8:
        raise ValueError("Hidden payload header is truncated")

    meta_len = struct.unpack(">I", blob[4:8])[0]
    meta_end = 8 + meta_len
    if meta_end > len(blob):
        raise ValueError("Hidden payload metadata is truncated")

    meta = json.loads(blob[8:meta_end].decode("utf-8"))
    payload = blob[meta_end:]
    if len(payload) != meta["size"]:
        raise ValueError("Hidden payload size does not match metadata")
    if sha256_hex(payload) != meta["sha256"]:
        raise ValueError("Hidden payload checksum does not match metadata")
    return meta, payload


def embed_payload(cover_png: Path, payload_path: Path, output_png: Path) -> dict[str, object]:
    png_bytes = cover_png.read_bytes()
    meta, hidden_blob = build_payload_blob(payload_path)

    output = bytearray(PNG_SIGNATURE)
    inserted = False
    for chunk in iter_chunks(png_bytes):
        if chunk.chunk_type == CHUNK_TYPE:
            continue
        if chunk.chunk_type == b"IEND" and not inserted:
            output.extend(pack_chunk(CHUNK_TYPE, hidden_blob))
            inserted = True
        output.extend(chunk.raw)

    if not inserted:
        raise ValueError("Failed to insert hidden chunk before IEND")

    output_png.parent.mkdir(parents=True, exist_ok=True)
    output_png.write_bytes(output)

    return {
        "cover": cover_png.name,
        "output": output_png.name,
        "payload": meta["filename"],
        "payload_size": meta["size"],
        "payload_sha256": meta["sha256"],
        "output_size": output_png.stat().st_size,
    }


def extract_payload(steg_png: Path, output_dir: Path) -> dict[str, object]:
    png_bytes = steg_png.read_bytes()
    for chunk in iter_chunks(png_bytes):
        if chunk.chunk_type != CHUNK_TYPE:
            continue

        meta, payload = parse_payload_blob(chunk.data)
        output_dir.mkdir(parents=True, exist_ok=True)
        target_path = output_dir / meta["filename"]
        target_path.write_bytes(payload)
        return {
            "source": steg_png.name,
            "extracted": target_path.name,
            "size": len(payload),
            "sha256": meta["sha256"],
        }

    raise ValueError(f"No hidden chunk {CHUNK_TYPE!r} found in {steg_png}")


def create_cover(output_path: Path, index: int, size: tuple[int, int] = (960, 960)) -> None:
    width, height = size
    gradient_x = Image.linear_gradient("L").resize(size)
    gradient_y = Image.linear_gradient("L").rotate(90).resize(size)
    radial = Image.radial_gradient("L").resize(size)

    if index == 0:
        red = gradient_x
        green = ImageChops.screen(gradient_y, radial)
        blue = ImageChops.multiply(ImageOps.invert(radial), gradient_x)
        accent = (247, 232, 205, 92)
    elif index == 1:
        red = ImageChops.screen(radial, gradient_y)
        green = gradient_x
        blue = ImageOps.invert(gradient_y)
        accent = (217, 245, 255, 84)
    else:
        red = ImageOps.invert(gradient_x)
        green = ImageChops.multiply(gradient_x, radial)
        blue = ImageChops.screen(gradient_y, radial)
        accent = (255, 214, 162, 88)

    image = Image.merge("RGB", (red, green, blue))
    draw = ImageDraw.Draw(image, "RGBA")

    draw.rounded_rectangle((80, 80, width - 80, height - 80), radius=64, outline=(255, 255, 255, 90), width=6)
    draw.ellipse((120, 120, width - 120, height - 120), outline=(255, 255, 255, 55), width=4)
    draw.polygon(
        [
            (width * 0.22, height * 0.70),
            (width * 0.50, height * 0.16),
            (width * 0.78, height * 0.70),
            (width * 0.60, height * 0.60),
            (width * 0.40, height * 0.60),
        ],
        fill=accent,
    )
    draw.rectangle((140, height - 170, width - 140, height - 120), fill=(255, 255, 255, 45))

    blurred = image.filter(ImageFilter.GaussianBlur(radius=1.5))
    final = Image.blend(image, blurred, alpha=0.18)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    final.save(output_path, format="PNG", optimize=True, compress_level=9)


def command_batch(payloads: list[Path], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    cover_dir = output_dir / "covers"
    extract_dir = output_dir / "extracted_check"
    manifest_path = output_dir / "manifest.json"
    manifest: list[dict[str, object]] = []

    for index, payload_path in enumerate(payloads, start=1):
        cover_path = cover_dir / f"cover_{index:02d}.png"
        steg_path = output_dir / f"acvi_hidden_{index:02d}.png"
        create_cover(cover_path, index - 1)
        record = embed_payload(cover_path, payload_path, steg_path)
        record["cover_size"] = cover_path.stat().st_size
        manifest.append(record)

    verification: list[dict[str, object]] = []
    for index in range(1, len(payloads) + 1):
        verification.append(extract_payload(output_dir / f"acvi_hidden_{index:02d}.png", extract_dir))

    manifest_doc = {
        "chunk_type": CHUNK_TYPE.decode("ascii"),
        "payload_header_magic": PAYLOAD_MAGIC.decode("ascii"),
        "items": manifest,
        "verification": verification,
    }
    manifest_path.write_text(json.dumps(manifest_doc, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    make_cover_parser = subparsers.add_parser("make-covers", help="Generate visual cover PNG files")
    make_cover_parser.add_argument("--output-dir", type=Path, required=True)
    make_cover_parser.add_argument("--count", type=int, default=3)

    embed_parser = subparsers.add_parser("embed", help="Embed a payload into a cover PNG")
    embed_parser.add_argument("--cover", type=Path, required=True)
    embed_parser.add_argument("--payload", type=Path, required=True)
    embed_parser.add_argument("--output", type=Path, required=True)

    extract_parser = subparsers.add_parser("extract", help="Extract a hidden payload from a PNG")
    extract_parser.add_argument("--input", type=Path, required=True)
    extract_parser.add_argument("--output-dir", type=Path, required=True)

    batch_parser = subparsers.add_parser("batch", help="Generate covers, embed payloads, and verify extraction")
    batch_parser.add_argument("--output-dir", type=Path, required=True)
    batch_parser.add_argument("payloads", nargs="+", type=Path)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "make-covers":
        for index in range(args.count):
            create_cover(args.output_dir / f"cover_{index + 1:02d}.png", index)
        return

    if args.command == "embed":
        result = embed_payload(args.cover, args.payload, args.output)
        print(json.dumps(result, ensure_ascii=True, indent=2))
        return

    if args.command == "extract":
        result = extract_payload(args.input, args.output_dir)
        print(json.dumps(result, ensure_ascii=True, indent=2))
        return

    if args.command == "batch":
        command_batch(args.payloads, args.output_dir)
        print(json.dumps({"output_dir": str(args.output_dir), "items": len(args.payloads)}, ensure_ascii=True, indent=2))
        return

    raise SystemExit("Unsupported command")


if __name__ == "__main__":
    main()
