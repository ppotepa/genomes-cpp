#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/hydrology/HydrologyArtifact.hpp>
#include <genomes/proc/Seed.hpp>
#include <genomes/world/BuildingSite.hpp>
#include <genomes/world/CityPlan.hpp>
#include <genomes/world/GridLayout.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::world {

inline constexpr std::uint32_t WorldGeneratorVersion = 2;
inline constexpr std::uint32_t WorldStageFingerprintVersion = 1;

enum class WorldFeatureKind : std::uint8_t {
    TerrainPatch,
    Road,
    Parcel,
    Building,
    Vegetation,
    Fence
};

enum class WorldGenerationStage : std::uint8_t {
    Terrain,
    Hydrology,
    Roads,
    Buildings,
    Vegetation,
};

struct WorldStageFingerprint final {
    WorldGenerationStage stage{WorldGenerationStage::Terrain};
    proc::Seed seed{0};
    std::uint32_t version{0};
    std::uint64_t dependency_fingerprint{0};

    [[nodiscard]] bool valid() const noexcept {
        return seed != 0U && version != 0U && dependency_fingerprint != 0U;
    }

    friend constexpr bool operator==(const WorldStageFingerprint&, const WorldStageFingerprint&) noexcept = default;
};

struct WorldGenerationRequest final {
    // A request is mutable session state. Domain defaults live exclusively in
    // FrozenWorldGenerationProfile and must be resolved before generation.
    proc::Seed seed{0U};
    std::uint32_t map_size_m{0U};
    float vegetation{0.0F};
    float buildings{0.0F};
    float fenced_parcels{0.0F};
    hydrology::HydrologyMode hydrology_mode{hydrology::HydrologyMode::Off};
    float river_probability{0.0F};

    [[nodiscard]] bool valid() const noexcept {
        const auto valid_density = [](float value) {
            return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
        };
        const bool valid_hydrology_mode =
            hydrology_mode == hydrology::HydrologyMode::Off ||
            hydrology_mode == hydrology::HydrologyMode::SeededOptional ||
            hydrology_mode == hydrology::HydrologyMode::Forced;
        return seed != 0U && map_size_m >= 128 && map_size_m <= 4096 && map_size_m % 8U == 0U &&
               valid_density(vegetation) &&
               valid_density(buildings) && valid_density(fenced_parcels) &&
               std::isfinite(river_probability) && river_probability >= 0.0F &&
               river_probability <= 1.0F && valid_hydrology_mode;
    }
};

struct WorldFeature final {
    foundation::StableId id{0};
    WorldFeatureKind kind{WorldFeatureKind::TerrainPatch};
    foundation::Vec3 position{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
    std::uint32_t variant{0};
};

struct WorldPlan final {
    std::uint32_t generator_version{WorldGeneratorVersion};
    proc::Seed seed{0};
    std::uint32_t map_size_m{0};
    CityPlan city{};
    hydrology::HydrologyArtifact hydrology{};
    std::vector<WorldFeature> features;
    std::vector<BuildingSiteRequest> building_sites;
    std::array<WorldStageFingerprint, 5> stage_fingerprints{};
    std::uint64_t content_hash{0};

    [[nodiscard]] bool hasValidStageFingerprints() const noexcept {
        for (std::size_t index = 0; index < stage_fingerprints.size(); ++index) {
            const auto& stage = stage_fingerprints[index];
            if (stage.stage != static_cast<WorldGenerationStage>(index) || !stage.valid()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::size_t count(WorldFeatureKind kind) const noexcept {
        std::size_t result = 0;
        for (const WorldFeature& feature : features) {
            if (feature.kind == kind) {
                ++result;
            }
        }
        return result;
    }
};

class WorldGenerator final {
public:
    [[nodiscard]] static foundation::Result<WorldPlan, foundation::Error> generate(
        const WorldGenerationRequest& request);
};

} // namespace genomes::world
