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
#include <mutex>
#include <optional>
#include <memory>
#include <unordered_map>

namespace genomes::infantry {

struct InfantryModelRequest final {
    proc::Seed seed{0};
    float variation{1.0F};
    std::uint32_t detail_level{2};
    foundation::StableId loadout_id{0};
    EquipmentOverrideSet equipment_overrides{};
    foundation::Color uniform_color{};
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
    [[nodiscard]] foundation::Result<InfantryModelArtifact, foundation::Error>
    compile(const InfantryModelRequest& request);

    [[nodiscard]] std::optional<InfantryModelArtifact> lastSuccessful() const;
    [[nodiscard]] std::uint64_t cacheHits() const noexcept { return cache_hits_; }
    [[nodiscard]] std::uint64_t cacheMisses() const noexcept { return cache_misses_; }

private:
    mutable std::mutex mutex_;
    std::optional<InfantryModelArtifact> last_successful_;
    std::unordered_map<foundation::StableId,
                       std::shared_ptr<const InfantryModelArtifact>> cache_;
    std::uint64_t cache_hits_{0};
    std::uint64_t cache_misses_{0};
};

} // namespace genomes::infantry
