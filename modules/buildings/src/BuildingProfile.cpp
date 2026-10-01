#include <genomes/buildings/BuildingProfile.hpp>

#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <exception>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::buildings {

namespace {

[[nodiscard]] bool exactFields(const nlohmann::json& value,
                               const std::set<std::string>& fields) noexcept {
    if (!value.is_object() || value.size() != fields.size()) return false;
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (!fields.contains(key)) return false;
    }
    return true;
}

[[nodiscard]] foundation::Error profileError(std::string_view message) noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] bool readFiniteFloat(const nlohmann::json& object,
                                   const char* key,
                                   float& output) {
    const auto& value = object.at(key);
    if (!value.is_number()) return false;
    output = value.get<float>();
    return std::isfinite(output);
}

[[nodiscard]] bool readUint32(const nlohmann::json& object,
                              const char* key,
                              std::uint32_t& output) {
    const auto& value = object.at(key);
    if (!value.is_number_unsigned()) return false;
    const auto parsed = value.get<std::uint64_t>();
    if (parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

} // namespace

bool BuildingSiteGenerationProfile::valid() const noexcept {
    return std::isfinite(floor_height) && floor_height > 1.5F && floor_height <= 10.0F &&
           std::isfinite(wall_thickness) && wall_thickness > 0.05F &&
           wall_thickness < 0.5F && std::isfinite(target_room_width) &&
           target_room_width >= 1.0F && target_room_width <= 100.0F &&
           minimum_rooms_per_floor > 0U &&
           minimum_rooms_per_floor <= maximum_rooms_per_floor &&
           maximum_rooms_per_floor <= 16U;
}

bool BuildingTemplateDefinition::valid() const noexcept {
    return instantiate(1U).valid();
}

BuildingSpec BuildingTemplateDefinition::instantiate(proc::Seed seed) const noexcept {
    return {id, seed, footprint, floors, floor_height, wall_thickness, rooms_per_floor};
}

foundation::Result<FrozenBuildingProfile, foundation::Error>
loadBuildingProfile(const std::filesystem::path& path) {
    constexpr std::size_t maximum_profile_bytes = 64U * 1024U;
    auto document = content::readContentText(path, {maximum_profile_bytes});
    if (!document) {
        return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
            document.error());
    }
    try {
        const nlohmann::json json = nlohmann::json::parse(document.value().text);
        static const std::set<std::string> root_fields{
            "schema_version", "id", "site_generation", "lab_preview"};
        static const std::set<std::string> site_fields{
            "floor_height", "wall_thickness", "target_room_width",
            "minimum_rooms_per_floor", "maximum_rooms_per_floor"};
        static const std::set<std::string> template_fields{
            "id", "footprint", "floors", "floor_height", "wall_thickness",
            "rooms_per_floor"};
        if (!exactFields(json, root_fields) ||
            !json.at("schema_version").is_number_unsigned() ||
            json.at("schema_version").get<std::uint64_t>() != BuildingProfileSchemaVersion ||
            !json.at("id").is_string() || json.at("id").get<std::string>().empty() ||
            !exactFields(json.at("site_generation"), site_fields) ||
            !exactFields(json.at("lab_preview"), template_fields)) {
            return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                profileError("invalid building profile schema"));
        }

        FrozenBuildingProfile result{};
        const auto profile_id = json.at("id").get<std::string>();
        const auto& site = json.at("site_generation");
        if (!readFiniteFloat(site, "floor_height", result.site_generation_.floor_height) ||
            !readFiniteFloat(site, "wall_thickness", result.site_generation_.wall_thickness) ||
            !readFiniteFloat(site, "target_room_width",
                             result.site_generation_.target_room_width) ||
            !readUint32(site, "minimum_rooms_per_floor",
                        result.site_generation_.minimum_rooms_per_floor) ||
            !readUint32(site, "maximum_rooms_per_floor",
                        result.site_generation_.maximum_rooms_per_floor) ||
            !result.site_generation_.valid()) {
            return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                profileError("invalid building site generation profile"));
        }

        const auto& preview = json.at("lab_preview");
        if (!preview.at("id").is_string() || preview.at("id").get<std::string>().empty() ||
            !preview.at("footprint").is_array() || preview.at("footprint").size() != 3U ||
            !readUint32(preview, "floors", result.lab_preview_.floors) ||
            !readFiniteFloat(preview, "floor_height", result.lab_preview_.floor_height) ||
            !readFiniteFloat(preview, "wall_thickness", result.lab_preview_.wall_thickness) ||
            !readUint32(preview, "rooms_per_floor", result.lab_preview_.rooms_per_floor)) {
            return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                profileError("invalid building preview profile"));
        }
        for (std::size_t index = 0U; index < 3U; ++index) {
            if (!preview.at("footprint").at(index).is_number()) {
                return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                    profileError("invalid building preview footprint"));
            }
        }
        result.lab_preview_.footprint = {
            preview.at("footprint").at(0U).get<float>(),
            preview.at("footprint").at(1U).get<float>(),
            preview.at("footprint").at(2U).get<float>()};
        result.lab_preview_.id = foundation::stable_id(preview.at("id").get<std::string>());
        if (!std::isfinite(result.lab_preview_.footprint.x) ||
            !std::isfinite(result.lab_preview_.footprint.y) ||
            !std::isfinite(result.lab_preview_.footprint.z) ||
            result.lab_preview_.id == 0U || !result.lab_preview_.valid()) {
            return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                profileError("invalid building preview definition"));
        }

        document.value().provenance.source_id = profile_id;
        content::ContentSnapshotBuilder snapshot_builder{"buildings.profile", 1U};
        if (auto added = snapshot_builder.add(std::move(document.value().provenance)); !added) {
            return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                added.error());
        }
        auto snapshot = std::move(snapshot_builder).freeze();
        if (!snapshot) {
            return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
                snapshot.error());
        }
        result.snapshot_ = std::move(snapshot.value());

        const auto& rules = result.site_generation_;
        const auto& lab = result.lab_preview_;
        const std::vector<foundation::CanonicalConfigField> fields{
            {"profile.id", foundation::stableHashString(profile_id)},
            {"site.floor_height", foundation::stableHashFloat(rules.floor_height)},
            {"site.wall_thickness", foundation::stableHashFloat(rules.wall_thickness)},
            {"site.target_room_width", foundation::stableHashFloat(rules.target_room_width)},
            {"site.minimum_rooms_per_floor", rules.minimum_rooms_per_floor},
            {"site.maximum_rooms_per_floor", rules.maximum_rooms_per_floor},
            {"lab.id", lab.id},
            {"lab.footprint.x", foundation::stableHashFloat(lab.footprint.x)},
            {"lab.footprint.y", foundation::stableHashFloat(lab.footprint.y)},
            {"lab.footprint.z", foundation::stableHashFloat(lab.footprint.z)},
            {"lab.floors", lab.floors},
            {"lab.floor_height", foundation::stableHashFloat(lab.floor_height)},
            {"lab.wall_thickness", foundation::stableHashFloat(lab.wall_thickness)},
            {"lab.rooms_per_floor", lab.rooms_per_floor}};
        result.fingerprint_ = foundation::makeSimConfigHash("buildings.profile.v1", fields);
        result.frozen_ = true;
        return foundation::Result<FrozenBuildingProfile, foundation::Error>::success(
            std::move(result));
    } catch (const std::exception&) {
        return foundation::Result<FrozenBuildingProfile, foundation::Error>::failure(
            profileError("invalid building profile document"));
    }
}

} // namespace genomes::buildings
