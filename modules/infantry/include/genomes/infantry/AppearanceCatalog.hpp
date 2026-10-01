#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

// Appearance values are resolved once by the application composition root.
// Runtime presentation consumes only this immutable, provenance-bearing view;
// there is no native fallback catalog after loading.
class FrozenAppearanceCatalog final {
public:
    [[nodiscard]] std::span<const AppearancePresetDefinition> presets() const noexcept {
        return {presets_.data(), presets_.size()};
    }
    [[nodiscard]] const AppearancePresetDefinition* find(
        foundation::StableId id) const noexcept;
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] const content::FrozenContentSnapshot& contentSnapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] foundation::PresentationConfigHash fingerprint() const noexcept {
        return fingerprint_;
    }

private:
    friend foundation::Result<FrozenAppearanceCatalog, foundation::Error>
    loadAppearanceCatalog(const std::filesystem::path& path);

    std::vector<AppearancePresetDefinition> presets_;
    content::FrozenContentSnapshot snapshot_{};
    foundation::PresentationConfigHash fingerprint_{};
    bool frozen_{false};
};

[[nodiscard]] foundation::Result<FrozenAppearanceCatalog, foundation::Error>
loadAppearanceCatalog(const std::filesystem::path& path);

} // namespace genomes::infantry
