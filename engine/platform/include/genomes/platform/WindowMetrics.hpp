#pragma once

#include <genomes/foundation/Types.hpp>

#include <algorithm>
#include <cstdint>

namespace genomes::platform {

// Input positions from SDL are window coordinates; presentation uses pixels.
// Keep both dimensions even when a minimized window is temporarily not drawable.
struct WindowMetrics final {
    std::int32_t logical_width{0};
    std::int32_t logical_height{0};
    std::int32_t pixel_width{0};
    std::int32_t pixel_height{0};
    bool minimized{false};

    [[nodiscard]] bool drawable() const noexcept {
        return !minimized && logical_width > 0 && logical_height > 0 &&
               pixel_width > 0 && pixel_height > 0;
    }
    [[nodiscard]] float scale_x() const noexcept {
        return logical_width > 0 && pixel_width > 0
            ? static_cast<float>(pixel_width) / static_cast<float>(logical_width) : 1.0F;
    }
    [[nodiscard]] float scale_y() const noexcept {
        return logical_height > 0 && pixel_height > 0
            ? static_cast<float>(pixel_height) / static_cast<float>(logical_height) : 1.0F;
    }
    [[nodiscard]] foundation::Vec2 to_pixels(float x, float y) const noexcept {
        return {x * scale_x(), y * scale_y()};
    }
    [[nodiscard]] bool operator==(const WindowMetrics&) const noexcept = default;
};

} // namespace genomes::platform
