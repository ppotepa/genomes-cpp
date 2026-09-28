#pragma once

#include <genomes/foundation/Types.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::render {

// Render-facing vertex/material colors use linear-light float components. UI
// and serialized palette values may be authored in sRGB and must cross this
// boundary explicitly; keeping the conversion here prevents shader-specific
// accidental mixing of the two spaces.
[[nodiscard]] inline float srgbToLinear(float value) noexcept {
    value = std::clamp(value, 0.0F, 1.0F);
    return value <= 0.04045F ? value / 12.92F
                             : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] inline float linearToSrgb(float value) noexcept {
    value = std::clamp(value, 0.0F, 1.0F);
    return value <= 0.0031308F ? value * 12.92F
                               : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

[[nodiscard]] inline foundation::Color srgbToLinear(foundation::Color value) noexcept {
    return {srgbToLinear(value.r), srgbToLinear(value.g), srgbToLinear(value.b), value.a};
}

[[nodiscard]] inline foundation::Color linearToSrgb(foundation::Color value) noexcept {
    return {linearToSrgb(value.r), linearToSrgb(value.g), linearToSrgb(value.b), value.a};
}

} // namespace genomes::render
