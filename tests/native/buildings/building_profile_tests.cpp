#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/buildings/BuildingModel.hpp>

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

    const fs::path canonical_path =
        fs::path{GENOMES_SOURCE_DIR} / "mods/core/profiles/building.json";
    const auto canonical = buildings::loadBuildingProfile(canonical_path);
    assert(canonical);
    assert(canonical.value().frozen());
    assert(canonical.value().fingerprint().value != 0U);
    assert(canonical.value().contentSnapshot().sources.size() == 1U);
    assert(canonical.value().contentSnapshot().sources.front().source_id ==
           "core.building-profile");
    const auto& site = canonical.value().siteGeneration();
    assert(site.floor_height == 2.8F);
    assert(site.wall_thickness == 0.25F);
    assert(site.target_room_width == 6.0F);
    assert(site.minimum_rooms_per_floor == 1U);
    assert(site.maximum_rooms_per_floor == 8U);
    const auto preview = canonical.value().labPreview().instantiate(0xB01D1A9U);
    assert(preview.building_id == foundation::stable_id("building-lab.preview"));
    assert(preview.footprint.x == 18.0F && preview.footprint.y == 1.0F &&
           preview.footprint.z == 14.0F);
    assert(preview.floors == 2U && preview.floor_height == 3.0F &&
           preview.wall_thickness == 0.30F && preview.rooms_per_floor == 3U);

    const fs::path temporary = fs::temp_directory_path() / "genomes-building-profile-tests";
    std::error_code error;
    fs::remove_all(temporary, error);
    assert(fs::create_directories(temporary, error) && !error);

    const std::string reordered = R"json({
      "lab_preview": {"rooms_per_floor":3,"wall_thickness":0.3,"floor_height":3,
        "floors":2,"footprint":[18,1,14],"id":"building-lab.preview"},
      "site_generation": {"maximum_rooms_per_floor":8,"minimum_rooms_per_floor":1,
        "target_room_width":6,"wall_thickness":0.25,"floor_height":2.8},
      "id":"core.building-profile","schema_version":1
    })json";
    const auto reordered_profile = buildings::loadBuildingProfile(
        writeDocument(temporary, "reordered.json", reordered));
    assert(reordered_profile);
    assert(reordered_profile.value().fingerprint() == canonical.value().fingerprint());

    const std::string unknown_field = R"json({
      "schema_version":1,"id":"core.building-profile","unexpected":true,
      "site_generation":{"floor_height":2.8,"wall_thickness":0.25,
        "target_room_width":6,"minimum_rooms_per_floor":1,"maximum_rooms_per_floor":8},
      "lab_preview":{"id":"building-lab.preview","footprint":[18,1,14],
        "floors":2,"floor_height":3,"wall_thickness":0.3,"rooms_per_floor":3}
    })json";
    assert(!buildings::loadBuildingProfile(
        writeDocument(temporary, "unknown.json", unknown_field)));

    const std::string invalid_range = R"json({
      "schema_version":1,"id":"core.building-profile",
      "site_generation":{"floor_height":2.8,"wall_thickness":0.25,
        "target_room_width":0,"minimum_rooms_per_floor":1,"maximum_rooms_per_floor":8},
      "lab_preview":{"id":"building-lab.preview","footprint":[18,1,14],
        "floors":2,"floor_height":3,"wall_thickness":0.3,"rooms_per_floor":3}
    })json";
    assert(!buildings::loadBuildingProfile(
        writeDocument(temporary, "invalid-range.json", invalid_range)));

    const std::string oversized(64U * 1024U + 1U, 'x');
    const auto oversized_result = buildings::loadBuildingProfile(
        writeDocument(temporary, "oversized.json", oversized));
    assert(!oversized_result);
    assert(oversized_result.error().code == foundation::ErrorCode::OutOfRange);

    fs::remove_all(temporary, error);
    return 0;
}
