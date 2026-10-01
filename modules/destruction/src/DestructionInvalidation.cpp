#include <genomes/destruction/DestructionInvalidation.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool reasonFor(InvalidationConsumer consumer,
                             world::DirtyReasonMask reasons) noexcept {
    switch (consumer) {
    case InvalidationConsumer::Navigation:
        return world::hasReason(reasons, world::DirtyReason::StructuralGeometry) ||
               world::hasReason(reasons, world::DirtyReason::RubbleSurface) ||
               world::hasReason(reasons, world::DirtyReason::Traversability);
    case InvalidationConsumer::StaticQuery:
        return world::hasReason(reasons, world::DirtyReason::StructuralGeometry) ||
               world::hasReason(reasons, world::DirtyReason::RubbleSurface) ||
               world::hasReason(reasons, world::DirtyReason::StaticCollision);
    case InvalidationConsumer::RenderRepresentation:
        return world::hasReason(reasons, world::DirtyReason::StructuralGeometry) ||
               world::hasReason(reasons, world::DirtyReason::RubbleSurface) ||
               world::hasReason(reasons, world::DirtyReason::Cover) ||
               world::hasReason(reasons, world::DirtyReason::Presentation);
    case InvalidationConsumer::Physics:
        return world::hasReason(reasons, world::DirtyReason::StructuralGeometry) ||
               world::hasReason(reasons, world::DirtyReason::RubbleSurface) ||
               world::hasReason(reasons, world::DirtyReason::StaticCollision);
    }
    return false;
}

} // namespace

bool DestructionInvalidationQueue::emit(world::WorldId world_id,
                                        foundation::StableId source_id,
                                        std::uint64_t semantic_revision,
                                        world::Aabb3 bounds,
                                        world::DirtyReasonMask reasons,
                                        std::int32_t layer) {
    if (!config_.valid() || !world_id.isValid() || source_id == 0 || semantic_revision == 0 ||
        (world_revision_ != 0U && semantic_revision != world_revision_) ||
        reasons == 0U || !bounds.valid()) {
        return false;
    }

    const double region_size = config_.coordinates.region_size_m;
    const auto min_region_x = static_cast<std::int64_t>(
        std::floor(static_cast<double>(bounds.minimum.x) / region_size));
    const auto max_region_x = static_cast<std::int64_t>(
        std::floor(static_cast<double>(bounds.maximum.x) / region_size));
    const auto min_region_z = static_cast<std::int64_t>(
        std::floor(static_cast<double>(bounds.minimum.z) / region_size));
    const auto max_region_z = static_cast<std::int64_t>(
        std::floor(static_cast<double>(bounds.maximum.z) / region_size));
    constexpr std::int64_t max_region_span = 4096;
    if (max_region_x - min_region_x > max_region_span ||
        max_region_z - min_region_z > max_region_span) {
        return false;
    }

    bool emitted = false;
    for (std::int64_t region_x = min_region_x; region_x <= max_region_x; ++region_x) {
        for (std::int64_t region_z = min_region_z; region_z <= max_region_z; ++region_z) {
            const world::RegionCoord region{region_x, region_z, layer};
            const double origin_x = static_cast<double>(region_x) * region_size;
            const double origin_z = static_cast<double>(region_z) * region_size;
            world::Aabb3 clipped = bounds;
            clipped.minimum.x = std::max(clipped.minimum.x, static_cast<float>(origin_x));
            clipped.maximum.x = std::min(
                clipped.maximum.x, static_cast<float>(origin_x + region_size));
            clipped.minimum.z = std::max(clipped.minimum.z, static_cast<float>(origin_z));
            clipped.maximum.z = std::min(
                clipped.maximum.z, static_cast<float>(origin_z + region_size));
            if (!clipped.valid()) {
                continue;
            }
            const world::DirtyBounds event{clipped,
                                           reasons,
                                           world_id,
                                           region,
                                           world::regionId(world_id, region),
                                           source_id,
                                           semantic_revision};
            enqueue(event);
            emitted = true;
        }
    }
    return emitted;
}

bool DestructionInvalidationQueue::emit(const world::DirtyBounds& event) {
    if (!config_.valid() || !event.valid() ||
        (world_revision_ != 0U && event.semantic_revision != world_revision_)) {
        return false;
    }
    enqueue(event);
    return true;
}

bool DestructionInvalidationQueue::emitRubbleTile(world::WorldId world_id,
                                                  foundation::StableId source_id,
                                                  std::uint64_t semantic_revision,
                                                  const RubbleField& field,
                                                  TileCoord tile) {
    const auto iterator = field.tiles().find(tile);
    if (iterator == field.tiles().end() || !config_.valid() ||
        (world_revision_ != 0U && semantic_revision != world_revision_)) {
        return false;
    }
    const RubbleFieldSpec& spec = field.spec();
    float minimum_y = 0.0F;
    float maximum_y = 0.0F;
    const RubbleTile& rubble_tile = iterator->second;
    for (std::uint32_t z = 0; z < rubble_tile.cellsPerSide(); ++z) {
        for (std::uint32_t x = 0; x < rubble_tile.cellsPerSide(); ++x) {
            if (rubble_tile.cellVolume(x, z) <= 1.0e-12) {
                continue;
            }
            const auto global_x = static_cast<std::int64_t>(tile.x) * spec.cells_per_tile + x;
            const auto global_z = static_cast<std::int64_t>(tile.z) * spec.cells_per_tile + z;
            const float world_x = (static_cast<float>(global_x) + 0.5F) * spec.cell_size_m;
            const float world_z = (static_cast<float>(global_z) + 0.5F) * spec.cell_size_m;
            const float pile = field.pileHeightAt(world_x, world_z);
            const float surface = field.surfaceHeightAt(world_x, world_z);
            if (!finite(pile) || !finite(surface)) {
                return false;
            }
            minimum_y = std::min(minimum_y, surface - pile);
            maximum_y = std::max(maximum_y, surface);
        }
    }
    const float minimum_x = static_cast<float>(tile.x) * spec.tile_size_m;
    const float minimum_z = static_cast<float>(tile.z) * spec.tile_size_m;
    return emit(world_id,
                source_id,
                semantic_revision,
                {{minimum_x, minimum_y, minimum_z},
                 {minimum_x + spec.tile_size_m, maximum_y, minimum_z + spec.tile_size_m}},
                world::DirtyReason::RubbleSurface | world::DirtyReason::Traversability |
                    world::DirtyReason::Cover | world::DirtyReason::StaticCollision |
                    world::DirtyReason::Presentation);
}

void DestructionInvalidationQueue::enqueue(const world::DirtyBounds& event) {
    for (std::size_t consumer = 0; consumer < consumer_count; ++consumer) {
        const auto kind = static_cast<InvalidationConsumer>(consumer);
        if (reasonFor(kind, event.reasons)) {
            queues_[consumer].push_back(event);
        }
    }
}

bool DestructionInvalidationQueue::canMerge(const world::DirtyBounds& left,
                                            const world::DirtyBounds& right) const noexcept {
    if (left.world != right.world || left.region != right.region ||
        left.region_id != right.region_id || left.semantic_revision != right.semantic_revision) {
        return false;
    }
    if (!left.world_aabb.overlaps(right.world_aabb, config_.coalesce_distance_m)) {
        return false;
    }
    const world::Aabb3 united = left.world_aabb.united(right.world_aabb);
    const foundation::Vec3 union_extent = united.extent();
    const foundation::Vec3 left_extent = left.world_aabb.extent();
    const foundation::Vec3 right_extent = right.world_aabb.extent();
    return union_extent.x <= std::max(left_extent.x, right_extent.x) + config_.max_expansion_m &&
           union_extent.y <= std::max(left_extent.y, right_extent.y) + config_.max_expansion_m &&
           union_extent.z <= std::max(left_extent.z, right_extent.z) + config_.max_expansion_m;
}

bool DestructionInvalidationQueue::sameEvent(const world::DirtyBounds& left,
                                             const world::DirtyBounds& right) noexcept {
    return left.world == right.world && left.region == right.region &&
           left.region_id == right.region_id && left.source_id == right.source_id &&
           left.semantic_revision == right.semantic_revision && left.reasons == right.reasons &&
           left.world_aabb.minimum.x == right.world_aabb.minimum.x &&
           left.world_aabb.minimum.y == right.world_aabb.minimum.y &&
           left.world_aabb.minimum.z == right.world_aabb.minimum.z &&
           left.world_aabb.maximum.x == right.world_aabb.maximum.x &&
           left.world_aabb.maximum.y == right.world_aabb.maximum.y &&
           left.world_aabb.maximum.z == right.world_aabb.maximum.z;
}

void DestructionInvalidationQueue::coalesce(InvalidationConsumer consumer) {
    auto& queue = queues_[index(consumer)];
    std::stable_sort(queue.begin(), queue.end(), [](const auto& left, const auto& right) {
        if (left.region != right.region) {
            return left.region < right.region;
        }
        if (left.semantic_revision != right.semantic_revision) {
            return left.semantic_revision < right.semantic_revision;
        }
        if (left.source_id != right.source_id) {
            return left.source_id < right.source_id;
        }
        if (left.world_aabb.minimum.x != right.world_aabb.minimum.x) {
            return left.world_aabb.minimum.x < right.world_aabb.minimum.x;
        }
        if (left.world_aabb.minimum.z != right.world_aabb.minimum.z) {
            return left.world_aabb.minimum.z < right.world_aabb.minimum.z;
        }
        return left.world_aabb.minimum.y < right.world_aabb.minimum.y;
    });

    std::vector<world::DirtyBounds> merged;
    merged.reserve(queue.size());
    for (const world::DirtyBounds& event : queue) {
        if (!merged.empty() && canMerge(merged.back(), event)) {
            world::DirtyBounds& target = merged.back();
            target.world_aabb = target.world_aabb.united(event.world_aabb);
            target.reasons |= event.reasons;
            target.source_id = std::min(target.source_id, event.source_id);
        } else {
            merged.push_back(event);
        }
    }
    queue = std::move(merged);
}

std::vector<world::DirtyBounds> DestructionInvalidationQueue::pending(
    InvalidationConsumer consumer) const {
    return queues_[index(consumer)];
}

std::size_t DestructionInvalidationQueue::pendingCount(InvalidationConsumer consumer) const noexcept {
    return queues_[index(consumer)].size();
}

bool DestructionInvalidationQueue::acknowledge(InvalidationConsumer consumer,
                                               const world::DirtyBounds& event) {
    auto& queue = queues_[index(consumer)];
    const auto iterator = std::find_if(queue.begin(), queue.end(), [&event](const auto& current) {
        return sameEvent(current, event);
    });
    if (iterator == queue.end()) {
        return false;
    }
    queue.erase(iterator);
    return true;
}

} // namespace genomes::destruction
