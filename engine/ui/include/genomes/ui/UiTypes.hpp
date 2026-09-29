#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace genomes::ui {

enum class UiWidgetType : std::uint8_t {
    Panel,
    Label,
    Button,
    Separator
};

struct UiWidget final {
    foundation::StableId id{0};
    UiWidgetType type{UiWidgetType::Label};
    std::string text;
    bool enabled{true};
    bool selected{false};
    float width{0.0F};
    float height{0.0F};
};

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
    // Transitional extraction data used by the software glyph fallback. New
    // RML renderers should consume commands only.
    std::vector<UiWidget> widgets;
    std::uint32_t viewport_width{0};
    std::uint32_t viewport_height{0};
    std::uint64_t revision{0};

    void clear() {
        commands.clear();
        ++revision;
    }
};

} // namespace genomes::ui
