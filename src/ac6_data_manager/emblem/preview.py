from __future__ import annotations

from dataclasses import dataclass
from hashlib import sha256
from math import cos, radians, sin

from PIL import Image, ImageDraw

from .binary import EmblemGroup, EmblemImage, EmblemRecord, GroupData
from .models import PreviewHandle, PreviewKind


SQUARE_SOLID_DECAL = 100
ELLIPSE_SOLID_DECAL = 102
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass(frozen=True)
class PreviewWarning:
    decal_id: int
    provenance: str
    message: str


@dataclass(frozen=True)
class PreviewRenderResult:
    image: Image.Image
    placeholder_count: int
    provenance: tuple[str, ...]
    warnings: tuple[PreviewWarning, ...]



def extract_preview_handle(payload: bytes | None) -> PreviewHandle:
    if not payload:
        return PreviewHandle(kind=PreviewKind.NONE, sha256=None, media_type=None, size_bytes=0)
    kind = PreviewKind.PNG if payload.startswith(PNG_SIGNATURE) else PreviewKind.BINARY
    media_type = "image/png" if kind is PreviewKind.PNG else "application/octet-stream"
    return PreviewHandle(
        kind=kind,
        sha256=sha256(payload).hexdigest(),
        media_type=media_type,
        size_bytes=len(payload),
    )


class EmblemPreviewRenderer:
    def __init__(self, *, canvas_size: int = 256) -> None:
        self.canvas_size = canvas_size

    def render(self, record_or_image: EmblemRecord | EmblemImage) -> PreviewRenderResult:
        image = record_or_image.image if isinstance(record_or_image, EmblemRecord) else record_or_image
        canvas = Image.new("RGBA", (self.canvas_size, self.canvas_size), (0, 0, 0, 0))
        warnings: list[PreviewWarning] = []
        provenance = ["pil-basic-shapes-v1"]
        placeholder_count = 0
        for layer_index, layer in enumerate(image.layers):
            placeholder_count += self._render_group(
                canvas,
                layer.group,
                provenance_prefix=f"layer[{layer_index}]",
                warnings=warnings,
            )
        return PreviewRenderResult(
            image=canvas,
            placeholder_count=placeholder_count,
            provenance=tuple(provenance),
            warnings=tuple(warnings),
        )

    def _render_group(
        self,
        canvas: Image.Image,
        group: EmblemGroup,
        *,
        provenance_prefix: str,
        warnings: list[PreviewWarning],
    ) -> int:
        if group.children:
            placeholders = 0
            for child_index, child in enumerate(group.children):
                placeholders += self._render_group(
                    canvas,
                    child,
                    provenance_prefix=f"{provenance_prefix}/child[{child_index}]",
                    warnings=warnings,
                )
            return placeholders
        return self._render_leaf(canvas, group.data, provenance_prefix=provenance_prefix, warnings=warnings)

    def _render_leaf(
        self,
        canvas: Image.Image,
        data: GroupData,
        *,
        provenance_prefix: str,
        warnings: list[PreviewWarning],
    ) -> int:
        base_decal = data.base_decal_id
        temp = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(temp)
        bounds = self._bounds(data)
        fill = data.rgba
        if base_decal == SQUARE_SOLID_DECAL:
            self._draw_rotated_rectangle(draw, bounds, data.angle, fill)
        elif base_decal == ELLIPSE_SOLID_DECAL:
            draw.ellipse(bounds, fill=fill)
            if data.angle % 360:
                temp = temp.rotate(
                    data.angle,
                    resample=Image.Resampling.BICUBIC,
                    center=self._center(data),
                )
        else:
            draw.rectangle(bounds, outline=(180, 180, 180, 255), fill=(70, 70, 70, 80), width=2)
            draw.line((bounds[0], bounds[1], bounds[2], bounds[3]), fill=(180, 180, 180, 255), width=2)
            draw.line((bounds[0], bounds[3], bounds[2], bounds[1]), fill=(180, 180, 180, 255), width=2)
            warnings.append(
                PreviewWarning(
                    decal_id=base_decal,
                    provenance=f"{provenance_prefix}/decal[{base_decal}]",
                    message="Unsupported decal rendered as placeholder",
                )
            )
            canvas.alpha_composite(temp)
            return 1
        canvas.alpha_composite(temp)
        return 0

    def _center(self, data: GroupData) -> tuple[float, float]:
        return (
            self.canvas_size / 2 + (data.pos_x / 16.0),
            self.canvas_size / 2 + (data.pos_y / 16.0),
        )

    def _bounds(self, data: GroupData) -> tuple[float, float, float, float]:
        center_x, center_y = self._center(data)
        half_width = max(2.0, abs(data.scale_x) / 16.0)
        half_height = max(2.0, abs(data.scale_y) / 16.0)
        return (
            center_x - half_width,
            center_y - half_height,
            center_x + half_width,
            center_y + half_height,
        )

    def _draw_rotated_rectangle(
        self,
        draw: ImageDraw.ImageDraw,
        bounds: tuple[float, float, float, float],
        angle: int,
        fill: tuple[int, int, int, int],
    ) -> None:
        x0, y0, x1, y1 = bounds
        corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
        if angle % 360 == 0:
            draw.rectangle(bounds, fill=fill)
            return
        cx = (x0 + x1) / 2.0
        cy = (y0 + y1) / 2.0
        theta = radians(angle)
        rotated = [self._rotate_point(point, (cx, cy), theta) for point in corners]
        draw.polygon(rotated, fill=fill)

    @staticmethod
    def _rotate_point(
        point: tuple[float, float],
        origin: tuple[float, float],
        angle_radians: float,
    ) -> tuple[float, float]:
        px, py = point
        ox, oy = origin
        dx = px - ox
        dy = py - oy
        return (
            ox + dx * cos(angle_radians) - dy * sin(angle_radians),
            oy + dx * sin(angle_radians) + dy * cos(angle_radians),
        )
