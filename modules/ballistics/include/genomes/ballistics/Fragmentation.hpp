#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::ballistics {

// The fuze model is intentionally small and gameplay-oriented.  Unsupported
// delayed/programmed modes must not silently fall back to contact behavior.
enum class FuzeMode : std::uint8_t {
    None,
    ArmedContact,
};

struct FragmentationProfile final {
    std::uint32_t requested_count{0};
    float body_mass_fraction{0.0F};
    float explosive_energy_fraction{0.0F};
    float mass_spread{0.0F};
    float radial_velocity_scale{1.0F};
    float diameter_scale{0.35F};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool enabled() const noexcept {
        return requested_count != 0U && body_mass_fraction > 0.0F &&
               explosive_energy_fraction > 0.0F;
    }
};

struct FragmentSpawn final {
    foundation::StableId projectile_id{0};
    foundation::StableId trace_id{0};
    foundation::StableId parent_trace_id{0};
    foundation::StableId seed{0};
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    float mass_kg{0.0F};
    float diameter_m{0.0F};
    float represented_energy{0.0F};
    std::uint32_t pair_index{0};
    bool pair_member{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct FragmentationLedger final {
    float parent_mass{0.0F};
    float requested_mass{0.0F};
    float represented_mass{0.0F};
    float omitted_mass{0.0F};
    float explosive_energy{0.0F};
    float blast_energy{0.0F};
    float requested_fragment_energy{0.0F};
    float represented_fragment_energy{0.0F};
    float unrepresented_energy{0.0F};
    float nose_crush_work{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

} // namespace genomes::ballistics
