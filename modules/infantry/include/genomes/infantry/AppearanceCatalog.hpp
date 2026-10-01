#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <cstdint>
#include <span>

namespace genomes::infantry {

inline constexpr foundation::StableId kInspectionOliveAppearancePreset =
    foundation::stable_id("appearance.inspection-olive");
inline constexpr std::uint32_t kAppearancePresetSchemaVersion = 1U;

// Presentation presets are immutable domain data.  The runtime adapter owns
// the rendering operation, while this catalog owns the stable identity,
// schema and material intent of each preset.
struct AppearancePresetDefinition final {
    foundation::StableId id{0};
    std::uint32_t schema_version{0};
    AppearanceMaterialRegion material_region{AppearanceMaterialRegion::UniformCloth};
    foundation::Color color{};
};

[[nodiscard]] std::span<const AppearancePresetDefinition> appearancePresets() noexcept;
[[nodiscard]] const AppearancePresetDefinition* findAppearancePreset(
    foundation::StableId id) noexcept;
[[nodiscard]] foundation::Result<void, foundation::Error> validateAppearanceCatalog();

} // namespace genomes::infantry
