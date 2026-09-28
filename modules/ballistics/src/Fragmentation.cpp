#include <genomes/ballistics/Fragmentation.hpp>

#include <cmath>

namespace genomes::ballistics {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

} // namespace

bool FragmentationProfile::valid() const noexcept {
    return requested_count <= 2048U && finite(body_mass_fraction) &&
           finite(explosive_energy_fraction) && finite(mass_spread) &&
           finite(radial_velocity_scale) && finite(diameter_scale) &&
           body_mass_fraction >= 0.0F && body_mass_fraction <= 1.0F &&
           explosive_energy_fraction >= 0.0F && explosive_energy_fraction <= 1.0F &&
           mass_spread >= 0.0F && mass_spread <= 1.0F && radial_velocity_scale >= 0.0F &&
           diameter_scale > 0.0F;
}

bool FragmentSpawn::valid() const noexcept {
    return projectile_id != 0 && trace_id != 0 && parent_trace_id != 0 && seed != 0 &&
           finite(position) && finite(velocity) && finite(mass_kg) && mass_kg > 0.0F &&
           finite(diameter_m) && diameter_m > 0.0F && finite(represented_energy) &&
           represented_energy >= 0.0F;
}

bool FragmentationLedger::valid() const noexcept {
    return finite(parent_mass) && finite(requested_mass) && finite(represented_mass) &&
           finite(omitted_mass) && finite(explosive_energy) && finite(blast_energy) &&
           finite(requested_fragment_energy) && finite(represented_fragment_energy) &&
           finite(unrepresented_energy) && finite(nose_crush_work) && parent_mass >= 0.0F &&
           requested_mass >= 0.0F && represented_mass >= 0.0F && omitted_mass >= 0.0F &&
           explosive_energy >= 0.0F && blast_energy >= 0.0F &&
           requested_fragment_energy >= 0.0F && represented_fragment_energy >= 0.0F &&
           unrepresented_energy >= 0.0F && nose_crush_work >= 0.0F;
}

} // namespace genomes::ballistics
