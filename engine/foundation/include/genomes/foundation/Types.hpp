#pragma once

#include <cstdint>
#include <genomes/math/Vec.hpp>
#include <string_view>

namespace genomes::foundation {

using StableId = std::uint64_t;
using SceneId = StableId;

using Vec2 = math::Vec2;
using Vec3 = math::Vec3;

struct Color {
    float r{1.0F};
    float g{1.0F};
    float b{1.0F};
    float a{1.0F};
};

// Stable IDs are used for commands, scene entries and generated objects. They
// are deliberately independent of pointer addresses and container order.
constexpr StableId stable_id(std::string_view value) noexcept {
    StableId hash = 14695981039346656037ull;
    for (const unsigned char character : value) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

constexpr SceneId scene_id(std::string_view value) noexcept {
    return stable_id(value);
}

} // namespace genomes::foundation
