#include <genomes/infantry/AppearanceCatalog.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <exception>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace genomes::infantry {

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

[[nodiscard]] std::optional<AppearanceMaterialRegion> materialRegion(
    const nlohmann::json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto name = value.get<std::string>();
    for (std::uint16_t index = 0U; index <= 17U; ++index) {
        const auto region = static_cast<AppearanceMaterialRegion>(index);
        if (materialRegionName(region) == name) return region;
    }
    return std::nullopt;
}

[[nodiscard]] bool validColor(const foundation::Color& color) noexcept {
    return std::isfinite(color.r) && std::isfinite(color.g) &&
           std::isfinite(color.b) && std::isfinite(color.a) &&
           color.r >= 0.0F && color.r <= 1.0F &&
           color.g >= 0.0F && color.g <= 1.0F &&
           color.b >= 0.0F && color.b <= 1.0F &&
           color.a >= 0.0F && color.a <= 1.0F;
}

[[nodiscard]] foundation::Error catalogError(std::string_view message) noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

} // namespace

const AppearancePresetDefinition* FrozenAppearanceCatalog::find(
    foundation::StableId id) const noexcept {
    for (const auto& definition : presets_) {
        if (definition.id == id) return &definition;
    }
    return nullptr;
}

foundation::Result<FrozenAppearanceCatalog, foundation::Error>
loadAppearanceCatalog(const std::filesystem::path& path) {
    auto document = content::readContentText(path);
    if (!document) {
        return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
            document.error());
    }
    try {
        const nlohmann::json json = nlohmann::json::parse(document.value().text);
        static const std::set<std::string> root_fields{"schema_version", "id", "presets"};
        static const std::set<std::string> preset_fields{"id", "schema_version",
                                                           "material_region", "color"};
        if (!exactFields(json, root_fields) || json.at("schema_version") != 1 ||
            !json.at("id").is_string() || json.at("id").get<std::string>().empty() ||
            !json.at("presets").is_array() || json.at("presets").empty()) {
            return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                catalogError("invalid appearance catalog schema"));
        }

        FrozenAppearanceCatalog result{};
        const auto catalog_id = json.at("id").get<std::string>();
        for (const auto& value : json.at("presets")) {
            if (!exactFields(value, preset_fields) || !value.at("id").is_string() ||
                value.at("id").get<std::string>().empty() ||
                value.at("schema_version") != kAppearancePresetSchemaVersion ||
                !value.at("color").is_array() || value.at("color").size() != 4U) {
                return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                    catalogError("invalid appearance preset schema"));
            }
            const auto region = materialRegion(value.at("material_region"));
            if (!region) {
                return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                    catalogError("invalid appearance material region"));
            }
            foundation::Color color{};
            for (std::size_t index = 0U; index < 4U; ++index) {
                if (!value.at("color").at(index).is_number()) {
                    return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                        catalogError("invalid appearance color"));
                }
            }
            color.r = value.at("color").at(0).get<float>();
            color.g = value.at("color").at(1).get<float>();
            color.b = value.at("color").at(2).get<float>();
            color.a = value.at("color").at(3).get<float>();
            const auto id = foundation::stable_id(value.at("id").get<std::string>());
            if (id == 0U || !validColor(color) || result.find(id) != nullptr) {
                return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                    catalogError("invalid or duplicate appearance preset"));
            }
            result.presets_.push_back({id, kAppearancePresetSchemaVersion, *region, color});
        }
        document.value().provenance.source_id = catalog_id;
        content::ContentSnapshotBuilder snapshot_builder{"infantry.appearance.catalog", 1U};
        if (auto added = snapshot_builder.add(std::move(document.value().provenance)); !added) {
            return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                added.error());
        }
        auto snapshot = std::move(snapshot_builder).freeze();
        if (!snapshot) {
            return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
                snapshot.error());
        }
        result.snapshot_ = std::move(snapshot.value());
        std::vector<foundation::CanonicalConfigField> fields;
        std::vector<std::string> names;
        fields.reserve(result.presets_.size() * 7U + 1U);
        names.reserve(result.presets_.size() * 7U + 1U);
        names.emplace_back("id");
        fields.push_back({"id", foundation::stableHashString(catalog_id)});
        for (std::size_t index = 0U; index < result.presets_.size(); ++index) {
            const auto& preset = result.presets_[index];
            const auto prefix = std::string{"preset."} + std::to_string(index) + ".";
            names.push_back(prefix + "id");
            fields.push_back({names.back(), preset.id});
            names.push_back(prefix + "schema_version");
            fields.push_back({names.back(), preset.schema_version});
            names.push_back(prefix + "material_region");
            fields.push_back({names.back(),
                              static_cast<std::uint16_t>(preset.material_region)});
            names.push_back(prefix + "r");
            fields.push_back({names.back(), foundation::stableHashFloat(preset.color.r)});
            names.push_back(prefix + "g");
            fields.push_back({names.back(), foundation::stableHashFloat(preset.color.g)});
            names.push_back(prefix + "b");
            fields.push_back({names.back(), foundation::stableHashFloat(preset.color.b)});
            names.push_back(prefix + "a");
            fields.push_back({names.back(), foundation::stableHashFloat(preset.color.a)});
        }
        result.fingerprint_ = foundation::makePresentationConfigHash(
            "infantry.appearance.v1", fields);
        result.frozen_ = true;
        return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::success(
            std::move(result));
    } catch (const std::exception&) {
        return foundation::Result<FrozenAppearanceCatalog, foundation::Error>::failure(
            catalogError("invalid appearance catalog document"));
    }
}

} // namespace genomes::infantry
