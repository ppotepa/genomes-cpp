#include <genomes/infantry/GroundContact.hpp>
#include <genomes/infantry/TwoBoneIK.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

bool GroundContactOutput::valid() const noexcept {
    if (!has_surface || !std::isfinite(body_lift) || !std::isfinite(max_ik_error) ||
        body_lift < 0.0F || max_ik_error < 0.0F) {
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
        input.sole_offset < 0.0F || input.gear_support_count > input.gear_supports.size()) {
        return foundation::Result<GroundContactOutput, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ground contact input"});
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
        output.feet[index].normal = sample.normal;
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

} // namespace genomes::infantry
