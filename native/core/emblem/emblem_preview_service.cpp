#include "emblem_preview_service.hpp"

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <array>
#include <set>
#include <cmath>
#include <cstdint>
#include <functional>

namespace ac6dm::emblem {
namespace {

constexpr std::int16_t kInvertedDecalBit = 0x4000;
enum class RegisteredShape {
    Rectangle,
    Ellipse,
};

struct DecalRegistryEntry final {
    std::int16_t decalId;
    RegisteredShape shape;
};

constexpr std::array<DecalRegistryEntry, 2> kDecalRegistry{{
    {100, RegisteredShape::Rectangle},
    {102, RegisteredShape::Ellipse},
}};

std::optional<RegisteredShape> resolveRegisteredShape(const std::int16_t decalId) {
    for (const auto& entry : kDecalRegistry) {
        if (entry.decalId == decalId) {
            return entry.shape;
        }
    }
    return std::nullopt;
}

void collectUnsupportedDecals(const EmblemGroup& group, std::set<std::int16_t>& unsupported) {
    if (!group.children.empty()) {
        for (const auto& child : group.children) {
            collectUnsupportedDecals(child, unsupported);
        }
        return;
    }

    const auto baseDecal = static_cast<std::int16_t>(group.data.decalId & ~kInvertedDecalBit);
    if (!resolveRegisteredShape(baseDecal).has_value()) {
        unsupported.insert(baseDecal);
    }
}

std::optional<std::string> buildUnsupportedDecalNote(const EmblemRecord& record) {
    std::set<std::int16_t> unsupported;
    for (const auto& layer : record.image.layers) {
        collectUnsupportedDecals(layer.group, unsupported);
    }
    if (unsupported.empty()) {
        return std::nullopt;
    }

    std::string note = "Unsupported decal IDs rendered as placeholders:";
    bool first = true;
    for (const auto decalId : unsupported) {
        note += first ? " " : ", ";
        note += std::to_string(decalId);
        first = false;
    }
    note += ". Full-fidelity emblem preview still requires a decal-id-to-shape mapping.";
    return note;
}

}  // namespace

QImage renderEmblemPreviewImage(const EmblemRecord& record) {
    QImage canvas(256, 256, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    std::function<void(const EmblemGroup&)> renderGroup = [&](const EmblemGroup& group) {
        if (!group.children.empty()) {
            for (const auto& child : group.children) {
                renderGroup(child);
            }
            return;
        }

        const auto baseDecal = static_cast<std::int16_t>(group.data.decalId & ~kInvertedDecalBit);
        const QPointF center(128.0 + (group.data.posX / 16.0), 128.0 + (group.data.posY / 16.0));
        const QSizeF size(std::max(2.0, std::abs(group.data.scaleX) / 16.0),
                          std::max(2.0, std::abs(group.data.scaleY) / 16.0));
        const QRectF bounds(center.x() - size.width(), center.y() - size.height(), size.width() * 2.0, size.height() * 2.0);
        const auto registeredShape = resolveRegisteredShape(baseDecal);
        painter.save();
        painter.translate(center);
        painter.rotate(group.data.angle);
        painter.translate(-center);
        const QColor fill(group.data.red, group.data.green, group.data.blue, group.data.alpha);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        if (registeredShape == RegisteredShape::Rectangle) {
            painter.drawRect(bounds);
        } else if (registeredShape == RegisteredShape::Ellipse) {
            painter.drawEllipse(bounds);
        } else {
            painter.setPen(QPen(QColor(180, 180, 180, 255), 2.0));
            painter.setBrush(QColor(70, 70, 70, 80));
            painter.drawRect(bounds);
            painter.drawLine(bounds.topLeft(), bounds.bottomRight());
            painter.drawLine(bounds.bottomLeft(), bounds.topRight());
        }
        painter.restore();
    };

    for (const auto& layer : record.image.layers) {
        renderGroup(layer.group);
    }
    painter.end();
    return canvas;
}

std::vector<std::uint8_t> renderEmblemPreviewPng(const EmblemRecord& record) {
    const auto image = renderEmblemPreviewImage(record);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

preview::PreviewResolution buildEmblemPreview(const EmblemRecord& record) {
    const auto bytes = renderEmblemPreviewPng(record);
    if (bytes.empty()) {
        return preview::makeUnavailablePreview(
            "emblem.qt-basic-solids-only-v1",
            "No Preview");
    }
    const auto unsupportedNote = buildUnsupportedDecalNote(record);
    return preview::makeDerivedRenderPreview(
        bytes,
        "emblem.qt-basic-solids-only-v1",
        "rendered-from-emblem-record",
        unsupportedNote,
        "image/png");
}

}  // namespace ac6dm::emblem
