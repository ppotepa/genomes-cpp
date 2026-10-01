#pragma once

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/world_core/WorldPosition.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace genomes::world_core {

struct Aabb3 final {
    foundation::Vec3 minimum{};
    foundation::Vec3 maximum{};

    [[nodiscard]] bool valid() const noexcept {
        return std::isfinite(minimum.x) && std::isfinite(minimum.y) &&
               std::isfinite(minimum.z) && std::isfinite(maximum.x) &&
               std::isfinite(maximum.y) && std::isfinite(maximum.z) &&
               minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z;
    }

    [[nodiscard]] foundation::Vec3 extent() const noexcept {
        return {maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z};
    }

    [[nodiscard]] bool overlaps(const Aabb3& other, float margin = 0.0F) const noexcept {
        return minimum.x <= other.maximum.x + margin &&
               maximum.x + margin >= other.minimum.x &&
               minimum.y <= other.maximum.y + margin &&
               maximum.y + margin >= other.minimum.y &&
               minimum.z <= other.maximum.z + margin &&
               maximum.z + margin >= other.minimum.z;
    }

    [[nodiscard]] Aabb3 united(const Aabb3& other) const noexcept {
        return {{std::min(minimum.x, other.minimum.x),
                 std::min(minimum.y, other.minimum.y),
                 std::min(minimum.z, other.minimum.z)},
                {std::max(maximum.x, other.maximum.x),
                 std::max(maximum.y, other.maximum.y),
                 std::max(maximum.z, other.maximum.z)}};
    }
};

enum class DirtyReason : std::uint32_t {
    None = 0,
    StructuralGeometry = 1U << 0U,
    RubbleSurface = 1U << 1U,
    Traversability = 1U << 2U,
    Cover = 1U << 3U,
    StaticCollision = 1U << 4U,
    Presentation = 1U << 5U,
};

using DirtyReasonMask = std::uint32_t;

[[nodiscard]] constexpr DirtyReasonMask operator|(DirtyReason left,
                                                    DirtyReason right) noexcept {
    return static_cast<DirtyReasonMask>(left) | static_cast<DirtyReasonMask>(right);
}

[[nodiscard]] constexpr DirtyReasonMask operator|(DirtyReasonMask left,
                                                    DirtyReason right) noexcept {
    return left | static_cast<DirtyReasonMask>(right);
}

[[nodiscard]] constexpr bool hasReason(DirtyReasonMask mask, DirtyReason reason) noexcept {
    return (mask & static_cast<DirtyReasonMask>(reason)) != 0U;
}

struct DirtyBounds final {
    Aabb3 world_aabb{};
    DirtyReasonMask reasons{0};
    WorldId world{};
    RegionCoord region{};
    RegionId region_id{};
    foundation::StableId source_id{0};
    std::uint64_t semantic_revision{0};

    [[nodiscard]] bool valid() const noexcept {
        return world_aabb.valid() && reasons != 0U && world.isValid() && region_id.isValid() &&
               source_id != 0 && semantic_revision != 0;
    }
};

} // namespace genomes::world_core
