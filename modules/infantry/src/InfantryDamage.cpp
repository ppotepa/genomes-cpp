#include <genomes/infantry/InfantryDamage.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace genomes::infantry {

namespace {

constexpr std::array<HitVolumeRecipe, 15> kRecipes{{
    {DamageRegion::Head, BoneId::Head, {0.0F, 0.02F, 0.0F}, {0.14F, 0.16F, 0.14F}, 0.15F, 1.8F},
    {DamageRegion::Torso, BoneId::Chest, {0.0F, 0.0F, 0.0F}, {0.28F, 0.24F, 0.16F}, 0.20F, 1.0F},
    {DamageRegion::Pelvis, BoneId::Hips, {0.0F, 0.0F, 0.0F}, {0.23F, 0.16F, 0.14F}, 0.18F, 1.15F},
    {DamageRegion::UpperArmLeft, BoneId::UpperArmL, {0.0F, -0.12F, 0.0F}, {0.10F, 0.20F, 0.10F}, 0.08F, 0.75F},
    {DamageRegion::ForeArmLeft, BoneId::ForeArmL, {0.0F, -0.11F, 0.0F}, {0.085F, 0.18F, 0.085F}, 0.06F, 0.65F},
    {DamageRegion::HandLeft, BoneId::HandL, {0.0F, 0.0F, 0.0F}, {0.10F, 0.07F, 0.08F}, 0.04F, 0.5F},
    {DamageRegion::UpperArmRight, BoneId::UpperArmR, {0.0F, -0.12F, 0.0F}, {0.10F, 0.20F, 0.10F}, 0.08F, 0.75F},
    {DamageRegion::ForeArmRight, BoneId::ForeArmR, {0.0F, -0.11F, 0.0F}, {0.085F, 0.18F, 0.085F}, 0.06F, 0.65F},
    {DamageRegion::HandRight, BoneId::HandR, {0.0F, 0.0F, 0.0F}, {0.10F, 0.07F, 0.08F}, 0.04F, 0.5F},
    {DamageRegion::ThighLeft, BoneId::ThighL, {0.0F, -0.20F, 0.0F}, {0.14F, 0.23F, 0.13F}, 0.09F, 0.8F},
    {DamageRegion::ShinLeft, BoneId::ShinL, {0.0F, -0.20F, 0.0F}, {0.11F, 0.22F, 0.11F}, 0.07F, 0.7F},
    {DamageRegion::FootLeft, BoneId::FootL, {0.0F, -0.05F, 0.08F}, {0.12F, 0.07F, 0.20F}, 0.05F, 0.55F},
    {DamageRegion::ThighRight, BoneId::ThighR, {0.0F, -0.20F, 0.0F}, {0.14F, 0.23F, 0.13F}, 0.09F, 0.8F},
    {DamageRegion::ShinRight, BoneId::ShinR, {0.0F, -0.20F, 0.0F}, {0.11F, 0.22F, 0.11F}, 0.07F, 0.7F},
    {DamageRegion::FootRight, BoneId::FootR, {0.0F, -0.05F, 0.08F}, {0.12F, 0.07F, 0.20F}, 0.05F, 0.55F},
}};

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] const HitVolumeRecipe* recipeFor(DamageRegion region) noexcept {
    for (const HitVolumeRecipe& recipe : kRecipes) {
        if (recipe.region == region) {
            return &recipe;
        }
    }
    return nullptr;
}

} // namespace

bool HitVolumeRecipe::valid() const noexcept {
    return finite(local_offset) && finite(half_extent) && half_extent.x > 0.0F &&
           half_extent.y > 0.0F && half_extent.z > 0.0F && finite(protection) &&
           protection >= 0.0F && finite(damage_multiplier) && damage_multiplier > 0.0F;
}

bool HitVolume::valid() const noexcept {
    return entity.isValid() && recipe.valid() && finite(world_position);
}

bool DamagePacket::valid() const noexcept {
    return finite(energy) && finite(armor_penetration) && energy >= 0.0F &&
           armor_penetration >= 0.0F;
}

std::span<const HitVolumeRecipe> InfantryDamageModel::recipes() noexcept {
    return kRecipes;
}

foundation::Result<std::vector<HitVolume>, foundation::Error> InfantryDamageModel::buildVolumes(
    simulation::EntityId entity, const SkeletonData& skeleton) {
    if (!entity.isValid() || !skeleton.valid()) {
        return foundation::Result<std::vector<HitVolume>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid hit-volume skeleton contract"});
    }
    std::vector<HitVolume> volumes;
    volumes.reserve(kRecipes.size());
    for (std::size_t index = 0U; index < kRecipes.size(); ++index) {
        if (skeleton.find(kRecipes[index].bone) == nullptr) {
            return foundation::Result<std::vector<HitVolume>, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "hit-volume bone missing from skeleton"});
        }
        volumes.push_back({entity, static_cast<std::uint16_t>(index), kRecipes[index], {}, 0U});
    }
    return foundation::Result<std::vector<HitVolume>, foundation::Error>::success(
        std::move(volumes));
}

foundation::Result<void, foundation::Error> InfantryDamageModel::updateVolumes(
    std::span<HitVolume> volumes, const AnimationPose& pose, const SkeletonData& skeleton,
    std::uint64_t pose_revision) noexcept {
    if (!pose.valid() || !skeleton.valid() || volumes.size() != kRecipes.size()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid hit-volume pose update"});
    }
    for (HitVolume& volume : volumes) {
        const auto* bone = skeleton.find(volume.recipe.bone);
        if (bone == nullptr || volume.volume_id >= kRecipes.size()) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "hit-volume recipe/skeleton mismatch"});
        }
        const auto& local = pose.bones[boneIndex(volume.recipe.bone)].translation;
        volume.world_position = add(add(pose.root_position, local), volume.recipe.local_offset);
        volume.pose_revision = pose_revision;
        if (!volume.valid()) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "hit-volume update produced invalid value"});
        }
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<DamageResult, foundation::Error> InfantryDamageModel::apply(
    float& health, float max_health, const DamagePacket& packet) noexcept {
    const HitVolumeRecipe* recipe = recipeFor(packet.region);
    if (recipe == nullptr || !packet.valid() || !finite(health) || !finite(max_health) ||
        max_health <= 0.0F || health < 0.0F) {
        return foundation::Result<DamageResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry damage packet"});
    }
    const float effective_protection = std::max(0.0F, recipe->protection - packet.armor_penetration);
    const float absorbed = std::min(packet.energy, effective_protection);
    const float applied = std::max(0.0F, packet.energy - effective_protection) *
                          recipe->damage_multiplier;
    health = std::clamp(health - applied, 0.0F, max_health);
    return foundation::Result<DamageResult, foundation::Error>::success(
        {absorbed, applied, health, health <= 0.0F, health <= 0.0F});
}

foundation::Result<DamageBatchReport, foundation::Error> HitVolumeBatcher::update(
    std::span<DamageBatchItem> items, spatial::UniformGrid& index,
    std::uint64_t reindex_revision) {
    if (reindex_revision < last_reindex_revision_) {
        return foundation::Result<DamageBatchReport, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "hit-volume reindex revision regressed"});
    }
    index.clear();
    std::size_t volume_count = 0U;
    for (DamageBatchItem& item : items) {
        if (!item.entity.isValid() || item.pose == nullptr || item.skeleton == nullptr ||
            !item.pose->valid()) {
            return foundation::Result<DamageBatchReport, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid hit-volume batch item"});
        }
        const auto updated = InfantryDamageModel::updateVolumes(
            item.volumes, *item.pose, *item.skeleton, item.pose_revision);
        if (!updated) {
            return foundation::Result<DamageBatchReport, foundation::Error>::failure(updated.error());
        }
        for (const HitVolume& volume : item.volumes) {
            index.insert(item.entity, volume.world_position);
        }
        volume_count += item.volumes.size();
    }
    last_reindex_revision_ = reindex_revision;
    return foundation::Result<DamageBatchReport, foundation::Error>::success(
        {static_cast<std::uint32_t>(items.size()), static_cast<std::uint32_t>(volume_count),
         reindex_revision});
}

} // namespace genomes::infantry
