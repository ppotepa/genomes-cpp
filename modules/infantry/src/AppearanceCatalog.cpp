#include <genomes/infantry/AppearanceCatalog.hpp>

#include <array>
#include <cmath>

namespace genomes::infantry {

namespace {

const std::array<AppearancePresetDefinition, 1U>& definitions() noexcept {
    static constexpr std::array<AppearancePresetDefinition, 1U> value{{
        {kInspectionOliveAppearancePreset, kAppearancePresetSchemaVersion,
         AppearanceMaterialRegion::UniformCloth,
         foundation::Color{0.20F, 0.25F, 0.16F, 1.0F}},
    }};
    return value;
}

} // namespace

std::span<const AppearancePresetDefinition> appearancePresets() noexcept {
    const auto& value = definitions();
    return {value.data(), value.size()};
}

const AppearancePresetDefinition* findAppearancePreset(foundation::StableId id) noexcept {
    for (const auto& definition : definitions()) {
        if (definition.id == id) {
            return &definition;
        }
    }
    return nullptr;
}

foundation::Result<void, foundation::Error> validateAppearanceCatalog() {
    const auto values = appearancePresets();
    for (std::size_t index = 0U; index < values.size(); ++index) {
        const auto& definition = values[index];
        if (definition.id == 0U || definition.schema_version == 0U ||
            !std::isfinite(definition.color.r) || !std::isfinite(definition.color.g) ||
            !std::isfinite(definition.color.b) || !std::isfinite(definition.color.a) ||
            definition.color.r < 0.0F || definition.color.r > 1.0F ||
            definition.color.g < 0.0F || definition.color.g > 1.0F ||
            definition.color.b < 0.0F || definition.color.b > 1.0F ||
            definition.color.a < 0.0F || definition.color.a > 1.0F) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "invalid appearance preset definition"});
        }
        for (std::size_t previous = 0U; previous < index; ++previous) {
            if (definition.id == values[previous].id) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState,
                     "duplicate appearance preset identity"});
            }
        }
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::infantry
