#pragma once

#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/world/GridLayout.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/EquipmentCatalog.hpp>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace genomes::game_scenes::battlefield_detail {

constexpr std::uint64_t MassBattleRtsCameraRevision = 0x52545343414D3031ULL;
#if GENOMES_HAS_INFANTRY
constexpr float MassBattleModelYawOffset = 0.0F;
// Simulation yaw is +Z toward +X; the Diligent world matrix rotates +Z
// toward -X for positive yaw. Convert only at the presentation boundary.
[[nodiscard]] constexpr float infantryPresentationYaw(float heading) noexcept {
    return -heading + MassBattleModelYawOffset;
}
#endif

constexpr std::array<std::size_t, 8U> MassBattlePosePhaseCounts{
     1U, 12U, 12U, 1U, 8U, 1U, 8U, 1U};
static_assert([]() constexpr {
    std::size_t total = 0U;
    for (const std::size_t phase_count : MassBattlePosePhaseCounts) {
        total += phase_count;
    }
    return total;
}() == 44U);

[[nodiscard]] constexpr const char* massBattleProfileName(
    MassBattlePresentationProfile profile) noexcept {
    switch (profile) {
    case MassBattlePresentationProfile::Quality: return "Quality";
    case MassBattlePresentationProfile::Balanced: return "Balanced";
    case MassBattlePresentationProfile::Stress: return "Stress";
    }
    return "Balanced";
}

[[nodiscard]] constexpr const char* massBattleAnimationMode(
    MassBattlePresentationProfile profile) noexcept {
    return profile == MassBattlePresentationProfile::Quality ? "live skinning" : "atlas";
}

[[nodiscard]] constexpr const char* terrainPresetName(world::TerrainPreset preset) noexcept {
    switch (preset) {
    case world::TerrainPreset::Plains: return "Plains";
    case world::TerrainPreset::RollingHills: return "Rolling hills";
    case world::TerrainPreset::Highlands: return "Highlands";
    case world::TerrainPreset::RiverValley: return "River valley";
    case world::TerrainPreset::CombatMixed: return "Combat mixed";
    }
    return "Rolling hills";
}

[[nodiscard]] constexpr const char* hydrologyModeName(
    hydrology::HydrologyMode mode) noexcept {
    switch (mode) {
    case hydrology::HydrologyMode::Off: return "Off";
    case hydrology::HydrologyMode::SeededOptional: return "Seeded optional";
    case hydrology::HydrologyMode::Forced: return "Forced";
    }
    return "Off";
}

[[nodiscard]] constexpr world::TerrainPreset nextTerrainPreset(
    world::TerrainPreset preset) noexcept {
    switch (preset) {
    case world::TerrainPreset::Plains: return world::TerrainPreset::RollingHills;
    case world::TerrainPreset::RollingHills: return world::TerrainPreset::Highlands;
    case world::TerrainPreset::Highlands: return world::TerrainPreset::RiverValley;
    case world::TerrainPreset::RiverValley: return world::TerrainPreset::CombatMixed;
    case world::TerrainPreset::CombatMixed: return world::TerrainPreset::Plains;
    }
    return world::TerrainPreset::RollingHills;
}

[[nodiscard]] constexpr hydrology::HydrologyMode nextHydrologyMode(
    hydrology::HydrologyMode mode) noexcept {
    switch (mode) {
    case hydrology::HydrologyMode::Off: return hydrology::HydrologyMode::SeededOptional;
    case hydrology::HydrologyMode::SeededOptional: return hydrology::HydrologyMode::Forced;
    case hydrology::HydrologyMode::Forced: return hydrology::HydrologyMode::Off;
    }
    return hydrology::HydrologyMode::Off;
}

[[nodiscard]] constexpr std::uint8_t nextTerrainSampleSpacing(
    std::uint8_t sample_spacing_m) noexcept {
    // 4 m is the default battlefield detail and 8 m is the lower-cost option.
    // Finer levels remain available in the profile API, but are deliberately
    // not offered as a casual live-scene toggle for a 2 km battle map.
    return sample_spacing_m <= 4U ? 8U : 4U;
}

[[nodiscard]] inline foundation::Color lerpColor(foundation::Color from, foundation::Color to,
                                          float amount) noexcept {
    const float t = std::clamp(amount, 0.0F, 1.0F);
    return {std::lerp(from.r, to.r, t), std::lerp(from.g, to.g, t),
            std::lerp(from.b, to.b, t), std::lerp(from.a, to.a, t)};
}

[[nodiscard]] inline float terrainPatchNoise(float x, float z, float cell_size_m,
                                      foundation::StableId seed) noexcept {
    const float grid_x = x / cell_size_m;
    const float grid_z = z / cell_size_m;
    const int x0 = static_cast<int>(std::floor(grid_x));
    const int z0 = static_cast<int>(std::floor(grid_z));
    const auto lattice = [seed](int cell_x, int cell_z) {
        std::uint64_t hash = foundation::stableHashCombine(
            seed, static_cast<std::uint64_t>(static_cast<std::int64_t>(cell_x)));
        hash = foundation::stableHashCombine(
            hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(cell_z)));
        return static_cast<float>(hash & 0xFFFFU) / 65535.0F;
    };
    const auto smooth = [](float value) {
        const float t = std::clamp(value, 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    };
    const float tx = smooth(grid_x - static_cast<float>(x0));
    const float tz = smooth(grid_z - static_cast<float>(z0));
    const float lower = std::lerp(lattice(x0, z0), lattice(x0 + 1, z0), tx);
    const float upper = std::lerp(lattice(x0, z0 + 1), lattice(x0 + 1, z0 + 1), tx);
    return std::lerp(lower, upper, tz);
}

[[nodiscard]] inline foundation::Color terrainSurfaceColor(
    const terrain::TerrainMeshVertex& vertex, const world::WorldPlan& plan,
    float minimum_height, float maximum_height) noexcept {
    const float relief = std::max(maximum_height - minimum_height, 1.0F);
    const float elevation = std::clamp((vertex.position.y - minimum_height) / relief, 0.0F, 1.0F);
    const float slope = std::clamp((1.0F - vertex.normal.y) * 5.0F, 0.0F, 1.0F);
    const float wetness = plan.hydrology.sampleWater(vertex.position.x, vertex.position.z).wetness;
    // This is a compact, deterministic surface mask until authored terrain
    // materials are introduced.  It makes large landforms legible without
    // leaking renderer textures into world generation.
    const float broad_patch = terrainPatchNoise(vertex.position.x, vertex.position.z, 180.0F,
                                                 foundation::stable_id("terrain.surface.broad"));
    const float fine_patch = terrainPatchNoise(vertex.position.x, vertex.position.z, 52.0F,
                                                foundation::stable_id("terrain.surface.fine"));
    const float dryness = std::clamp(elevation * 0.62F + broad_patch * 0.22F +
                                         fine_patch * 0.08F - wetness * 0.58F,
                                     0.0F, 1.0F);
    foundation::Color surface = lerpColor({0.075F, 0.27F, 0.075F, 1.0F},
                                          {0.48F, 0.38F, 0.105F, 1.0F}, dryness);
    surface = lerpColor(surface, {0.34F, 0.31F, 0.24F, 1.0F}, slope * slope * 0.48F);
    surface = lerpColor(surface, {0.055F, 0.20F, 0.085F, 1.0F}, wetness * 0.72F);
    return surface;
}

[[nodiscard]] inline std::string durationText(foundation::Nanoseconds duration) {
    const double milliseconds = std::chrono::duration<double, std::milli>(duration).count();
    const double rounded = std::round(milliseconds * 100.0) / 100.0;
    return std::to_string(rounded) + " ms";
}

#if GENOMES_HAS_INFANTRY
[[nodiscard]] constexpr infantry::AnimationState massBattleAnimationState(
    std::uint8_t variant) noexcept {
    switch (massBattleAnimationArchetype(variant)) {
    case MassBattleAnimationArchetype::Idle: return infantry::AnimationState::IDLE;
    case MassBattleAnimationArchetype::Walk: return infantry::AnimationState::WALK;
    case MassBattleAnimationArchetype::Run: return infantry::AnimationState::RUN;
    case MassBattleAnimationArchetype::Crouch: return infantry::AnimationState::CROUCH;
    case MassBattleAnimationArchetype::CrouchWalk:
        return infantry::AnimationState::CROUCH_WALK;
    case MassBattleAnimationArchetype::Prone: return infantry::AnimationState::PRONE;
    case MassBattleAnimationArchetype::ProneMove: return infantry::AnimationState::PRONE_MOVE;
    case MassBattleAnimationArchetype::WeaponReady: return infantry::AnimationState::IDLE;
    }
    return infantry::AnimationState::IDLE;
}

[[nodiscard]] inline bool massBattleUnitShouldRender(
    const render::PresentationSnapshot& presentation,
    MassBattlePresentationProfile profile,
    foundation::Vec3 position,
    float height,
    foundation::Vec3 camera_target) noexcept {
    if (profile == MassBattlePresentationProfile::Stress ||
        !presentation.has_resolved_camera) {
        return true;
    }
    const foundation::Vec3 center = position + foundation::Vec3{0.0F, height * 0.5F, 0.0F};
    const float radius = height * 1.5F;
    bool intersects_view = true;
    for (const auto& plane : presentation.resolved_camera.frustum.planes) {
        if (plane.signedDistance(center) < -radius) {
            intersects_view = false;
            break;
        }
    }
    const float shadow_dx = position.x - camera_target.x;
    const float shadow_dz = position.z - camera_target.z;
    const bool shadow_relevant = shadow_dx * shadow_dx + shadow_dz * shadow_dz <=
                                 180.0F * 180.0F;
    return intersects_view || shadow_relevant;
}

[[nodiscard]] constexpr std::size_t massBattleArchetypeIndex(
    std::uint8_t variant) noexcept {
    return static_cast<std::size_t>(massBattleAnimationArchetype(variant));
}

[[nodiscard]] inline std::optional<infantry::AnimationState> infantryActionAnimationState(
    foundation::StableId action) noexcept {
    if (action == foundation::stable_id("infantry.action.idle"))
        return infantry::AnimationState::IDLE;
    if (action == foundation::stable_id("infantry.action.walk"))
        return infantry::AnimationState::WALK;
    if (action == foundation::stable_id("infantry.action.run"))
        return infantry::AnimationState::RUN;
    if (action == foundation::stable_id("infantry.action.crouch") ||
        action == foundation::stable_id("infantry.action.weapon-ready"))
        return infantry::AnimationState::CROUCH;
    if (action == foundation::stable_id("infantry.action.crouch-walk"))
        return infantry::AnimationState::CROUCH_WALK;
    if (action == foundation::stable_id("infantry.action.prone"))
        return infantry::AnimationState::PRONE;
    if (action == foundation::stable_id("infantry.action.prone-move"))
        return infantry::AnimationState::PRONE_MOVE;
    return std::nullopt;
}
#endif

[[nodiscard]] constexpr std::size_t massBattlePoseOffset(std::size_t variant) noexcept {
    std::size_t offset=0U;
    for (std::size_t index=0U;index<variant;++index) offset+=MassBattlePosePhaseCounts[index];
    return offset;
}

[[nodiscard]] constexpr std::size_t massBattlePoseBucket(std::size_t variant,float phase,
                                                          std::size_t visible_phases) noexcept {
    const std::size_t native_phases=MassBattlePosePhaseCounts[variant];
    visible_phases=std::clamp(visible_phases,std::size_t{1U},native_phases);
    const float wrapped=std::clamp(phase,0.0F,0.99999994F);
    const std::size_t coarse=std::min(
        static_cast<std::size_t>(wrapped*static_cast<float>(visible_phases)),visible_phases-1U);
    const std::size_t native=std::min(
        ((coarse*2U+1U)*native_phases)/(visible_phases*2U),native_phases-1U);
    return massBattlePoseOffset(variant)+native;
}

static_assert(massBattlePoseOffset(0U)==0U&&massBattlePoseOffset(1U)==1U&&
              massBattlePoseOffset(2U)==13U&&massBattlePoseOffset(3U)==25U&&
              massBattlePoseOffset(4U)==26U);
static_assert(massBattlePoseBucket(1U,0.0F,12U)==1U&&
              massBattlePoseBucket(1U,0.99F,12U)==12U&&
              massBattlePoseBucket(2U,0.25F,6U)>=13U&&
              massBattlePoseBucket(4U,0.75F,2U)<34U&&
              massBattlePoseOffset(5U)==34U&&massBattlePoseOffset(6U)==35U);

#if GENOMES_HAS_INFANTRY
inline bool sample_battlefield_ground(void* context, foundation::Vec3 position,
                               infantry::GroundSample& output) noexcept {
    const auto* artifacts = static_cast<const gameplay::WorldScenarioArtifact*>(context);
    if (artifacts == nullptr || artifacts->terrain == nullptr) return false;
    const auto landscape = artifacts->sampleLandscape(position.x, position.z);
    if (!landscape.traversable()) return false;
    output.height = landscape.ground_y;
    output.normal = artifacts->terrain->normal(position.x, position.z);
    return std::isfinite(output.height) && std::isfinite(output.normal.x) &&
           std::isfinite(output.normal.y) && std::isfinite(output.normal.z);
}
#endif

[[nodiscard]] inline std::string feature_summary(const world::WorldPlan& plan) {
    return "Features: " + std::to_string(plan.features.size()) + "  Roads: " +
           std::to_string(plan.count(world::WorldFeatureKind::Road)) + "  Buildings: " +
           std::to_string(plan.count(world::WorldFeatureKind::Building)) + "  Vegetation: " +
           std::to_string(plan.count(world::WorldFeatureKind::Vegetation)) + "  Parcels: " +
           std::to_string(plan.city.parcels.size()) + "  Rivers: " +
           std::to_string(plan.hydrology.rivers.size());
}

[[nodiscard]] inline application::WorldGenerationConfig massBattleWorldConfig(
    application::WorldGenerationConfig config) noexcept {
    if (config.seed == 0U) config.seed = 0x1F4A77U;
    const bool initial_mass_battle_config = config.map_size_m != 2000U;
    config.map_size_m = 2000U;
    config.vegetation = std::min(config.vegetation, 0.18F);
    config.buildings = 0.0F;
    config.fenced_parcels = 0.0F;
    if (initial_mass_battle_config) {
        config.hydrology_mode = hydrology::HydrologyMode::Forced;
        config.river_probability = 1.0F;
        config.terrain.preset = world::TerrainPreset::CombatMixed;
    }
    config.terrain.elevation_range_m = std::max(config.terrain.elevation_range_m, 250.0F);
    if (initial_mass_battle_config) config.terrain.sample_spacing_m = 4U;
    config.terrain.landform_scale_m = std::min(config.terrain.landform_scale_m, 420.0F);
    config.terrain.roughness = std::max(config.terrain.roughness, 0.70F);
    config.hydrology.main_river_min = std::max(config.hydrology.main_river_min, std::uint8_t{1U});
    config.hydrology.river_width_min_m =
        std::max(config.hydrology.river_width_min_m, 16.0F);
    config.hydrology.river_width_max_m =
        std::max(config.hydrology.river_width_max_m, 28.0F);
    config.hydrology.valley_width_min_m =
        std::max(config.hydrology.valley_width_min_m, 48.0F);
    config.hydrology.valley_width_max_m =
        std::max(config.hydrology.valley_width_max_m, 110.0F);
    return config;
}

} // namespace genomes::game_scenes::battlefield_detail
