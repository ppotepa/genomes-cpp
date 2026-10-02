#include <genomes/infantry/GroundContact.hpp>
#include <genomes/infantry/TwoBoneIK.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(value.x * value.x + value.y * value.y +
                                    value.z * value.z);
    if (!(length > 1.0e-6F) || !std::isfinite(length)) return {0.0F, 1.0F, 0.0F};
    return {value.x / length, value.y / length, value.z / length};
}

} // namespace

bool GroundContactOutput::valid() const noexcept {
    if (!has_surface || !std::isfinite(body_lift) || !std::isfinite(max_ik_error) ||
        !std::isfinite(clearance) || body_lift < 0.0F || max_ik_error < 0.0F ||
        clearance < 0.0F) {
        return false;
    }
    for (const GroundSupportPoint& foot : feet) {
        if (!foot.valid || !finite(foot.bind_position) || !finite(foot.target_position) ||
            !finite(foot.normal) || foot.normal.y < 0.0F) {
            return false;
        }
    }
    return true;
}

foundation::Result<GroundContactOutput, foundation::Error> GroundContactSolver::solve(
    const GroundContactInput& input, const GroundSurfaceQuery& surface) {
    if (!surface.valid() || !finite(input.hips) || !finite(input.left_foot) ||
        !finite(input.right_foot) || input.upper_leg_length <= 0.0F ||
        input.lower_leg_length <= 0.0F || !std::isfinite(input.sole_offset) ||
        input.sole_offset < 0.0F || !std::isfinite(input.max_body_lift) ||
        input.max_body_lift < 0.0F || !std::isfinite(input.max_replant_distance) ||
        input.max_replant_distance <= 0.0F ||
        input.gear_support_count > input.gear_supports.size()) {
        return foundation::Result<GroundContactOutput, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ground contact input"});
    }
    for (std::size_t index = 0U; index < input.gear_support_count; ++index) {
        if (!finite(input.gear_supports[index])) {
            return foundation::Result<GroundContactOutput, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid gear support point"});
        }
    }
    GroundContactOutput output{};
    output.surface_revision = surface.revision;
    const foundation::Vec3 bind_feet[2]{input.left_foot, input.right_foot};
    for (std::size_t index = 0U; index < 2U; ++index) {
        GroundSample sample{};
        if (!surface.sample(surface.context, bind_feet[index], sample) ||
            !std::isfinite(sample.height) || !finite(sample.normal) || sample.normal.y <= 0.0F) {
            return foundation::Result<GroundContactOutput, foundation::Error>::failure(
                {foundation::ErrorCode::NotFound, "ground surface sample unavailable"});
        }
        output.feet[index].bind_position = bind_feet[index];
        output.feet[index].target_position = {bind_feet[index].x,
                                               sample.height + input.sole_offset,
                                               bind_feet[index].z};
        output.feet[index].normal = normalized(sample.normal);
        output.feet[index].valid = true;
    }
    float highest_support = input.hips.y;
    for (const GroundSupportPoint& foot : output.feet) {
        highest_support = std::max(highest_support, foot.target_position.y);
    }
    for (std::size_t index = 0U; index < input.gear_support_count; ++index) {
        GroundSample sample{};
        if (!surface.sample(surface.context, input.gear_supports[index], sample) ||
            !std::isfinite(sample.height) || !finite(sample.normal)) {
            return foundation::Result<GroundContactOutput, foundation::Error>::failure(
                {foundation::ErrorCode::NotFound, "gear support surface sample unavailable"});
        }
        highest_support = std::max(highest_support, sample.height + input.sole_offset);
    }
    output.body_lift = std::max(0.0F, highest_support - input.hips.y);
    output.clearance = input.max_body_lift - output.body_lift;
    if (output.body_lift > input.max_body_lift) {
        return foundation::Result<GroundContactOutput, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "ground contact exceeds body lift bound"});
    }
    const foundation::Vec3 lifted_hips{input.hips.x, input.hips.y + output.body_lift, input.hips.z};
    const foundation::Vec3 poles[2]{input.left_pole, input.right_pole};
    for (std::size_t index = 0U; index < 2U; ++index) {
        const auto solution = TwoBoneIK::solve(lifted_hips, output.feet[index].target_position,
                                               poles[index], input.upper_leg_length,
                                               input.lower_leg_length);
        if (!solution) {
            return foundation::Result<GroundContactOutput, foundation::Error>::failure(
                solution.error());
        }
        output.max_ik_error = std::max(output.max_ik_error, solution.value().residual_error);
    }
    output.has_surface = true;
    return output.valid()
               ? foundation::Result<GroundContactOutput, foundation::Error>::success(output)
               : foundation::Result<GroundContactOutput, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "ground contact result is invalid"});
}

const GroundContactOutput* GroundContactCache::find(foundation::StableId key) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(key);
    return iterator == entries_.end() ? nullptr : &iterator->second;
}

void GroundContactCache::store(foundation::StableId key, const GroundContactOutput& value) {
    if (key == 0 || !value.valid()) {
        return;
    }
    std::scoped_lock lock(mutex_);
    entries_[key] = value;
}

void GroundContactCache::clear() {
    std::scoped_lock lock(mutex_);
    entries_.clear();
}

std::size_t GroundContactCache::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

namespace {

foundation::Vec3 resolvePersistentContact(
    std::array<PersistentGroundContact, 2U>& contacts, std::size_t index,
    foundation::Vec3 candidate, float strength, bool enabled, float max_drift,
    float phase, bool stance) noexcept {
    if (index >= contacts.size()) return candidate;
    auto& contact = contacts[index];
    if (!std::isfinite(strength) || !std::isfinite(max_drift) ||
        !std::isfinite(phase)) {
        contact.release();
        return candidate;
    }
    float normalized_phase = std::fmod(phase, 1.0F);
    if (normalized_phase < 0.0F) normalized_phase += 1.0F;
    const bool phase_wrap = contact.phase_valid &&
                            normalized_phase + 0.5F < contact.phase;
    contact.phase = normalized_phase;
    contact.phase_valid = true;

    const float weight = std::clamp(strength, 0.0F, 1.0F);
    constexpr float acquire_weight = 0.35F;
    constexpr float release_weight = 0.20F;
    if (!enabled || !stance || weight < release_weight || !finite(candidate) ||
        (!contact.active && weight < acquire_weight)) {
        // Keep the phase history while the authored foot is in swing. This
        // makes the next planted sample a replant event instead of reviving a
        // stale world-space lock.
        contact.releaseConstraint();
        return candidate;
    }

    const bool replant = !contact.active || !contact.stance || phase_wrap;
    if (replant) {
        contact.point = candidate;
        contact.active = true;
        contact.locked = false;
        contact.weight = 0.0F;
        if (phase_wrap && contact.stance) ++contact.reanchors;
    }
    contact.stance = true;

    const float dx = candidate.x - contact.point.x;
    const float dz = candidate.z - contact.point.z;
    if (!replant && std::sqrt(dx * dx + dz * dz) > std::max(0.001F, max_drift)) {
        // Reanchor at the current authored target. The zeroed blend weight
        // lets the next planted samples ramp the lock back in smoothly.
        contact.point = candidate;
        contact.locked = false;
        contact.weight = 0.0F;
        ++contact.reanchors;
    }

    const float normalized_strength =
        std::clamp((weight - release_weight) / (1.0F - release_weight), 0.0F, 1.0F);
    const float target_weight = normalized_strength * normalized_strength *
                                (3.0F - 2.0F * normalized_strength);
    contact.weight += (target_weight - contact.weight) * 0.35F;
    if (contact.locked && (weight < 0.65F || contact.weight < 0.65F)) {
        contact.locked = false;
    } else if (!contact.locked && contact.weight >= 0.92F && weight >= 0.65F) {
        contact.locked = true;
    }
    return {candidate.x + (contact.point.x - candidate.x) * contact.weight,
            candidate.y + (contact.point.y - candidate.y) * contact.weight,
            candidate.z + (contact.point.z - candidate.z) * contact.weight};
}

} // namespace

foundation::Vec3 GroundContactRuntime::resolve(std::size_t index,
                                                foundation::Vec3 candidate,
                                                float strength, bool enabled,
                                                float max_drift, float phase,
                                                bool stance) noexcept {
    return resolvePersistentContact(feet, index, candidate, strength, enabled,
                                    max_drift, phase, stance);
}

foundation::Vec3 GroundContactRuntime::resolveHand(std::size_t index,
                                                    foundation::Vec3 candidate,
                                                    float strength, bool enabled,
                                                    float max_drift, float phase,
                                                    bool stance) noexcept {
    return resolvePersistentContact(hands, index, candidate, strength, enabled,
                                    max_drift, phase, stance);
}

} // namespace genomes::infantry
