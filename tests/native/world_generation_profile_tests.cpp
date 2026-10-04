#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::filesystem::path writeDocument(const std::filesystem::path& directory,
                                    std::string_view name,
                                    std::string_view text) {
    const auto path = directory / name;
    std::ofstream stream(path, std::ios::binary);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    return path;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace genomes;
    const fs::path profile_path =
        fs::path{GENOMES_SOURCE_DIR} / "mods/core/profiles/world-generation.json";
    const auto loaded = world::loadWorldGenerationProfile(profile_path);
    assert(loaded);
    const auto& profile = loaded.value();
    assert(profile.frozen());
    assert(profile.id() == "world-generation-default");
    assert(profile.source() == profile_path);
    assert(profile.contentSnapshot().sources.size() == 1U);
    assert(profile.contentSnapshot().sources.front().source_id == profile.id());
    assert(profile.fingerprint().value != 0U);

    const auto request = profile.makeDefaultRequest();
    assert(request.valid());
    assert(request.seed == 0x5EED2026ULL);
    assert(request.map_size_m == 600U);
    assert(request.vegetation == 0.62F);
    assert(request.buildings == 0.55F);
    assert(request.fenced_parcels == 0.48F);
    assert(request.hydrology_mode == hydrology::HydrologyMode::SeededOptional);
    assert(request.river_probability == 0.35F);
    assert(request.terrain.preset == world::TerrainPreset::CombatMixed);
    assert(request.terrain.sample_spacing_m == 8U);
    assert(request.hydrology.tributary_density == world::TributaryDensity::Low);

    const auto changed_seed = profile.makeRequest(0x1234ULL);
    assert(changed_seed.valid());
    assert(changed_seed.seed == 0x1234ULL);
    assert(changed_seed.map_size_m == request.map_size_m);
    assert(!profile.makeRequest(0U).valid());

    const auto bounded = world::loadWorldGenerationProfile(
        profile_path, content::ContentReadLimits{8U});
    assert(!bounded);
    assert(bounded.error().code == foundation::ErrorCode::OutOfRange);

    const auto second = world::loadWorldGenerationProfile(profile_path);
    assert(second);
    assert(second.value().fingerprint() == profile.fingerprint());

    const fs::path temporary =
        fs::temp_directory_path() / "genomes-world-generation-profile-tests";
    std::error_code error;
    fs::remove_all(temporary, error);
    assert(fs::create_directories(temporary, error) && !error);

    const std::string reordered = R"json({
      "river_probability":0.35,"hydrology_mode":"seeded-optional",
      "fenced_parcels":0.48,"buildings":0.55,"vegetation":0.62,
      "map_size_m":600,"default_seed":1592598566,
      "hydrology":{"valley_width_max_m":80.0,"valley_width_min_m":20.0,
        "meander_strength":0.4,"depth_max_m":2.0,"depth_min_m":0.3,
        "river_width_max_m":14.0,"river_width_min_m":5.0,
        "stream_width_max_m":4.0,"stream_width_min_m":1.0,
        "tributary_density":"low","main_river_max":1,"main_river_min":0},
      "terrain":{"roughness":0.35,"landform_scale_m":600.0,"sample_spacing_m":8,
        "elevation_range_m":70.0,"preset":"combat-mixed"},
      "id":"world-generation-default","schema_version":3
    })json";
    const auto reordered_profile = world::loadWorldGenerationProfile(
        writeDocument(temporary, "reordered.json", reordered));
    assert(reordered_profile);
    assert(reordered_profile.value().fingerprint() == profile.fingerprint());

    const std::string unknown_field = R"json({
      "schema_version":1,"id":"world-generation-default",
      "default_seed":1592598566,"map_size_m":600,"vegetation":0.62,
      "buildings":0.55,"fenced_parcels":0.48,
      "hydrology_mode":"seeded-optional","river_probability":0.35,
      "unexpected":true
    })json";
    assert(!world::loadWorldGenerationProfile(
        writeDocument(temporary, "unknown.json", unknown_field)));

    const std::string invalid_range = R"json({
      "schema_version":1,"id":"world-generation-default",
      "default_seed":1592598566,"map_size_m":601,"vegetation":0.62,
      "buildings":0.55,"fenced_parcels":0.48,
      "hydrology_mode":"seeded-optional","river_probability":0.35
    })json";
    assert(!world::loadWorldGenerationProfile(
        writeDocument(temporary, "invalid-range.json", invalid_range)));

    const std::string invalid_seed = R"json({
      "schema_version":1,"id":"world-generation-default",
      "default_seed":0,"map_size_m":600,"vegetation":0.62,
      "buildings":0.55,"fenced_parcels":0.48,
      "hydrology_mode":"seeded-optional","river_probability":0.35
    })json";
    assert(!world::loadWorldGenerationProfile(
        writeDocument(temporary, "invalid-seed.json", invalid_seed)));

    fs::remove_all(temporary, error);
    return 0;
}
