#pragma once

#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <filesystem>
#include <string>

namespace genomes::world {

inline constexpr std::uint32_t WorldGenerationProfileSchemaVersion = 3U;

// The application composition root resolves this immutable domain snapshot
// once. Generators receive only mutable requests copied from this profile and
// never retain JSON/parser state.
class FrozenWorldGenerationProfile final {
public:
    FrozenWorldGenerationProfile(const FrozenWorldGenerationProfile&) = default;
    FrozenWorldGenerationProfile(FrozenWorldGenerationProfile&&) noexcept = default;
    FrozenWorldGenerationProfile& operator=(const FrozenWorldGenerationProfile&) = default;
    FrozenWorldGenerationProfile& operator=(FrozenWorldGenerationProfile&&) noexcept = default;

    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::filesystem::path& source() const noexcept { return source_; }
    [[nodiscard]] const content::FrozenContentSnapshot& contentSnapshot() const noexcept {
        return content_snapshot_;
    }
    [[nodiscard]] foundation::SimConfigHash fingerprint() const noexcept {
        return fingerprint_;
    }
    [[nodiscard]] WorldGenerationRequest makeRequest(proc::Seed seed) const noexcept {
        WorldGenerationRequest request{};
        request.seed = seed;
        request.map_size_m = map_size_m_;
        request.vegetation = vegetation_;
        request.buildings = buildings_;
        request.fenced_parcels = fenced_parcels_;
        request.hydrology_mode = hydrology_mode_;
        request.river_probability = river_probability_;
        request.terrain = terrain_;
        request.hydrology = hydrology_;
        return request;
    }
    [[nodiscard]] WorldGenerationRequest makeDefaultRequest() const noexcept {
        return makeRequest(default_seed_);
    }

private:
    FrozenWorldGenerationProfile() = default;

    friend foundation::Result<FrozenWorldGenerationProfile, foundation::Error>
    loadWorldGenerationProfile(const std::filesystem::path&, content::ContentReadLimits);

    proc::Seed default_seed_{0U};
    std::uint32_t map_size_m_{0U};
    float vegetation_{0.0F};
    float buildings_{0.0F};
    float fenced_parcels_{0.0F};
    hydrology::HydrologyMode hydrology_mode_{hydrology::HydrologyMode::Off};
    float river_probability_{0.0F};
    TerrainGenerationConfig terrain_{};
    HydrologyGenerationConfig hydrology_{};
    std::string id_;
    std::filesystem::path source_;
    content::FrozenContentSnapshot content_snapshot_{};
    foundation::SimConfigHash fingerprint_{};
    bool frozen_{false};
};

[[nodiscard]] foundation::Result<FrozenWorldGenerationProfile, foundation::Error>
loadWorldGenerationProfile(
    const std::filesystem::path& path,
    content::ContentReadLimits limits = {64U * 1024U});

} // namespace genomes::world
