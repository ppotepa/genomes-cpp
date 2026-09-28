#include <genomes/ballistics/Detonation.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace genomes::ballistics {

namespace {

constexpr float kEpsilon = 1.0e-6F;

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left,
                                   foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 left,
                                     foundation::Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float length(foundation::Vec3 value) noexcept {
    return std::sqrt(std::max(0.0F, dot(value, value)));
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    return multiply(value, 1.0F / std::max(kEpsilon, length(value)));
}

[[nodiscard]] float unitFloat(std::uint64_t hash, unsigned shift) noexcept {
    return static_cast<float>((hash >> shift) & 0xFFFFU) / 65535.0F;
}

[[nodiscard]] foundation::StableId nonzero(foundation::StableId value,
                                            foundation::StableId fallback) noexcept {
    return value == 0 ? (fallback == 0 ? 1 : fallback) : value;
}

[[nodiscard]] FragmentationProfile effectiveProfile(const AmmunitionStrategy& strategy) noexcept {
    if (strategy.fragmentation.enabled()) {
        return strategy.fragmentation;
    }
    FragmentationProfile profile{};
    if (strategy.explosive && strategy.max_fragments != 0U &&
        strategy.fragment_mass_fraction > 0.0F) {
        profile.requested_count = std::min(strategy.max_fragments, 2048U);
        profile.body_mass_fraction = strategy.fragment_mass_fraction;
        profile.explosive_energy_fraction = strategy.fragment_mass_fraction;
        profile.mass_spread = 0.0F;
    }
    return profile;
}

} // namespace

bool BlastCommand::valid() const noexcept {
    return command_id != 0 && source_projectile_id != 0 && finite(origin) && finite(radius_m) &&
           radius_m > 0.0F && finite(energy_j) && energy_j >= 0.0F && seed != 0;
}

bool DetonationEvent::valid() const noexcept {
    if (version != DetonationEventVersion || !projectile_id.isValid() || !shot_id.isValid() ||
        !parent_trace_id.isValid() || !ammunition_id.isValid() || !finite(origin) ||
        !finite(normal) || length(normal) <= kEpsilon || seed == 0 || !blast.valid() ||
        !ledger.valid()) {
        return false;
    }
    return std::all_of(fragments.begin(), fragments.end(),
                       [](const FragmentSpawn& fragment) { return fragment.valid(); });
}

foundation::Result<DetonationEvent, foundation::Error> Detonation::detonate(
    const ProjectileState& projectile,
    const AmmunitionDefinition& ammunition,
    const AmmunitionStrategy& strategy,
    const DetonationInput& input) {
    if (!projectile.valid() || !ammunition.valid() || !strategy.valid() ||
        strategy.fuze != FuzeMode::ArmedContact || !strategy.explosive ||
        !input.refined_surface || input.contact_id == 0 || !finite(input.point) ||
        !finite(input.normal) || length(input.normal) <= kEpsilon) {
        return foundation::Result<DetonationEvent, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "detonation requires an armed refined contact surface"});
    }

    const FragmentationProfile profile = effectiveProfile(strategy);
    if (!profile.valid()) {
        return foundation::Result<DetonationEvent, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid fragmentation profile"});
    }

    const std::uint64_t root_seed = nonzero(
        foundation::stableHashCombine(
            foundation::stableHashCombine(projectile.seed, projectile.projectile_id.value()),
            input.contact_id),
        projectile.projectile_id.value());
    const float explosive_energy = std::max(0.0F, ammunition.explosive_energy_j);
    const float requested_fragment_energy = explosive_energy * profile.explosive_energy_fraction;
    const float blast_energy = explosive_energy - requested_fragment_energy;
    const float requested_mass = projectile.mass_kg * profile.body_mass_fraction;
    const float nose_crush = std::min(projectile.translationalEnergy(), strategy.nose_crush_work_j);

    struct PairSpec final {
        std::uint32_t index{0};
        std::uint32_t count{0};
        float weight{1.0F};
        foundation::Vec3 direction{};
    };
    std::vector<PairSpec> pairs;
    pairs.reserve((profile.requested_count + 1U) / 2U);
    float total_weight = 0.0F;
    const foundation::Vec3 forward = normalized(projectile.body_forward);
    const foundation::Vec3 reference = std::abs(forward.y) < 0.9F
                                            ? foundation::Vec3{0.0F, 1.0F, 0.0F}
                                            : foundation::Vec3{1.0F, 0.0F, 0.0F};
    const foundation::Vec3 right = normalized(cross(reference, forward));
    const foundation::Vec3 up = cross(forward, right);
    for (std::uint32_t pair_index = 0; pair_index < (profile.requested_count + 1U) / 2U;
         ++pair_index) {
        const std::uint64_t hash = foundation::stableHashCombine(root_seed, pair_index + 1U);
        const float z = unitFloat(hash, 0U) * 2.0F - 1.0F;
        const float radial = std::sqrt(std::max(0.0F, 1.0F - z * z));
        const float phi = unitFloat(hash, 16U) * (2.0F * std::numbers::pi_v<float>);
        const foundation::Vec3 local{radial * std::cos(phi), radial * std::sin(phi), z};
        const foundation::Vec3 direction = normalized(add(
            add(multiply(right, local.x), multiply(up, local.y)), multiply(forward, local.z)));
        const std::uint32_t count = pair_index * 2U + 2U <= profile.requested_count ? 2U : 1U;
        const float weight = std::max(
            0.001F, 1.0F + profile.mass_spread * (unitFloat(hash, 32U) * 2.0F - 1.0F));
        pairs.push_back({pair_index, count, weight, direction});
        total_weight += weight * static_cast<float>(count);
    }

    DetonationEvent event{};
    event.projectile_id = projectile.projectile_id;
    event.shot_id = projectile.shot_id;
    event.parent_trace_id = projectile.trace_id;
    event.ammunition_id = projectile.ammunition_id;
    event.origin = input.point;
    event.normal = normalized(input.normal);
    event.seed = root_seed;
    event.ledger.parent_mass = projectile.mass_kg;
    event.ledger.requested_mass = requested_mass;
    event.ledger.explosive_energy = explosive_energy;
    event.ledger.blast_energy = blast_energy;
    event.ledger.requested_fragment_energy = requested_fragment_energy;
    event.ledger.nose_crush_work = nose_crush;
    event.blast = {nonzero(foundation::stableHashCombine(root_seed, 0xB1A57U), root_seed),
                   projectile.projectile_id.value(),
                   event.origin,
                   std::max(0.1F, std::cbrt(std::max(0.0F, blast_energy)) * 0.5F),
                   blast_energy,
                   root_seed};

    std::uint32_t remaining_capacity = input.available_fragment_capacity;
    if (total_weight > 0.0F && remaining_capacity > 0U && requested_mass > kEpsilon &&
        requested_fragment_energy > kEpsilon) {
        event.fragments.reserve(std::min(profile.requested_count, remaining_capacity));
        for (const PairSpec& pair : pairs) {
            if (remaining_capacity < pair.count) {
                break; // capacity is consumed by complete opposite pairs
            }
            remaining_capacity -= pair.count;
            const float pair_mass = requested_mass * pair.weight / total_weight;
            const float pair_energy = requested_fragment_energy * pair.weight / total_weight;
            for (std::uint32_t member = 0; member < pair.count; ++member) {
                const foundation::StableId index = pair.index * 2U + member + 1U;
                const foundation::StableId seed = nonzero(
                    foundation::stableHashCombine(root_seed, index), root_seed + index);
                const foundation::Vec3 radial_direction = member == 0U
                                                              ? pair.direction
                                                              : multiply(pair.direction, -1.0F);
                const float fragment_mass = pair_mass;
                const float fragment_energy = pair_energy;
                const float speed = fragment_mass > kEpsilon
                                        ? std::sqrt(2.0F * fragment_energy / fragment_mass) *
                                              profile.radial_velocity_scale
                                        : 0.0F;
                const float diameter = std::max(
                    kEpsilon,
                    projectile.diameter_m * profile.diameter_scale *
                        std::cbrt(std::max(kEpsilon, fragment_mass / projectile.mass_kg)));
                event.fragments.push_back(
                    {nonzero(foundation::stableHashCombine(root_seed, 0xF000U + index), seed),
                     nonzero(foundation::stableHashCombine(root_seed, 0xD000U + index), seed),
                     projectile.trace_id.value(),
                     seed,
                     input.point,
                     add(projectile.velocity, multiply(radial_direction, speed)),
                     fragment_mass,
                     diameter,
                     fragment_energy,
                     pair.index,
                     pair.count == 2U});
            }
        }
    }

    for (const FragmentSpawn& fragment : event.fragments) {
        event.ledger.represented_mass += fragment.mass_kg;
        event.ledger.represented_fragment_energy += fragment.represented_energy;
    }
    event.ledger.omitted_mass = std::max(0.0F, event.ledger.requested_mass -
                                                   event.ledger.represented_mass);
    event.ledger.unrepresented_energy = std::max(
        0.0F, event.ledger.requested_fragment_energy - event.ledger.represented_fragment_energy);
    return foundation::Result<DetonationEvent, foundation::Error>::success(std::move(event));
}

FireRequest Detonation::makeFireRequest(const DetonationEvent& event,
                                        const FragmentSpawn& fragment,
                                        std::uint64_t tick) noexcept {
    FireRequest request{};
    request.projectile_id = ProjectileId{fragment.projectile_id};
    request.shot_id = event.shot_id;
    request.trace_id = TraceId{fragment.trace_id};
    request.parent_trace_id = TraceId{fragment.parent_trace_id};
    request.ammunition_id = event.ammunition_id;
    request.position = fragment.position;
    request.direction = normalized(fragment.velocity);
    request.tick = tick;
    request.seed = fragment.seed;
    request.fragment = true;
    request.velocity_override = fragment.velocity;
    request.mass_override_kg = fragment.mass_kg;
    request.diameter_override_m = fragment.diameter_m;
    request.drag_diameter_override_m = fragment.diameter_m;
    return request;
}

} // namespace genomes::ballistics
