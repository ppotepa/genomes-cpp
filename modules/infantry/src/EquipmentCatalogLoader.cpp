#include <genomes/infantry/EquipmentCatalog.hpp>

#include <genomes/foundation/StableHash.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#include <nlohmann/json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <set>
#include <string>

namespace genomes::infantry {

namespace {

// The exported catalog contains the immutable reference geometry index.  It is
// larger than the generic 1 MiB content default, but remains explicitly
// bounded at this domain boundary rather than disabling content limits.
constexpr std::size_t kEquipmentCatalogReadLimit = 4U * 1024U * 1024U;

constexpr std::string_view kReferenceSchema = "genomes.infantry.reference.v1";
constexpr std::string_view kEquipmentSchema = "EQUIPMENT/v1";

[[nodiscard]] foundation::Error catalogError(foundation::ErrorCode code,
                                              std::string_view message) noexcept {
    return {code, message};
}

[[nodiscard]] bool exactFields(const nlohmann::json& value,
                               const std::set<std::string>& fields) noexcept {
    if (!value.is_object() || value.size() != fields.size()) {
        return false;
    }
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (!fields.contains(key)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const EquipmentSlotDefinition* nativeSlot(std::string_view identifier) noexcept {
    for (const auto& value : EquipmentCatalog::slots()) {
        if (value.identifier == identifier) {
            return &value;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<EquipmentKind> kindFromJson(const nlohmann::json& value) {
    if (!value.is_string()) {
        return std::nullopt;
    }
    const auto kind = value.get<std::string>();
    static constexpr std::array names{
        std::pair{"cap", EquipmentKind::Cap},
        std::pair{"helmet", EquipmentKind::Helmet},
        std::pair{"eyewear", EquipmentKind::Eyewear},
        std::pair{"mask", EquipmentKind::Mask},
        std::pair{"neckwear", EquipmentKind::Neckwear},
        std::pair{"clothing", EquipmentKind::Clothing},
        std::pair{"armor", EquipmentKind::Armor},
        std::pair{"rig", EquipmentKind::Rig},
        std::pair{"pack", EquipmentKind::Pack},
        std::pair{"belt", EquipmentKind::Belt},
        std::pair{"pouch", EquipmentKind::Pouch},
        std::pair{"weapon", EquipmentKind::Weapon},
    };
    for (const auto& [name, result] : names) {
        if (kind == name) {
            return result;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool nearlyEqual(float left, float right) noexcept {
    return std::isfinite(left) && std::isfinite(right) && std::abs(left - right) <= 1.0e-5F;
}

[[nodiscard]] bool visualParity(const nlohmann::json& value,
                                const EquipmentVisualDefinition& expected) {
    static const std::set<std::string> fields{
        "style", "coverage", "ease", "width", "shaft", "thickness", "size", "count",
        "pads", "roll"};
    if (!exactFields(value, fields) && (!value.is_object() || value.size() > fields.size())) {
        return false;
    }
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (!fields.contains(key)) {
            return false;
        }
    }
    const auto stringField = [&value](std::string_view key, std::string_view expected_value) {
        return !value.contains(std::string{key}) ||
               (value.at(std::string{key}).is_string() &&
                value.at(std::string{key}).get<std::string>() == expected_value);
    };
    const auto numberField = [&value](std::string_view key, float expected_value) {
        return !value.contains(std::string{key}) ||
               (value.at(std::string{key}).is_number() &&
                nearlyEqual(value.at(std::string{key}).get<float>(), expected_value));
    };
    if (!stringField("style", expected.style) || !stringField("coverage", expected.coverage) ||
        !numberField("ease", expected.ease) || !numberField("width", expected.width) ||
        !numberField("shaft", expected.shaft) || !numberField("thickness", expected.thickness)) {
        return false;
    }
    if (value.contains("size")) {
        const auto& size = value.at("size");
        if (!size.is_array() || size.size() != 3U ||
            !size.at(0).is_number() || !size.at(1).is_number() || !size.at(2).is_number() ||
            !nearlyEqual(size.at(0).get<float>(), expected.size.x) ||
            !nearlyEqual(size.at(1).get<float>(), expected.size.y) ||
            !nearlyEqual(size.at(2).get<float>(), expected.size.z)) {
            return false;
        }
    }
    if (value.contains("count") &&
        (!value.at("count").is_number_unsigned() ||
         value.at("count").get<std::uint8_t>() != expected.count)) {
        return false;
    }
    if (value.contains("pads") &&
        (!value.at("pads").is_boolean() || value.at("pads").get<bool>() != expected.pads)) {
        return false;
    }
    if (value.contains("roll") &&
        (!value.at("roll").is_boolean() || value.at("roll").get<bool>() != expected.roll)) {
        return false;
    }
    return true;
}

[[nodiscard]] std::uint64_t equipmentFingerprint(
    std::span<const EquipmentSlotDefinition> slots,
    std::span<const EquipmentItemDefinition> items,
    std::span<const InfantryLoadout> loadouts,
    std::string_view source_commit) noexcept {
    std::uint64_t hash = foundation::stableHashString(kEquipmentSchema);
    hash = foundation::stableHashCombine(hash, foundation::stableHashString(source_commit));
    for (const auto& value : slots) {
        hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(value.slot));
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.identifier));
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.required_item));
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.socket));
    }
    for (const auto& value : items) {
        hash = foundation::stableHashCombine(hash, value.id);
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.identifier));
        hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(value.kind));
        hash = foundation::stableHashCombine(hash, value.allowed_slot_count);
        for (std::size_t index = 0U; index < value.allowed_slot_count; ++index) {
            hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(value.allowed_slots[index]));
        }
        for (const float scalar : {value.weight_kg, value.fit_scale, value.fit_thickness,
                                   value.visual.ease, value.visual.width, value.visual.shaft,
                                   value.visual.thickness, value.visual.size.x, value.visual.size.y,
                                   value.visual.size.z}) {
            hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(scalar));
        }
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.style));
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.visual.style));
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.visual.coverage));
        hash = foundation::stableHashCombine(hash, value.visual.count);
        hash = foundation::stableHashCombine(hash, value.visual.pads ? 1U : 0U);
        hash = foundation::stableHashCombine(hash, value.visual.roll ? 1U : 0U);
    }
    for (const auto& value : loadouts) {
        hash = foundation::stableHashCombine(hash, value.id);
        hash = foundation::stableHashCombine(hash, foundation::stableHashString(value.identifier));
        for (const auto& choice : value.choices) {
            hash = foundation::stableHashCombine(hash, choice.count);
            for (std::size_t index = 0U; index < choice.count; ++index) {
                hash = foundation::stableHashCombine(hash, choice.definitions[index]);
            }
        }
    }
    return hash;
}

[[nodiscard]] bool choiceParity(const nlohmann::json& value,
                                const LoadoutChoice& expected) {
    if (value.is_string()) {
        return expected.count == 1U &&
               expected.definitions[0] == foundation::stable_id(value.get<std::string>());
    }
    if (!value.is_array() || value.size() != expected.count || value.size() > expected.definitions.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < expected.count; ++index) {
        if (value.at(index).is_null()) {
            if (expected.definitions[index] != 0U) return false;
        } else if (value.at(index).is_string()) {
            if (expected.definitions[index] != foundation::stable_id(
                    value.at(index).get<std::string>())) return false;
        } else {
            return false;
        }
    }
    return true;
}

} // namespace

const EquipmentItemDefinition* FrozenEquipmentCatalog::findItem(
    foundation::StableId id) const noexcept {
    const auto iterator = std::find_if(items_.begin(), items_.end(),
                                       [id](const auto& value) { return value.id == id; });
    return iterator == items_.end() ? nullptr : &*iterator;
}

foundation::Result<FrozenEquipmentCatalog, foundation::Error> loadEquipmentCatalog(
    const std::filesystem::path& path) {
    auto document = content::readContentText(path,
                                              content::ContentReadLimits{
                                                  kEquipmentCatalogReadLimit});
    if (!document) {
        return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
            document.error());
    }
    try {
        const nlohmann::json json = nlohmann::json::parse(document.value().text);
        static const std::set<std::string> fields{
            "schema", "provenance", "request", "equipmentSchema", "equipmentState",
            "equipmentFit", "slots", "items", "loadouts", "gear"};
        if (!exactFields(json, fields) || json.at("schema").get<std::string>() != kReferenceSchema ||
            json.at("equipmentSchema").get<std::string>() != kEquipmentSchema ||
            !json.at("provenance").is_object() ||
            !json.at("provenance").contains("sourceCommit") ||
            !json.at("provenance").at("sourceCommit").is_string()) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "invalid infantry equipment catalog fixture header"));
        }

        FrozenEquipmentCatalog result{};
        result.source_commit_ = json.at("provenance").at("sourceCommit").get<std::string>();
        if (result.source_commit_.empty()) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "infantry equipment catalog source commit is empty"));
        }

        const auto& slots = json.at("slots");
        if (!slots.is_array() || slots.size() != EquipmentCatalog::slots().size()) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "infantry equipment catalog slot count mismatch"));
        }
        static const std::set<std::string> slot_fields{"id", "required", "socket"};
        for (std::size_t index = 0U; index < slots.size(); ++index) {
            const auto& value = slots.at(index);
            if (!exactFields(value, slot_fields) || !value.at("id").is_string()) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid infantry equipment slot fixture entry"));
            }
            const auto* expected = nativeSlot(value.at("id").get<std::string>());
            if (expected == nullptr || expected->slot != static_cast<EquipmentSlot>(index)) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "infantry equipment slot order mismatch"));
            }
            const auto& required = value.at("required");
            const auto& socket = value.at("socket");
            const bool required_parity = required.is_null()
                                             ? expected->required_item.empty()
                                             : required.is_string() &&
                                                   required.get<std::string>() == expected->required_item;
            const bool socket_parity = socket.is_null()
                                           ? expected->socket.empty()
                                           : socket.is_string() &&
                                                 socket.get<std::string>() == expected->socket;
            if (!required_parity || !socket_parity) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "infantry equipment slot parity mismatch"));
            }
            result.slots_.push_back(*expected);
        }

        const auto& items = json.at("items");
        if (!items.is_array() || items.size() != EquipmentCatalog::items().size()) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "infantry equipment catalog item count mismatch"));
        }
        static const std::set<std::string> item_fields{"id", "slots", "kind", "weightKg", "visual"};
        static const std::set<std::string> visual_fields{
            "style", "coverage", "ease", "width", "shaft", "thickness", "size", "count",
            "pads", "roll"};
        std::set<std::string> seen_items;
        for (const auto& value : items) {
            if (!exactFields(value, item_fields) || !value.at("id").is_string() ||
                !value.at("slots").is_array() || !value.at("kind").is_string() ||
                !value.at("weightKg").is_number() || !value.at("visual").is_object()) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid infantry equipment item fixture entry"));
            }
            const auto identifier = value.at("id").get<std::string>();
            const auto* expected = EquipmentCatalog::findItem(identifier);
            const auto kind = kindFromJson(value.at("kind"));
            if (expected == nullptr || !kind.has_value() || !seen_items.insert(identifier).second ||
                expected->kind != *kind || !nearlyEqual(expected->weight_kg,
                                                        value.at("weightKg").get<float>()) ||
                !visualParity(value.at("visual"), expected->visual)) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "infantry equipment item parity mismatch"));
            }
            if (!exactFields(value.at("visual"), visual_fields) &&
                value.at("visual").size() > visual_fields.size()) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "unknown infantry equipment visual field"));
            }
            const auto& allowed = value.at("slots");
            if (allowed.size() != expected->allowed_slot_count) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "infantry equipment allowed-slot count mismatch"));
            }
            for (std::size_t index = 0U; index < allowed.size(); ++index) {
                if (!allowed.at(index).is_string()) {
                    return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                        catalogError(foundation::ErrorCode::InvalidArgument,
                                     "invalid infantry equipment allowed slot"));
                }
                const auto* slot = nativeSlot(allowed.at(index).get<std::string>());
                if (slot == nullptr || slot->slot != expected->allowed_slots[index]) {
                    return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                        catalogError(foundation::ErrorCode::InvalidArgument,
                                     "infantry equipment allowed-slot parity mismatch"));
                }
            }
            result.items_.push_back(*expected);
        }

        const auto& loadouts = json.at("loadouts");
        if (!loadouts.is_array() || loadouts.size() != infantryLoadouts().size()) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                catalogError(foundation::ErrorCode::InvalidArgument,
                             "infantry equipment loadout count mismatch"));
        }
        static const std::set<std::string> loadout_fields{"id", "slots"};
        std::set<std::string> seen_loadouts;
        for (const auto& value : loadouts) {
            if (!exactFields(value, loadout_fields) || !value.at("id").is_string() ||
                !value.at("slots").is_object()) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "invalid infantry equipment loadout fixture entry"));
            }
            const auto identifier = value.at("id").get<std::string>();
            const auto* expected = findInfantryLoadout(foundation::stable_id(identifier));
            if (expected == nullptr || expected->identifier != identifier ||
                !seen_loadouts.insert(identifier).second) {
                return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                    catalogError(foundation::ErrorCode::InvalidArgument,
                                 "infantry equipment loadout identity mismatch"));
            }
            for (const auto& [key, choice] : value.at("slots").items()) {
                const auto* slot = nativeSlot(key);
                if (slot == nullptr || !choiceParity(choice, expected->choices[equipmentSlotIndex(slot->slot)])) {
                    return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                        catalogError(foundation::ErrorCode::InvalidArgument,
                                     "infantry equipment loadout choice mismatch"));
                }
            }
            for (const auto& slot : EquipmentCatalog::slots()) {
                if (!value.at("slots").contains(slot.identifier) &&
                    expected->choices[equipmentSlotIndex(slot.slot)].count != 0U) {
                    return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
                        catalogError(foundation::ErrorCode::InvalidArgument,
                                     "infantry equipment loadout omits a choice"));
                }
            }
            result.loadouts_.push_back(*expected);
        }

        document.value().provenance.source_id = result.source_commit_;
        content::ContentSnapshotBuilder snapshot_builder{"infantry.equipment.catalog", 1U};
        auto added = snapshot_builder.add(std::move(document.value().provenance));
        if (!added) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(added.error());
        }
        auto snapshot = std::move(snapshot_builder).freeze();
        if (!snapshot) {
            return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(snapshot.error());
        }
        result.snapshot_ = std::move(snapshot.value());
        result.fingerprint_ = {equipmentFingerprint(result.slots_, result.items_, result.loadouts_,
                                                    result.source_commit_)};
        result.frozen_ = true;
        return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::success(std::move(result));
    } catch (const std::exception&) {
        return foundation::Result<FrozenEquipmentCatalog, foundation::Error>::failure(
            catalogError(foundation::ErrorCode::InvalidArgument,
                         "invalid infantry equipment catalog fixture document"));
    }
}

} // namespace genomes::infantry
