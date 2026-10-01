#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace genomes::ui {

enum class UiDrawPrimitive : std::uint8_t {
    Quad,
    Text,
    Separator
};

struct UiRect final {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
};

struct UiGeometryVertex final {
    float x{0.0F};
    float y{0.0F};
    float u{0.0F};
    float v{0.0F};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
};

struct UiTexture final {
    std::uint64_t id{0};
    // Producers increment this only when the pixel payload changes. Backends
    // cache by (id, content_revision), avoiding a full atlas hash every frame.
    std::uint64_t content_revision{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba;
};

struct UiDrawCommand final {
    foundation::StableId id{0};
    UiDrawPrimitive primitive{UiDrawPrimitive::Text};
    UiRect rect{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
    std::string text;
    bool enabled{true};
    bool selected{false};
    std::int32_t layer{0};
    std::vector<UiGeometryVertex> vertices;
    std::vector<std::uint32_t> indices;
    UiRect scissor{};
    bool scissor_enabled{false};
    std::uint64_t texture{0};
};

struct UiRenderFrame final {
    std::vector<UiDrawCommand> commands;
    std::vector<UiTexture> textures;
    std::uint32_t viewport_width{0};
    std::uint32_t viewport_height{0};
    std::uint64_t revision{0};

    void clear() {
        commands.clear();
        ++revision;
    }
};

[[nodiscard]] inline bool valid_ui_frame(const UiRenderFrame& frame,
                                         bool require_visible = true) noexcept {
    if (frame.viewport_width == 0U || frame.viewport_height == 0U) return false;
    const float width = static_cast<float>(frame.viewport_width);
    const float height = static_cast<float>(frame.viewport_height);
    bool has_visible_geometry = false;
    for (const auto& command : frame.commands) {
        const auto finite_rect = [](const UiRect& rect) {
            return std::isfinite(rect.x) && std::isfinite(rect.y) &&
                   std::isfinite(rect.width) && std::isfinite(rect.height);
        };
        if (!finite_rect(command.rect) || !std::isfinite(command.color.a)) return false;
        if (command.indices.size() % 3U != 0U) return false;
        for (const auto index : command.indices) if (index >= command.vertices.size()) return false;
        bool visible_alpha = command.color.a > 0.0F;
        float min_x = command.rect.x, min_y = command.rect.y;
        float max_x = command.rect.x + command.rect.width;
        float max_y = command.rect.y + command.rect.height;
        if (!command.vertices.empty()) {
            min_x = min_y = std::numeric_limits<float>::max();
            max_x = max_y = std::numeric_limits<float>::lowest();
            visible_alpha = false;
            for (const auto& vertex : command.vertices) {
                if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
                    !std::isfinite(vertex.u) || !std::isfinite(vertex.v) ||
                    !std::isfinite(vertex.color.a)) return false;
                min_x = std::min(min_x, vertex.x); min_y = std::min(min_y, vertex.y);
                max_x = std::max(max_x, vertex.x); max_y = std::max(max_y, vertex.y);
                visible_alpha = visible_alpha || vertex.color.a > 0.0F;
            }
        }
        bool intersects_frame = max_x > 0.0F && max_y > 0.0F && min_x < width && min_y < height;
        if (command.scissor_enabled) {
            if (!finite_rect(command.scissor) || command.scissor.width < 0.0F ||
                command.scissor.height < 0.0F) return false;
            intersects_frame = intersects_frame &&
                command.scissor.x + command.scissor.width > 0.0F &&
                command.scissor.y + command.scissor.height > 0.0F &&
                command.scissor.x < width && command.scissor.y < height;
        }
        const bool full_white_quad = command.vertices.empty() &&
            command.rect.width >= width * 0.95F && command.rect.height >= height * 0.95F &&
            command.color.r > 0.98F && command.color.g > 0.98F &&
            command.color.b > 0.98F && command.color.a > 0.98F;
        if (full_white_quad) return false;
        has_visible_geometry = has_visible_geometry || (visible_alpha && intersects_frame);
    }
    return !require_visible || has_visible_geometry;
}

} // namespace genomes::ui
