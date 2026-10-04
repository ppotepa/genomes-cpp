#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/GearSurfaceGenerator.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/RigBuilder.hpp>
#include <genomes/proc/ArtifactCache.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace genomes::infantry {

enum class InfantryDetail : std::uint32_t {
    Far = 1,
    World = 2,
    High = 3,
};

enum class InfantrySide : std::uint8_t {
    SideA,
    SideB,
    Neutral,
};

struct InfantryPalette final {
    foundation::Color uniform{kDefaultUniformColor};
    foundation::Color trousers{0.16F, 0.19F, 0.15F, 1.0F};
    foundation::Color leather{0.12F, 0.08F, 0.05F, 1.0F};
    foundation::Color metal{0.22F, 0.24F, 0.23F, 1.0F};
};

struct InfantryModelRequest final {
    proc::Seed seed{0};
    double variation{1.0};
    InfantryDetail detail_level{InfantryDetail::World};
    GenomeOverrides genome_overrides{};
    foundation::StableId loadout_id{0};
    EquipmentOverrideSet equipment_overrides{};
    InfantrySide side{InfantrySide::SideA};
    InfantryPalette palette{};
    double wear{0.0};
    // Compatibility alias used by existing callers until their palette UI is
    // migrated. It is normalized into `palette.uniform` before compilation
    // and hashing.
    foundation::Color uniform_color{kDefaultUniformColor};
};

struct InfantryModelArtifact final {
    std::uint32_t version{InfantryArtifactVersion};
    InfantryGenome genome{};
    PhenotypeArtifact phenotype{};
    SkeletonData skeleton{};
    AppearanceArtifact appearance{};
    EquipmentState equipment{};
    EquipmentFit equipment_fit{};
    GearArtifact gear{};
    AppearanceMesh gear_surface{};
    foundation::StableId cache_key{0};
};

struct InfantryModelCompileResult final {
    std::shared_ptr<const InfantryModelArtifact> artifact;
    proc::ArtifactKey artifact_key{};
};

class InfantryModelCompiler final {
public:
    [[nodiscard]] static foundation::StableId canonicalRequestKey(
        const InfantryModelRequest&) noexcept;
    [[nodiscard]] static proc::ArtifactKey artifactKey(
        const InfantryModelRequest&) noexcept;

    [[nodiscard]] foundation::Result<InfantryModelCompileResult, foundation::Error>
    compile(const InfantryModelRequest& request);
    [[nodiscard]] std::uint64_t cacheHits() const noexcept {
        return cache_hits_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t cacheMisses() const noexcept {
        return cache_misses_.load(std::memory_order_relaxed);
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<proc::ArtifactKey,
                       std::shared_ptr<const InfantryModelArtifact>,
                       proc::ArtifactKeyHash> cache_;
    std::atomic_uint64_t cache_hits_{0};
    std::atomic_uint64_t cache_misses_{0};
};

} // namespace genomes::infantry
