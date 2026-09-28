#pragma once

#include <genomes/destruction/DebrisRecord.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::destruction {

inline constexpr std::uint32_t DebrisLifecycleVersion = 1;

struct DebrisPolicy final {
    std::uint32_t max_debris{512};
    std::uint32_t max_active_debris{128};
    std::uint32_t max_cheap_debris{1024};
    std::uint64_t settle_grace_ticks{60};

    [[nodiscard]] bool valid() const noexcept {
        return settle_grace_ticks > 0 && max_debris <= 1'000'000U &&
               max_active_debris <= 1'000'000U && max_cheap_debris <= 1'000'000U;
    }
};

struct RepresentationAccounting final {
    double original_source_volume{0.0};
    double attached_volume{0.0};
    double hero_volume{0.0};
    double cheap_volume{0.0};
    double baked_volume{0.0};
    double removed_by_damage_volume{0.0};
    std::vector<MaterialVolume> material_totals;

    [[nodiscard]] double representedVolume() const noexcept {
        return attached_volume + hero_volume + cheap_volume + baked_volume +
               removed_by_damage_volume;
    }
};

class DebrisLifecycle final {
public:
    [[nodiscard]] static foundation::Result<DebrisLifecycle, foundation::Error> create(
        DebrisPolicy policy = {});

    [[nodiscard]] foundation::Result<foundation::StableId, foundation::Error> spawn(
        DebrisRecord record);
    [[nodiscard]] bool wake(foundation::StableId id) noexcept;
    [[nodiscard]] bool sleep(foundation::StableId id) noexcept;
    void advance(std::uint64_t ticks) noexcept;

    // The returned copies are the proposed deposits. The caller must commit
    // only after the rubble field accepts all of them.
    [[nodiscard]] std::vector<DebrisRecord> prepareSettlement() const;
    [[nodiscard]] bool commitSettlement(std::span<const foundation::StableId> ids) noexcept;

    [[nodiscard]] const DebrisPolicy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const std::vector<DebrisRecord>& records() const noexcept { return records_; }
    [[nodiscard]] RepresentationAccounting accounting() const;
    [[nodiscard]] std::size_t count(DebrisRepresentation representation) const noexcept;

private:
    void enforceBudgets() noexcept;
    [[nodiscard]] DebrisRecord* find(foundation::StableId id) noexcept;
    [[nodiscard]] const DebrisRecord* find(foundation::StableId id) const noexcept;

    std::uint32_t version_{DebrisLifecycleVersion};
    DebrisPolicy policy_{};
    std::vector<DebrisRecord> records_;
};

} // namespace genomes::destruction
