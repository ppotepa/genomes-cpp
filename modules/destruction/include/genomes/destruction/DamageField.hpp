#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::destruction {

inline constexpr std::uint32_t DamageFieldVersion = 1;

struct DamageFieldSpec final {
    proc::Seed seed{1};
    foundation::Vec3 minimum{};
    foundation::Vec3 maximum{1.0F, 1.0F, 1.0F};
    foundation::Vec3 cell_size{0.25F, 0.25F, 0.25F};
    std::uint32_t cells_x{4};
    std::uint32_t cells_y{4};
    std::uint32_t cells_z{4};
    std::uint32_t max_holes{128};

    [[nodiscard]] bool valid() const noexcept;
};

struct DamageCell final {
    foundation::Vec3 center{};
    float crush{0.0F};
    float crack{0.0F};
    float rear{0.0F};
    float weakness{0.0F};
};

struct HoleRecord final {
    foundation::StableId id{0};
    foundation::Vec3 center{};
    foundation::Vec3 direction{0.0F, 1.0F, 0.0F};
    float radius{0.0F};
    float depth{0.0F};
    float removed_volume{0.0F};
};

struct DamageCoefficients final {
    float crush{1.0F};
    float crack{1.0F};
    float rear{1.0F};
    float weakness{1.0F};
};

struct ImpactDamageCommand final {
    foundation::StableId event_id{0};
    foundation::Vec3 local_point{};
    foundation::Vec3 direction{0.0F, 1.0F, 0.0F};
    float radius{0.0F};
    float depth{0.0F};
    float energy{0.0F};
    DamageCoefficients coefficients{};
    bool rear_surface{false};
    bool through_channel{false};
};

struct DamageApplyResult final {
    std::uint32_t affected_cells{0};
    bool clipped{false};
    bool hole_added{false};
    float aggregate_damage_delta{0.0F};
    float removed_volume_delta{0.0F};
};

class DamageField final {
public:
    [[nodiscard]] static foundation::Result<DamageField, foundation::Error> create(
        DamageFieldSpec spec);

    [[nodiscard]] DamageApplyResult apply(const ImpactDamageCommand&) noexcept;

    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const DamageFieldSpec& spec() const noexcept { return spec_; }
    [[nodiscard]] const std::vector<DamageCell>& cells() const noexcept { return cells_; }
    [[nodiscard]] const std::vector<HoleRecord>& holes() const noexcept { return holes_; }
    [[nodiscard]] float aggregateDamage() const noexcept { return aggregate_damage_; }
    [[nodiscard]] float removedVolume() const noexcept { return removed_volume_; }

    [[nodiscard]] const DamageCell* cell(std::uint32_t x,
                                         std::uint32_t y,
                                         std::uint32_t z) const noexcept;

private:
    [[nodiscard]] std::size_t index(std::uint32_t x,
                                    std::uint32_t y,
                                    std::uint32_t z) const noexcept;

    std::uint32_t version_{DamageFieldVersion};
    DamageFieldSpec spec_{};
    std::vector<DamageCell> cells_;
    std::vector<HoleRecord> holes_;
    float aggregate_damage_{0.0F};
    float removed_volume_{0.0F};
};

} // namespace genomes::destruction
