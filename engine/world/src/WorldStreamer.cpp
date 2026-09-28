#include <genomes/world/WorldStreamer.hpp>

#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace genomes::world {

namespace {

[[nodiscard]] bool has(ResidencyMask mask, ResidencyAxis axis) noexcept {
    return (mask & residency(axis)) != 0U;
}

[[nodiscard]] std::size_t estimateBytes(const WorldPlan& plan) noexcept {
    return sizeof(WorldPlan) + plan.features.size() * sizeof(WorldFeature) +
           plan.building_sites.size() * sizeof(BuildingSiteRequest);
}

} // namespace

WorldStreamer::WorldStreamer(WorldId world_id, WorldGenerationRequest request,
                             WorldCoordinateConfig coordinates, jobs::JobSystem& jobs,
                             WorldStreamerConfig config)
    : world_id_(world_id), request_(request), coordinates_(coordinates), config_(config),
      cache_(std::make_shared<proc::ArtifactCache>()), generation_service_(jobs, cache_) {}

bool WorldStreamer::validMask(ResidencyMask mask) noexcept {
    constexpr ResidencyMask all = residency(ResidencyAxis::Semantic) |
                                  residency(ResidencyAxis::Simulation) |
                                  residency(ResidencyAxis::Render) |
                                  residency(ResidencyAxis::Physics) |
                                  residency(ResidencyAxis::Navigation);
    return (mask & ~all) == 0U;
}

foundation::Result<void, foundation::Error> WorldStreamer::setDesired(
    const ResidencyRequest& request) {
    if (!world_id_.isValid() || !request_.valid() || !coordinates_.valid() || !config_.valid() ||
        !request.id.isValid() || request.id != regionId(world_id_, request.coordinate) ||
        !validMask(request.desired)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world residency request"});
    }
    ResidencyState& state = states_[request.id.value()];
    if (!state.valid()) {
        state.id = request.id;
        state.coordinate = request.coordinate;
    }
    state.desired = request.desired;
    state.pin_count = request.pin_count;
    state.last_access = request.tick;
    if (has(state.current, ResidencyAxis::Semantic)) {
        state.current |= request.desired & ~residency(ResidencyAxis::Semantic);
    }
    if (has(request.desired, ResidencyAxis::Semantic) &&
        !has(state.current, ResidencyAxis::Semantic) && !state.pending) {
        return schedule(state);
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WorldStreamer::schedule(ResidencyState& state) {
    if (pending_.size() >= config_.max_pending) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "world streamer pending budget exceeded"});
    }
    const std::uint32_t region_size = static_cast<std::uint32_t>(
        std::lround(coordinates_.region_size_m));
    if (region_size < 128U || region_size > 4096U) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "region size outside generator limits"});
    }
    proc::SeedPath path(request_.seed);
    path = path.child("region-x", static_cast<std::uint64_t>(state.coordinate.x));
    path = path.child("region-z", static_cast<std::uint64_t>(state.coordinate.z));
    path = path.child("region-layer", static_cast<std::uint64_t>(
                                              static_cast<std::int64_t>(state.coordinate.layer)));
    WorldGenerationRequest region_request = request_;
    region_request.seed = path.seed();
    region_request.map_size_m = region_size;
    state.generation++;
    state.pending = true;
    pending_.emplace(state.id.value(), Pending{state.coordinate, regionOrigin(state.coordinate,
                                                                                coordinates_),
                                               state.generation,
                                               generation_service_.submit(region_request)});
    return foundation::Result<void, foundation::Error>::success();
}

void WorldStreamer::translatePlan(WorldPlan& plan, WorldPosition origin) noexcept {
    const auto translate = [origin](foundation::Vec3& position) noexcept {
        position.x += static_cast<float>(origin.x);
        position.z += static_cast<float>(origin.z);
    };
    for (WorldFeature& feature : plan.features) {
        translate(feature.position);
    }
    for (BuildingSiteRequest& site : plan.building_sites) {
        site = site.translated({static_cast<float>(origin.x), 0.0F,
                                static_cast<float>(origin.z)});
    }
    plan.hydrology = plan.hydrology.translated(
        {static_cast<float>(origin.x), 0.0F, static_cast<float>(origin.z)});
    plan.city.road_graph = plan.city.road_graph.translated(
        {static_cast<float>(origin.x), 0.0F, static_cast<float>(origin.z)});
    for (CityParcel& parcel : plan.city.parcels) {
        translate(parcel.position);
    }
}

foundation::Result<std::uint32_t, foundation::Error> WorldStreamer::poll(
    foundation::SimulationTick tick) {
    std::uint32_t committed = 0U;
    for (auto iterator = pending_.begin(); iterator != pending_.end();) {
        Pending& pending = iterator->second;
        if (!pending.task.ready()) {
            ++iterator;
            continue;
        }
        ResidencyState* state = nullptr;
        const auto state_iterator = states_.find(iterator->first);
        if (state_iterator != states_.end()) {
            state = &state_iterator->second;
        }
        const bool wanted = state != nullptr && state->pending &&
                           state->generation == pending.generation &&
                           has(state->desired, ResidencyAxis::Semantic);
        if (wanted && !pending.task.failed()) {
            if (auto plan = pending.task.take_result()) {
                translatePlan(*plan, pending.origin);
                state->pending = false;
                state->current |= residency(ResidencyAxis::Semantic) |
                                  (state->desired & ~residency(ResidencyAxis::Semantic));
                state->content_hash = plan->content_hash;
                state->semantic_bytes = estimateBytes(*plan);
                state->render_bytes = plan->features.size() * sizeof(WorldFeature);
                state->physics_bytes = plan->building_sites.size() * sizeof(BuildingSiteRequest);
                state->navigation_bytes = plan->city.road_graph.edges().size() * sizeof(roads::RoadEdge);
                state->last_access = tick;
                ready_.push_back({state->id, state->coordinate, pending.origin, state->generation,
                                  std::move(*plan)});
                ++committed;
            }
        } else if (state != nullptr) {
            state->pending = false;
            (void)pending.task.take_result();
        }
        iterator = pending_.erase(iterator);
    }
    return foundation::Result<std::uint32_t, foundation::Error>::success(committed);
}

void WorldStreamer::clearAxis(ResidencyState& state, ResidencyAxis axis) noexcept {
    state.current &= static_cast<ResidencyMask>(~residency(axis));
    if (axis == ResidencyAxis::Semantic) {
        state.content_hash = 0U;
        state.semantic_bytes = 0U;
    } else if (axis == ResidencyAxis::Render) {
        state.render_bytes = 0U;
    } else if (axis == ResidencyAxis::Physics) {
        state.physics_bytes = 0U;
    } else if (axis == ResidencyAxis::Navigation) {
        state.navigation_bytes = 0U;
    }
}

foundation::Result<std::uint32_t, foundation::Error> WorldStreamer::evict() {
    std::uint32_t evicted = 0U;
    for (auto& [unused_id, state] : states_) {
        static_cast<void>(unused_id);
        if (state.pin_count != 0U) {
            continue;
        }
        if (has(state.current, ResidencyAxis::Render) && !has(state.desired, ResidencyAxis::Render)) {
            clearAxis(state, ResidencyAxis::Render);
            ++evicted;
        }
        if (has(state.current, ResidencyAxis::Physics) && !has(state.desired, ResidencyAxis::Physics)) {
            clearAxis(state, ResidencyAxis::Physics);
            ++evicted;
        }
        if (has(state.current, ResidencyAxis::Navigation) &&
            !has(state.desired, ResidencyAxis::Navigation)) {
            clearAxis(state, ResidencyAxis::Navigation);
            ++evicted;
        }
        if (has(state.current, ResidencyAxis::Simulation) &&
            !has(state.desired, ResidencyAxis::Simulation)) {
            clearAxis(state, ResidencyAxis::Simulation);
            ++evicted;
        }
        if (has(state.current, ResidencyAxis::Semantic) &&
            !has(state.desired, ResidencyAxis::Semantic)) {
            clearAxis(state, ResidencyAxis::Semantic);
            ++evicted;
        }
    }
    auto evict_axis = [this, &evicted](ResidencyAxis axis, std::size_t budget,
                                       auto bytes) {
        while (residentBytes(axis) > budget) {
            auto candidate = states_.end();
            for (auto iterator = states_.begin(); iterator != states_.end(); ++iterator) {
                const ResidencyState& state = iterator->second;
                if (state.pin_count == 0U && has(state.current, axis) &&
                    !has(state.desired, axis) &&
                    (candidate == states_.end() || state.last_access < candidate->second.last_access ||
                     (state.last_access == candidate->second.last_access &&
                      state.id.value() < candidate->second.id.value()))) {
                    candidate = iterator;
                }
            }
            if (candidate == states_.end()) {
                break;
            }
            clearAxis(candidate->second, axis);
            static_cast<void>(bytes);
            ++evicted;
        }
    };
    evict_axis(ResidencyAxis::Render, config_.render_budget_bytes,
               [](const ResidencyState& state) { return state.render_bytes; });
    evict_axis(ResidencyAxis::Physics, config_.physics_budget_bytes,
               [](const ResidencyState& state) { return state.physics_bytes; });
    evict_axis(ResidencyAxis::Navigation, config_.navigation_budget_bytes,
               [](const ResidencyState& state) { return state.navigation_bytes; });
    evict_axis(ResidencyAxis::Semantic, config_.semantic_budget_bytes,
               [](const ResidencyState& state) { return state.semantic_bytes; });
    return foundation::Result<std::uint32_t, foundation::Error>::success(evicted);
}

const ResidencyState* WorldStreamer::state(RegionId id) const noexcept {
    const auto iterator = states_.find(id.value());
    return iterator == states_.end() ? nullptr : &iterator->second;
}

std::vector<StreamedRegion> WorldStreamer::takeReady() {
    std::vector<StreamedRegion> result;
    result.swap(ready_);
    return result;
}

std::size_t WorldStreamer::residentCount() const noexcept {
    std::size_t count = 0U;
    for (const auto& [unused_id, state] : states_) {
        static_cast<void>(unused_id);
        count += has(state.current, ResidencyAxis::Semantic) ? 1U : 0U;
    }
    return count;
}

std::size_t WorldStreamer::residentBytes(ResidencyAxis axis) const noexcept {
    std::size_t total = 0U;
    for (const auto& [unused_id, state] : states_) {
        static_cast<void>(unused_id);
        if (axis == ResidencyAxis::Semantic) {
            total += state.semantic_bytes;
        } else if (axis == ResidencyAxis::Render) {
            total += state.render_bytes;
        } else if (axis == ResidencyAxis::Physics) {
            total += state.physics_bytes;
        } else if (axis == ResidencyAxis::Navigation) {
            total += state.navigation_bytes;
        }
    }
    return total;
}

} // namespace genomes::world
