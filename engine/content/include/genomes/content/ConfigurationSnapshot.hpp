#pragma once

#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace genomes::content {

enum class ConfigurationLayerKind : std::uint8_t {
    BootstrapDefaults,
    Core,
    NamedProfile,
    Override,
    CommandLine,
};

struct ConfigurationLayer final {
    std::string id;
    ConfigurationLayerKind kind{ConfigurationLayerKind::Override};
    int load_priority{0};
    std::vector<std::string> dependencies;
    std::string document;
};

struct ConfigurationFieldRule final {
    std::string name;
    bool required{false};
    std::vector<std::string> allowed_values;
};

struct ConfigurationReferenceRule final {
    std::string field;
    std::vector<std::string> allowed_ids;
};

struct ConfigurationSchema final {
    std::vector<ConfigurationFieldRule> fields;
    std::vector<ConfigurationReferenceRule> references;
    std::vector<std::string> allowed_cli_fields;
};

using ConfigurationValue = std::variant<bool, std::int64_t, double, std::string>;

struct ResolvedConfigurationField final {
    std::string name;
    ConfigurationValue value;
    std::string source_layer;
};

struct FrozenConfigurationSnapshot final {
    std::vector<ResolvedConfigurationField> fields;
    foundation::SimConfigHash simulation_hash{};
    foundation::PresentationConfigHash presentation_hash{};
    foundation::ExecutionProfileHash execution_hash{};

    [[nodiscard]] const ResolvedConfigurationField* find(std::string_view name) const noexcept;
};

class ConfigurationResolver final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error>
    addLayer(ConfigurationLayer layer);

    [[nodiscard]] foundation::Result<FrozenConfigurationSnapshot, foundation::Error>
    resolve(const ConfigurationSchema& schema) &&;

private:
    std::vector<ConfigurationLayer> layers_;
};

} // namespace genomes::content
