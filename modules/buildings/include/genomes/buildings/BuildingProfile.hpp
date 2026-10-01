#pragma once

#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <cstdint>
#include <filesystem>

namespace genomes::buildings {

struct BuildingSpec;

inline constexpr std::uint32_t BuildingProfileSchemaVersion = 1U;

struct BuildingSiteGenerationProfile final {
    float floor_height{0.0F};
    float wall_thickness{0.0F};
    float target_room_width{0.0F};
    std::uint32_t minimum_rooms_per_floor{0U};
    std::uint32_t maximum_rooms_per_floor{0U};

    [[nodiscard]] bool valid() const noexcept;
};

struct BuildingTemplateDefinition final {
    foundation::StableId id{0U};
    foundation::Vec3 footprint{};
    std::uint32_t floors{0U};
    float floor_height{0.0F};
    float wall_thickness{0.0F};
    std::uint32_t rooms_per_floor{0U};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] BuildingSpec instantiate(proc::Seed seed) const noexcept;
};

// A building profile is resolved once at the composition boundary. Production
// generators consume only this frozen, typed view; the JSON document and any
// hardcoded fallback values never enter the runtime generation path.
class FrozenBuildingProfile final {
public:
    FrozenBuildingProfile(const FrozenBuildingProfile&) = default;
    FrozenBuildingProfile(FrozenBuildingProfile&&) noexcept = default;
    FrozenBuildingProfile& operator=(const FrozenBuildingProfile&) = default;
    FrozenBuildingProfile& operator=(FrozenBuildingProfile&&) noexcept = default;

    [[nodiscard]] const BuildingSiteGenerationProfile& siteGeneration() const noexcept {
        return site_generation_;
    }
    [[nodiscard]] const BuildingTemplateDefinition& labPreview() const noexcept {
        return lab_preview_;
    }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] const content::FrozenContentSnapshot& contentSnapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] foundation::SimConfigHash fingerprint() const noexcept {
        return fingerprint_;
    }

private:
    FrozenBuildingProfile() = default;

    friend foundation::Result<FrozenBuildingProfile, foundation::Error>
    loadBuildingProfile(const std::filesystem::path& path);

    BuildingSiteGenerationProfile site_generation_{};
    BuildingTemplateDefinition lab_preview_{};
    content::FrozenContentSnapshot snapshot_{};
    foundation::SimConfigHash fingerprint_{};
    bool frozen_{false};
};

[[nodiscard]] foundation::Result<FrozenBuildingProfile, foundation::Error>
loadBuildingProfile(const std::filesystem::path& path);

} // namespace genomes::buildings
