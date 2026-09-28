#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/RigSchema.hpp>
#include <genomes/simulation/Entity.hpp>
#include <genomes/spatial/SpatialGrid.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::infantry {

enum class DamageRegion : std::uint8_t {
    Head,
    Torso,
    Pelvis,
    UpperArmLeft,
    ForeArmLeft,
    HandLeft,
    UpperArmRight,
    ForeArmRight,
    HandRight,
    ThighLeft,
    ShinLeft,
    FootLeft,
    ThighRight,
    ShinRight,
    FootRight,
};

struct HitVolumeRecipe final {
    DamageRegion region{DamageRegion::Torso};
    BoneId bone{BoneId::Chest};
    foundation::Vec3 local_offset{};
    foundation::Vec3 half_extent{0.12F, 0.12F, 0.12F};
    float protection{0.0F};
    float damage_multiplier{1.0F};

    [[nodiscard]] bool valid() const noexcept;
};

struct HitVolume final {
    simulation::EntityId entity{};
    std::uint16_t volume_id{0};
    HitVolumeRecipe recipe{};
    foundation::Vec3 world_position{};
    std::uint64_t pose_revision{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct DamagePacket final {
    DamageRegion region{DamageRegion::Torso};
    float energy{0.0F};
    float armor_penetration{0.0F};
    std::uint64_t simulation_tick{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct DamageResult final {
    float absorbed{0.0F};
    float applied{0.0F};
    float health_after{0.0F};
    bool killed{false};
    bool ragdoll_requested{false};
};

class InfantryDamageModel final {
public:
    [[nodiscard]] static std::span<const HitVolumeRecipe> recipes() noexcept;
    [[nodiscard]] static foundation::Result<std::vector<HitVolume>, foundation::Error>
    buildVolumes(simulation::EntityId entity, const SkeletonData& skeleton);
    [[nodiscard]] static foundation::Result<void, foundation::Error> updateVolumes(
        std::span<HitVolume> volumes, const AnimationPose& pose, const SkeletonData& skeleton,
        std::uint64_t pose_revision) noexcept;
    [[nodiscard]] static foundation::Result<DamageResult, foundation::Error> apply(
        float& health, float max_health, const DamagePacket& packet) noexcept;
};

struct DamageBatchItem final {
    simulation::EntityId entity{};
    std::span<HitVolume> volumes{};
    const AnimationPose* pose{nullptr};
    const SkeletonData* skeleton{nullptr};
    std::uint64_t pose_revision{0};
};

struct DamageBatchReport final {
    std::uint32_t entities_updated{0};
    std::uint32_t volumes_updated{0};
    std::uint64_t reindex_revision{0};
};

// The grid is rebuilt once at the simulation barrier after all disjoint volume
// transforms are written. It is never mutated by individual hit-volume jobs.
class HitVolumeBatcher final {
public:
    [[nodiscard]] foundation::Result<DamageBatchReport, foundation::Error> update(
        std::span<DamageBatchItem> items, spatial::UniformGrid& index,
        std::uint64_t reindex_revision);

    [[nodiscard]] std::uint64_t lastReindexRevision() const noexcept {
        return last_reindex_revision_;
    }

private:
    std::uint64_t last_reindex_revision_{0};
};

} // namespace genomes::infantry
