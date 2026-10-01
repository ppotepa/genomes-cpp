#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>
#include <genomes/world/BuildingSite.hpp>

#include <cstdint>
#include <vector>

namespace genomes::buildings {

inline constexpr std::uint32_t BuildingGeneratorVersion = 2;

struct BuildingSpec final {
    foundation::StableId building_id{0};
    proc::Seed seed{0};
    foundation::Vec3 footprint{12.0F, 1.0F, 10.0F};
    std::uint32_t floors{1};
    float floor_height{3.0F};
    float wall_thickness{0.25F};
    std::uint32_t rooms_per_floor{2};

    [[nodiscard]] bool valid() const noexcept;
};

enum class BuildingPartKind : std::uint8_t {
    Foundation,
    Floor,
    Wall,
    Roof,
    Door,
};

enum class BuildingPartRole : std::uint8_t {
    Foundation,
    FloorSlab,
    ExteriorNorthWall,
    ExteriorSouthWall,
    ExteriorWestWall,
    ExteriorEastWall,
    InteriorPartition,
    EntranceDoor,
    Roof,
};

struct BuildingPartKey final {
    foundation::StableId building_id{0};
    std::uint32_t floor{0};
    BuildingPartKind kind{BuildingPartKind::Wall};
    BuildingPartRole role{BuildingPartRole::InteriorPartition};
    std::uint32_t ordinal{0};
};

struct BuildingRoom final {
    foundation::StableId id{0};
    std::uint32_t floor{0};
    foundation::Vec3 center{};
    foundation::Vec3 extent{};
};

struct BuildingPart final {
    foundation::StableId id{0};
    BuildingPartKey key{};
    BuildingPartKind kind{BuildingPartKind::Wall};
    foundation::Vec3 center{};
    foundation::Vec3 extent{1.0F, 1.0F, 1.0F};
    std::uint32_t floor{0};
    bool structural{true};
};

struct BuildingPlan final {
    std::uint32_t generator_version{BuildingGeneratorVersion};
    foundation::StableId building_id{0};
    proc::Seed seed{0};
    foundation::Vec3 footprint{};
    std::vector<BuildingRoom> rooms;
    std::vector<BuildingPart> parts;
    std::uint64_t content_hash{0};

    [[nodiscard]] bool compatible() const noexcept {
        return generator_version == BuildingGeneratorVersion;
    }
};

struct BuildingGenerationResult final {
    BuildingPlan plan{};
    world::BuildingSiteResolution resolution{};
};

class BuildingGenerator final {
public:
    [[nodiscard]] static foundation::Result<BuildingPlan, foundation::Error> generate(
        const BuildingSpec&);
    [[nodiscard]] static foundation::Result<BuildingGenerationResult, foundation::Error>
    generateSite(const world::BuildingSiteRequest&);
};

struct BuildingPartRuntime final {
    foundation::StableId part_id{0};
    float integrity{1.0F};
    bool destroyed{false};
};

class BuildingRuntime final {
public:
    explicit BuildingRuntime(const BuildingPlan&);

    [[nodiscard]] bool applyDamage(foundation::StableId part_id, float normalized_damage) noexcept;
    [[nodiscard]] bool isDestroyed(foundation::StableId part_id) const noexcept;
    [[nodiscard]] const std::vector<BuildingPartRuntime>& parts() const noexcept {
        return parts_;
    }

private:
    std::vector<BuildingPartRuntime> parts_;
};

} // namespace genomes::buildings
