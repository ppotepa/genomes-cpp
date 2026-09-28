#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace genomes::infantry {

struct GroundSample final {
    float height{0.0F};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
};

using GroundSampleFunction = bool (*)(void* context,
                                       foundation::Vec3 world_position,
                                       GroundSample& output) noexcept;

struct GroundSurfaceQuery final {
    std::uint64_t revision{0};
    void* context{nullptr};
    GroundSampleFunction sample{nullptr};

    [[nodiscard]] bool valid() const noexcept { return sample != nullptr; }
};

struct GroundSupportPoint final {
    foundation::Vec3 bind_position{};
    foundation::Vec3 target_position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    bool valid{false};
};

struct GroundContactInput final {
    foundation::Vec3 hips{};
    foundation::Vec3 left_foot{};
    foundation::Vec3 right_foot{};
    foundation::Vec3 left_pole{-1.0F, 0.0F, 0.0F};
    foundation::Vec3 right_pole{1.0F, 0.0F, 0.0F};
    float upper_leg_length{0.45F};
    float lower_leg_length{0.45F};
    float sole_offset{0.02F};
    std::array<foundation::Vec3, 4U> gear_supports{};
    std::uint8_t gear_support_count{0};
    foundation::StableId morphology_key{0};
};

struct GroundContactOutput final {
    std::uint64_t surface_revision{0};
    GroundSupportPoint feet[2]{};
    float body_lift{0.0F};
    float max_ik_error{0.0F};
    bool has_surface{false};

    [[nodiscard]] bool valid() const noexcept;
};

[[nodiscard]] inline foundation::StableId groundContactCacheKey(
    const GroundContactInput& input, const GroundSurfaceQuery& surface) noexcept {
    foundation::StableId key = foundation::stable_id("infantry.ground-contact.v1");
    key = foundation::stableHashCombine(key, surface.revision);
    key = foundation::stableHashCombine(key, input.morphology_key);
    const auto addPoint = [&key](foundation::Vec3 point) {
        key = foundation::stableHashCombine(key, foundation::stableHashFloat(point.x));
        key = foundation::stableHashCombine(key, foundation::stableHashFloat(point.y));
        key = foundation::stableHashCombine(key, foundation::stableHashFloat(point.z));
    };
    addPoint(input.left_foot);
    addPoint(input.right_foot);
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(input.sole_offset));
    return key;
}

class GroundContactSolver final {
public:
    [[nodiscard]] static foundation::Result<GroundContactOutput, foundation::Error> solve(
        const GroundContactInput&, const GroundSurfaceQuery&);
};

class GroundContactCache final {
public:
    [[nodiscard]] const GroundContactOutput* find(foundation::StableId key) const;
    void store(foundation::StableId key, const GroundContactOutput&);
    void clear();
    [[nodiscard]] std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<foundation::StableId, GroundContactOutput> entries_;
};

} // namespace genomes::infantry
