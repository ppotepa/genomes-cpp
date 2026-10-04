#include <genomes/world/WorldGenerationProfile.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace genomes::world {
namespace {

using Json = nlohmann::json;

[[nodiscard]] foundation::Error profileError(std::string_view message) noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] bool exactFields(const Json& value,
                               const std::set<std::string>& fields) noexcept {
    if (!value.is_object() || value.size() != fields.size()) return false;
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (!fields.contains(key)) return false;
    }
    return true;
}

[[nodiscard]] std::optional<hydrology::HydrologyMode> hydrologyMode(
    const Json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto mode = value.get<std::string>();
    if (mode == "off") return hydrology::HydrologyMode::Off;
    if (mode == "seeded-optional") return hydrology::HydrologyMode::SeededOptional;
    if (mode == "forced") return hydrology::HydrologyMode::Forced;
    return std::nullopt;
}

[[nodiscard]] std::optional<TerrainPreset> terrainPreset(const Json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto preset = value.get<std::string>();
    if (preset == "plains") return TerrainPreset::Plains;
    if (preset == "rolling-hills") return TerrainPreset::RollingHills;
    if (preset == "highlands") return TerrainPreset::Highlands;
    if (preset == "river-valley") return TerrainPreset::RiverValley;
    if (preset == "combat-mixed") return TerrainPreset::CombatMixed;
    return std::nullopt;
}

[[nodiscard]] std::optional<TributaryDensity> tributaryDensity(const Json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto density = value.get<std::string>();
    if (density == "none") return TributaryDensity::None;
    if (density == "low") return TributaryDensity::Low;
    if (density == "medium") return TributaryDensity::Medium;
    return std::nullopt;
}

[[nodiscard]] bool unsignedValue(const Json& value, std::uint64_t maximum) noexcept {
    if (!value.is_number_unsigned()) return false;
    try {
        return value.get<std::uint64_t>() <= maximum;
    } catch (const std::exception&) {
        return false;
    }
}

[[nodiscard]] bool finiteNumber(const Json& value) noexcept {
    if (!value.is_number()) return false;
    try {
        return std::isfinite(value.get<double>());
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

foundation::Result<FrozenWorldGenerationProfile, foundation::Error>
loadWorldGenerationProfile(const std::filesystem::path& path,
                           content::ContentReadLimits limits) {
    auto document = content::readContentText(path, limits);
    if (!document) {
        return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
            document.error());
    }
    try {
        const Json json = Json::parse(document.value().text);
        static const std::set<std::string> fields{
            "schema_version", "id", "default_seed", "map_size_m", "vegetation",
            "buildings", "fenced_parcels", "hydrology_mode", "river_probability",
            "terrain", "hydrology"};
        static const std::set<std::string> terrain_fields{
            "preset", "sample_spacing_m", "elevation_range_m", "landform_scale_m", "roughness"};
        static const std::set<std::string> hydrology_fields{
            "main_river_min", "main_river_max", "tributary_density",
            "stream_width_min_m", "stream_width_max_m", "river_width_min_m",
            "river_width_max_m", "depth_min_m", "depth_max_m", "meander_strength",
            "valley_width_min_m", "valley_width_max_m"};
        if (!exactFields(json, fields) ||
            !unsignedValue(json.at("schema_version"),
                           std::numeric_limits<std::uint32_t>::max()) ||
            json.at("schema_version").get<std::uint32_t>() !=
                WorldGenerationProfileSchemaVersion ||
            !json.at("id").is_string() || json.at("id").get<std::string>().empty() ||
            !unsignedValue(json.at("default_seed"),
                           std::numeric_limits<proc::Seed>::max()) ||
            !unsignedValue(json.at("map_size_m"),
                           std::numeric_limits<std::uint32_t>::max()) ||
            !finiteNumber(json.at("vegetation")) ||
            !finiteNumber(json.at("buildings")) ||
            !finiteNumber(json.at("fenced_parcels")) ||
            !finiteNumber(json.at("river_probability")) ||
            !exactFields(json.at("terrain"), terrain_fields) ||
            !exactFields(json.at("hydrology"), hydrology_fields) ||
            !unsignedValue(json.at("terrain").at("sample_spacing_m"), 16U) ||
            !finiteNumber(json.at("terrain").at("elevation_range_m")) ||
            !finiteNumber(json.at("terrain").at("landform_scale_m")) ||
            !finiteNumber(json.at("terrain").at("roughness")) ||
            !unsignedValue(json.at("hydrology").at("main_river_min"), 4U) ||
            !unsignedValue(json.at("hydrology").at("main_river_max"), 4U) ||
            !finiteNumber(json.at("hydrology").at("stream_width_min_m")) ||
            !finiteNumber(json.at("hydrology").at("stream_width_max_m")) ||
            !finiteNumber(json.at("hydrology").at("river_width_min_m")) ||
            !finiteNumber(json.at("hydrology").at("river_width_max_m")) ||
            !finiteNumber(json.at("hydrology").at("depth_min_m")) ||
            !finiteNumber(json.at("hydrology").at("depth_max_m")) ||
            !finiteNumber(json.at("hydrology").at("meander_strength")) ||
            !finiteNumber(json.at("hydrology").at("valley_width_min_m")) ||
            !finiteNumber(json.at("hydrology").at("valley_width_max_m"))) {
            return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
                profileError("invalid world generation profile schema"));
        }
        const auto mode = hydrologyMode(json.at("hydrology_mode"));
        const auto terrain_preset = terrainPreset(json.at("terrain").at("preset"));
        const auto tributary_density = tributaryDensity(
            json.at("hydrology").at("tributary_density"));
        if (!mode || !terrain_preset || !tributary_density) {
            return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
                profileError("invalid world generation hydrology mode"));
        }

        FrozenWorldGenerationProfile result{};
        result.id_ = json.at("id").get<std::string>();
        result.source_ = path;
        result.default_seed_ = json.at("default_seed").get<proc::Seed>();
        result.map_size_m_ = json.at("map_size_m").get<std::uint32_t>();
        result.vegetation_ = json.at("vegetation").get<float>();
        result.buildings_ = json.at("buildings").get<float>();
        result.fenced_parcels_ = json.at("fenced_parcels").get<float>();
        result.hydrology_mode_ = *mode;
        result.river_probability_ = json.at("river_probability").get<float>();
        const Json& terrain = json.at("terrain");
        const Json& hydrology = json.at("hydrology");
        result.terrain_.preset = *terrain_preset;
        result.terrain_.sample_spacing_m = terrain.at("sample_spacing_m").get<std::uint8_t>();
        result.terrain_.elevation_range_m = terrain.at("elevation_range_m").get<float>();
        result.terrain_.landform_scale_m = terrain.at("landform_scale_m").get<float>();
        result.terrain_.roughness = terrain.at("roughness").get<float>();
        result.hydrology_.main_river_min = hydrology.at("main_river_min").get<std::uint8_t>();
        result.hydrology_.main_river_max = hydrology.at("main_river_max").get<std::uint8_t>();
        result.hydrology_.tributary_density = *tributary_density;
        result.hydrology_.stream_width_min_m = hydrology.at("stream_width_min_m").get<float>();
        result.hydrology_.stream_width_max_m = hydrology.at("stream_width_max_m").get<float>();
        result.hydrology_.river_width_min_m = hydrology.at("river_width_min_m").get<float>();
        result.hydrology_.river_width_max_m = hydrology.at("river_width_max_m").get<float>();
        result.hydrology_.depth_min_m = hydrology.at("depth_min_m").get<float>();
        result.hydrology_.depth_max_m = hydrology.at("depth_max_m").get<float>();
        result.hydrology_.meander_strength = hydrology.at("meander_strength").get<float>();
        result.hydrology_.valley_width_min_m = hydrology.at("valley_width_min_m").get<float>();
        result.hydrology_.valley_width_max_m = hydrology.at("valley_width_max_m").get<float>();
        if (!result.makeDefaultRequest().valid()) {
            return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
                profileError("invalid world generation profile values"));
        }

        document.value().provenance.source_id = result.id_;
        content::ContentSnapshotBuilder snapshot_builder{
            "world.generation.profile", WorldGenerationProfileSchemaVersion};
        if (auto added = snapshot_builder.add(std::move(document.value().provenance)); !added) {
            return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
                added.error());
        }
        auto snapshot = std::move(snapshot_builder).freeze();
        if (!snapshot) {
            return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
                snapshot.error());
        }
        result.content_snapshot_ = std::move(snapshot.value());
        const std::array<foundation::CanonicalConfigField, 25U> canonical_fields{{
            {"id", foundation::stableHashString(result.id_)},
            {"default_seed", result.default_seed_},
            {"map_size_m", result.map_size_m_},
            {"vegetation", foundation::stableHashFloat(result.vegetation_)},
            {"buildings", foundation::stableHashFloat(result.buildings_)},
            {"fenced_parcels", foundation::stableHashFloat(result.fenced_parcels_)},
            {"hydrology_mode", static_cast<std::uint64_t>(result.hydrology_mode_)},
            {"river_probability",
             foundation::stableHashFloat(result.river_probability_)},
            {"terrain_preset", static_cast<std::uint64_t>(result.terrain_.preset)},
            {"terrain_spacing", result.terrain_.sample_spacing_m},
            {"terrain_elevation", foundation::stableHashFloat(result.terrain_.elevation_range_m)},
            {"terrain_scale", foundation::stableHashFloat(result.terrain_.landform_scale_m)},
            {"terrain_roughness", foundation::stableHashFloat(result.terrain_.roughness)},
            {"river_width_min", foundation::stableHashFloat(result.hydrology_.river_width_min_m)},
            {"river_width_max", foundation::stableHashFloat(result.hydrology_.river_width_max_m)},
            {"main_river_min", result.hydrology_.main_river_min},
            {"main_river_max", result.hydrology_.main_river_max},
            {"tributary_density",
             static_cast<std::uint64_t>(result.hydrology_.tributary_density)},
            {"stream_width_min",
             foundation::stableHashFloat(result.hydrology_.stream_width_min_m)},
            {"stream_width_max",
             foundation::stableHashFloat(result.hydrology_.stream_width_max_m)},
            {"depth_min", foundation::stableHashFloat(result.hydrology_.depth_min_m)},
            {"depth_max", foundation::stableHashFloat(result.hydrology_.depth_max_m)},
            {"meander_strength",
             foundation::stableHashFloat(result.hydrology_.meander_strength)},
            {"valley_width_min",
             foundation::stableHashFloat(result.hydrology_.valley_width_min_m)},
            {"valley_width_max",
             foundation::stableHashFloat(result.hydrology_.valley_width_max_m)},
        }};
        result.fingerprint_ = foundation::makeSimConfigHash(
            "world.generation.profile.v3", canonical_fields);
        result.frozen_ = true;
        return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::success(
            std::move(result));
    } catch (const std::exception&) {
        return foundation::Result<FrozenWorldGenerationProfile, foundation::Error>::failure(
            profileError("invalid world generation profile document"));
    }
}

} // namespace genomes::world
