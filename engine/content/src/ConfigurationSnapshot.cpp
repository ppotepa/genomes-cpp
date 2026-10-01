#include <genomes/content/ConfigurationSnapshot.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

namespace genomes::content {
namespace {

using Json = nlohmann::json;

[[nodiscard]] foundation::Error error(foundation::ErrorCode code,
                                      std::string_view message) noexcept {
    return {code, message};
}

[[nodiscard]] int kindRank(ConfigurationLayerKind kind) noexcept {
    return static_cast<int>(kind);
}

[[nodiscard]] bool scalar(const Json& value) noexcept {
    return value.is_boolean() || value.is_number_integer() || value.is_number_unsigned() ||
           value.is_number_float() || value.is_string();
}

[[nodiscard]] std::uint64_t valueHash(const ConfigurationValue& value) {
    return std::visit([](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) {
            return foundation::stableHashCombine(1U, item ? 1U : 0U);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return foundation::stableHashCombine(2U, static_cast<std::uint64_t>(item));
        } else if constexpr (std::is_same_v<T, double>) {
            std::uint64_t bits = 0U;
            static_assert(sizeof(bits) == sizeof(item));
            std::memcpy(&bits, &item, sizeof(bits));
            return foundation::stableHashCombine(3U, bits);
        } else {
            return foundation::stableHashCombine(4U, foundation::stableHashString(item));
        }
    }, value);
}

[[nodiscard]] foundation::Result<ConfigurationValue, foundation::Error>
parseValue(const Json& value) {
    if (!scalar(value)) {
        return foundation::Result<ConfigurationValue, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument,
                  "configuration values must be scalar"));
    }
    if (value.is_boolean()) {
        return foundation::Result<ConfigurationValue, foundation::Error>::success(
            value.get<bool>());
    }
    if (value.is_number_integer() || value.is_number_unsigned()) {
        try {
            return foundation::Result<ConfigurationValue, foundation::Error>::success(
                value.get<std::int64_t>());
        } catch (...) {
            return foundation::Result<ConfigurationValue, foundation::Error>::failure(
                error(foundation::ErrorCode::OutOfRange,
                      "configuration integer is out of range"));
        }
    }
    if (value.is_number_float()) {
        const double number = value.get<double>();
        if (!std::isfinite(number)) {
            return foundation::Result<ConfigurationValue, foundation::Error>::failure(
                error(foundation::ErrorCode::InvalidArgument,
                      "configuration number must be finite"));
        }
        return foundation::Result<ConfigurationValue, foundation::Error>::success(number);
    }
    return foundation::Result<ConfigurationValue, foundation::Error>::success(
        value.get<std::string>());
}

} // namespace

const ResolvedConfigurationField* FrozenConfigurationSnapshot::find(
    std::string_view name) const noexcept {
    const auto it = std::lower_bound(fields.begin(), fields.end(), name,
        [](const auto& field, std::string_view key) { return field.name < key; });
    return it != fields.end() && it->name == name ? &*it : nullptr;
}

foundation::Result<void, foundation::Error>
ConfigurationResolver::addLayer(ConfigurationLayer layer) {
    if (layer.id.empty() || layer.document.empty() ||
        std::any_of(layers_.begin(), layers_.end(),
                    [&](const auto& item) { return item.id == layer.id; })) {
        return foundation::Result<void, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument,
                  "configuration layer id must be unique and non-empty"));
    }
    layers_.push_back(std::move(layer));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<FrozenConfigurationSnapshot, foundation::Error>
ConfigurationResolver::resolve(const ConfigurationSchema& schema) && {
    if (layers_.empty()) {
        return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidState, "configuration has no layers"));
    }

    std::map<std::string, std::size_t, std::less<>> by_id;
    for (std::size_t index = 0; index < layers_.size(); ++index) {
        by_id.emplace(layers_[index].id, index);
    }
    for (const auto& layer : layers_) {
        for (const auto& dependency : layer.dependencies) {
            if (!by_id.contains(dependency)) {
                return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                    error(foundation::ErrorCode::NotFound,
                          "configuration layer dependency is missing"));
            }
        }
    }

    std::vector<std::size_t> order;
    std::vector<bool> emitted(layers_.size(), false);
    while (order.size() != layers_.size()) {
        std::optional<std::size_t> best;
        for (std::size_t index = 0; index < layers_.size(); ++index) {
            if (emitted[index] || std::any_of(
                    layers_[index].dependencies.begin(), layers_[index].dependencies.end(),
                    [&](const auto& dependency) { return !emitted[by_id.at(dependency)]; })) {
                continue;
            }
            const auto candidate = std::tuple{kindRank(layers_[index].kind),
                                              layers_[index].load_priority, layers_[index].id};
            if (!best || candidate < std::tuple{kindRank(layers_[*best].kind),
                                                layers_[*best].load_priority, layers_[*best].id}) {
                best = index;
            }
        }
        if (!best) {
            return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                error(foundation::ErrorCode::InvalidArgument,
                      "configuration layer dependencies contain a cycle"));
        }
        emitted[*best] = true;
        order.push_back(*best);
    }

    std::set<std::string, std::less<>> known;
    for (const auto& field : schema.fields) {
        if (!field.name.empty()) known.insert(field.name);
    }
    std::map<std::string, ResolvedConfigurationField, std::less<>> merged;
    for (const std::size_t index : order) {
        const auto& layer = layers_[index];
        Json document;
        try {
            document = Json::parse(layer.document);
        } catch (...) {
            return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                error(foundation::ErrorCode::InvalidArgument,
                      "configuration layer is not valid JSON"));
        }
        if (!document.is_object()) {
            return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                error(foundation::ErrorCode::InvalidArgument,
                      "configuration layer must be an object"));
        }
        for (auto it = document.begin(); it != document.end(); ++it) {
            const bool is_known = known.contains(it.key());
            if (!is_known && (layer.kind == ConfigurationLayerKind::Core ||
                              layer.kind == ConfigurationLayerKind::CommandLine)) {
                return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                    error(foundation::ErrorCode::InvalidArgument,
                          "configuration contains an unknown field"));
            }
            if (layer.kind == ConfigurationLayerKind::CommandLine &&
                std::find(schema.allowed_cli_fields.begin(), schema.allowed_cli_fields.end(),
                          it.key()) == schema.allowed_cli_fields.end()) {
                return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                    error(foundation::ErrorCode::InvalidArgument,
                          "configuration command-line field is not allowed"));
            }
            auto parsed = parseValue(it.value());
            if (!parsed) {
                return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                    parsed.error());
            }
            merged[it.key()] = {it.key(), std::move(parsed.value()), layer.id};
        }
    }

    for (const auto& field : schema.fields) {
        const auto it = merged.find(field.name);
        if (it == merged.end()) {
            if (field.required) {
                return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                    error(foundation::ErrorCode::InvalidArgument,
                          "required configuration field is missing"));
            }
            continue;
        }
        if (!field.allowed_values.empty()) {
            const auto* text = std::get_if<std::string>(&it->second.value);
            if (!text || std::find(field.allowed_values.begin(), field.allowed_values.end(), *text) ==
                            field.allowed_values.end()) {
                return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                    error(foundation::ErrorCode::InvalidArgument,
                          "configuration field value is not allowed"));
            }
        }
    }
    for (const auto& reference : schema.references) {
        const auto it = merged.find(reference.field);
        if (it == merged.end()) continue;
        const auto* text = std::get_if<std::string>(&it->second.value);
        if (!text || std::find(reference.allowed_ids.begin(), reference.allowed_ids.end(), *text) ==
                         reference.allowed_ids.end()) {
            return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::failure(
                error(foundation::ErrorCode::NotFound,
                      "configuration reference is missing"));
        }
    }

    FrozenConfigurationSnapshot snapshot{};
    for (auto& entry : merged) snapshot.fields.push_back(std::move(entry.second));
    std::vector<foundation::CanonicalConfigField> canonical;
    canonical.reserve(snapshot.fields.size());
    for (const auto& field : snapshot.fields) {
        canonical.push_back({field.name, valueHash(field.value)});
    }
    snapshot.simulation_hash = foundation::makeSimConfigHash("resolved", canonical);
    snapshot.presentation_hash = foundation::makePresentationConfigHash("resolved", canonical);
    snapshot.execution_hash = foundation::makeExecutionProfileHash("resolved", canonical);
    return foundation::Result<FrozenConfigurationSnapshot, foundation::Error>::success(
        std::move(snapshot));
}

} // namespace genomes::content
