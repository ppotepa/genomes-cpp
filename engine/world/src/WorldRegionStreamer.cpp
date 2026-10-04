#include <genomes/world/WorldRegionStreamer.hpp>

#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace genomes::world {

namespace {

void translate_plan(WorldPlan& plan, WorldPosition origin) noexcept {
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

} // namespace

WorldRegionStreamer::WorldRegionStreamer(WorldId world_id,
                                         WorldGenerationRequest request,
                                         WorldCoordinateConfig coordinates,
                                         jobs::JobSystem& jobs,
                                         std::uint32_t load_radius)
    : world_id_{world_id},
      request_{request},
      coordinates_{coordinates},
      cache_{std::make_shared<proc::ArtifactCache>()},
      generation_service_{jobs, cache_},
      load_radius_{std::min(load_radius, 4U)} {}

WorldRegionStreamer::WorldRegionStreamer(WorldId world_id,
                                         WorldGenerationRequest request,
                                         WorldCoordinateConfig coordinates,
                                         proc::GenerationClient generation,
                                         std::uint32_t load_radius)
    : world_id_{world_id},
      request_{request},
      coordinates_{coordinates},
      generation_service_{generation},
      load_radius_{std::min(load_radius, 4U)} {}

void WorldRegionStreamer::update(WorldPosition observer) {
    if (!world_id_.isValid() || !coordinates_.valid() || !request_.valid()) {
        return;
    }

    for (auto& [unused_id, pending] : pending_) {
        static_cast<void>(unused_id);
        pending.wanted = false;
    }

    const RegionCoord center = regionCoordFor(observer, coordinates_);
    std::map<std::uint64_t, RegionCoord> desired;
    const auto radius = static_cast<std::int64_t>(load_radius_);
    for (std::int64_t dz = -radius; dz <= radius; ++dz) {
        for (std::int64_t dx = -radius; dx <= radius; ++dx) {
            const RegionCoord coord{center.x + dx, center.z + dz, center.layer};
            const RegionId id = regionId(world_id_, coord);
            desired.emplace(id.value(), coord);
            const auto pending = pending_.find(id.value());
            if (pending != pending_.end()) {
                pending->second.wanted = true;
            } else if (resident_.find(id.value()) == resident_.end()) {
                schedule(coord, id);
            }
        }
    }

    for (auto it = resident_.begin(); it != resident_.end();) {
        if (desired.find(it->first) == desired.end()) {
            it = resident_.erase(it);
        } else {
            ++it;
        }
    }
    ready_.erase(std::remove_if(ready_.begin(), ready_.end(), [this](const WorldRegion& region) {
                     return resident_.find(region.id.value()) == resident_.end();
                 }),
                 ready_.end());

    // A region that left the desired set must not consume worker time until
    // completion.  Its ticket state is independent of this map entry, so it
    // is safe to cancel and release the entry; a later re-entry schedules a
    // fresh request with the same deterministic seed path.
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->second.wanted) {
            ++it;
            continue;
        }
        it->second.task.cancel();
        it = pending_.erase(it);
    }
}

void WorldRegionStreamer::poll() {
    for (auto it = pending_.begin(); it != pending_.end();) {
        PendingRegion& pending = it->second;
        if (!pending.task.ready()) {
            ++it;
            continue;
        }

        if (pending.wanted && !pending.task.failed()) {
            if (auto plan = pending.task.take_result()) {
                translate_plan(*plan, pending.origin);
                resident_.emplace(pending.id.value(), pending.coord);
                ready_.push_back({pending.id, pending.coord, pending.origin,
                                  std::move(*plan)});
            }
        } else {
            // A no-longer-wanted region is canceled deliberately by update();
            // that is normal streaming backpressure, not a generation error.
            if (pending.wanted && pending.task.failed()) {
                failed_ = true;
                error_ = pending.task.error();
            }
            (void)pending.task.take_result();
        }
        it = pending_.erase(it);
    }
}

std::vector<WorldRegion> WorldRegionStreamer::take_ready() {
    std::vector<WorldRegion> result;
    result.swap(ready_);
    return result;
}

std::size_t WorldRegionStreamer::pending_count() const noexcept {
    return pending_.size();
}

std::size_t WorldRegionStreamer::loaded_count() const noexcept {
    return resident_.size();
}

void WorldRegionStreamer::schedule(RegionCoord coord, RegionId id) {
    const auto region_size = static_cast<std::uint32_t>(std::lround(coordinates_.region_size_m));
    if (region_size < 128 || region_size > 4096) {
        failed_ = true;
        error_ = {foundation::ErrorCode::InvalidArgument,
                  "region size is outside the world generator limits"};
        return;
    }

    proc::SeedPath region_path(request_.seed);
    region_path = region_path.child("region-x", static_cast<std::uint64_t>(coord.x));
    region_path = region_path.child("region-z", static_cast<std::uint64_t>(coord.z));
    region_path = region_path.child("region-layer", static_cast<std::uint64_t>(
                                                          static_cast<std::int64_t>(coord.layer)));
    WorldGenerationRequest region_request = request_;
    region_request.seed = region_path.seed();
    region_request.map_size_m = region_size;

    PendingRegion pending{};
    pending.id = id;
    pending.coord = coord;
    pending.origin = regionOrigin(coord, coordinates_);
    pending.task = generation_service_.submit(region_request);
    pending.wanted = true;
    pending_.emplace(id.value(), std::move(pending));
}

} // namespace genomes::world
