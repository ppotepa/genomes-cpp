#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <cstdint>
#include <atomic>
#include <mutex>
#include <optional>
#include <memory>
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
    // migrated. It remains part of the key and drives the current materials.
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
    foundation::StableId cache_key{0};
};

class InfantryModelCompiler final {
public:
    using CompileRevision = std::uint64_t;

    // A revision identifies the most recent Unit Lab request.  Starting a new
    // revision makes older work stale; callers may cancel it explicitly or by
    // simply starting another revision before publishing the result.
    [[nodiscard]] CompileRevision beginRevision() noexcept {
        return revision_.fetch_add(1U, std::memory_order_acq_rel) + 1U;
    }
    void cancelRevision(CompileRevision revision) noexcept {
        auto expected = revision;
        (void)revision_.compare_exchange_strong(expected, revision + 1U,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
    }

    [[nodiscard]] static foundation::StableId canonicalRequestKey(
        const InfantryModelRequest&) noexcept;

    [[nodiscard]] foundation::Result<InfantryModelArtifact, foundation::Error>
    compile(const InfantryModelRequest& request);

    [[nodiscard]] foundation::Result<InfantryModelArtifact, foundation::Error>
    compile(const InfantryModelRequest& request, CompileRevision revision);

    [[nodiscard]] std::optional<foundation::Error> lastError() const;
    [[nodiscard]] std::uint64_t cacheHits() const noexcept {
        return cache_hits_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t cacheMisses() const noexcept {
        return cache_misses_.load(std::memory_order_relaxed);
    }

private:
    mutable std::mutex mutex_;
    std::optional<foundation::Error> last_error_;
    std::unordered_map<foundation::StableId,
                       std::shared_ptr<const InfantryModelArtifact>> cache_;
    std::atomic_uint64_t cache_hits_{0};
    std::atomic_uint64_t cache_misses_{0};
    std::atomic<CompileRevision> revision_{0};
};

} // namespace genomes::infantry
