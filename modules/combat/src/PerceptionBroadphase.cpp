#include <genomes/combat/PerceptionBroadphase.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::combat {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] std::uint64_t packed(simulation::EntityId id) noexcept { return id.packed(); }

} // namespace

bool PerceptionProfile::valid() const noexcept {
    return finite(range_m) && range_m > 0.0F && finite(horizontal_half_fov_radians) &&
           horizontal_half_fov_radians > 0.0F && horizontal_half_fov_radians <= 3.1415927F &&
           max_candidates > 0U;
}

bool PerceptionAgent::valid() const noexcept {
    return id.isValid() && finite(position) && finite(heading_radians);
}

foundation::Result<void, foundation::Error> PerceptionBroadphase::rebuild(
    std::span<const PerceptionAgent> agents, spatial::UniformGrid& grid) {
    agents_.assign(agents.begin(), agents.end());
    std::sort(agents_.begin(), agents_.end(), [](const PerceptionAgent& left,
                                                 const PerceptionAgent& right) {
        return packed(left.id) < packed(right.id);
    });
    for (std::size_t index = 0U; index < agents_.size(); ++index) {
        if (!agents_[index].valid() || (index > 0U && agents_[index - 1U].id == agents_[index].id)) {
            agents_.clear();
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid or duplicate perception agent"});
        }
    }
    grid.clear();
    grid.reserve(agents_.size());
    for (const PerceptionAgent& agent : agents_) {
        grid.insert(agent.id, agent.position);
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<std::vector<PerceptionCandidate>, foundation::Error> PerceptionBroadphase::query(
    const PerceptionAgent& observer, const PerceptionProfile& profile,
    const spatial::UniformGrid& grid) const {
    diagnostics_ = {};
    if (!observer.valid() || !profile.valid()) {
        return foundation::Result<std::vector<PerceptionCandidate>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid perception query"});
    }
    const auto observer_found = std::lower_bound(
        agents_.begin(), agents_.end(), packed(observer.id),
        [](const PerceptionAgent& value, std::uint64_t key) { return packed(value.id) < key; });
    if (observer_found == agents_.end() || observer_found->id != observer.id) {
        return foundation::Result<std::vector<PerceptionCandidate>, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "observer is absent from perception snapshot"});
    }
    std::vector<simulation::EntityId> ids;
    grid.queryRadius(observer.position, profile.range_m, ids);
    diagnostics_.grid_candidates = static_cast<std::uint32_t>(ids.size());
    const float range_squared = profile.range_m * profile.range_m;
    const float cos_half_fov = std::cos(profile.horizontal_half_fov_radians);
    const foundation::Vec3 forward{std::sin(observer.heading_radians), 0.0F,
                                   std::cos(observer.heading_radians)};
    std::vector<PerceptionCandidate> candidates;
    candidates.reserve(ids.size());
    for (const simulation::EntityId id : ids) {
        if (id == observer.id) {
            continue;
        }
        const auto found = std::lower_bound(
            agents_.begin(), agents_.end(), packed(id),
            [](const PerceptionAgent& value, std::uint64_t key) { return packed(value.id) < key; });
        if (found == agents_.end() || found->id != id || !found->alive) {
            continue;
        }
        if (found->team == observer.team || (profile.enemy_team_mask & (1U << (found->team % 32U))) == 0U) {
            ++diagnostics_.rejected_team;
            continue;
        }
        const foundation::Vec3 relative{found->position.x - observer.position.x,
                                        found->position.y - observer.position.y,
                                        found->position.z - observer.position.z};
        const float distance_squared = relative.x * relative.x + relative.y * relative.y +
                                       relative.z * relative.z;
        if (distance_squared > range_squared) {
            ++diagnostics_.rejected_range;
            continue;
        }
        const float horizontal_squared = relative.x * relative.x + relative.z * relative.z;
        if (horizontal_squared <= 1.0e-8F) {
            ++diagnostics_.rejected_fov;
            continue;
        }
        const float inverse_horizontal = 1.0F / std::sqrt(horizontal_squared);
        const float facing = (forward.x * relative.x + forward.z * relative.z) * inverse_horizontal;
        if (facing < cos_half_fov) {
            ++diagnostics_.rejected_fov;
            continue;
        }
        candidates.push_back({id, relative, distance_squared});
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const PerceptionCandidate& left,
                                                               const PerceptionCandidate& right) {
        if (left.distance_squared != right.distance_squared) {
            return left.distance_squared < right.distance_squared;
        }
        return packed(left.target) < packed(right.target);
    });
    if (candidates.size() > profile.max_candidates) {
        candidates.resize(profile.max_candidates);
        diagnostics_.truncated = true;
    }
    diagnostics_.accepted = static_cast<std::uint32_t>(candidates.size());
    return foundation::Result<std::vector<PerceptionCandidate>, foundation::Error>::success(
        std::move(candidates));
}

} // namespace genomes::combat
