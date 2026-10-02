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
    float max_body_lift{0.20F};
    float max_replant_distance{0.60F};
};

struct GroundContactOutput final {
    std::uint64_t surface_revision{0};
    GroundSupportPoint feet[2]{};
    float body_lift{0.0F};
    float max_ik_error{0.0F};
    float clearance{0.0F};
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
    addPoint(input.hips);
    key = foundation::stableHashCombine(key, input.gear_support_count);
    for (std::size_t index = 0U; index < input.gear_support_count; ++index)
        addPoint(input.gear_supports[index]);
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(input.sole_offset));
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(input.max_body_lift));
    key = foundation::stableHashCombine(key,
                                        foundation::stableHashFloat(input.max_replant_distance));
    return key;
}

class GroundContactSolver final {
public:
    [[nodiscard]] static foundation::Result<GroundContactOutput, foundation::Error> solve(
        const GroundContactInput&, const GroundSurfaceQuery&);
};

struct PersistentGroundContact final {
    foundation::Vec3 point{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    float weight{0.0F};
    float phase{0.0F};
    bool active{false};
    bool locked{false};
    bool phase_valid{false};
    bool stance{false};
    std::uint32_t reanchors{0U};

    void releaseConstraint() noexcept {
        active = false;
        locked = false;
        weight = 0.0F;
        stance = false;
    }

    void release() noexcept {
        releaseConstraint();
        phase = 0.0F;
        phase_valid = false;
    }
};

// One instance belongs to one animated infantry unit. It deliberately owns
// only constraint memory; terrain sampling and IK remain GroundContactSolver's
// responsibility.
struct GroundContactRuntime final {
    std::array<PersistentGroundContact, 2U> feet{};
    std::array<PersistentGroundContact, 2U> hands{};

    void reset() noexcept {
        for (auto& foot : feet) foot.release();
        for (auto& hand : hands) hand.release();
    }

    [[nodiscard]] foundation::Vec3 resolve(std::size_t index,
                                            foundation::Vec3 candidate,
                                            float strength, bool enabled,
                                            float max_drift, float phase = 0.0F,
                                            bool stance = true) noexcept;
    [[nodiscard]] foundation::Vec3 resolveHand(std::size_t index,
                                                foundation::Vec3 candidate,
                                                float strength, bool enabled,
                                                float max_drift, float phase = 0.0F,
                                                bool stance = true) noexcept;
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
