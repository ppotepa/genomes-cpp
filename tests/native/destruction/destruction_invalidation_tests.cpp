#include <genomes/destruction/DestructionInvalidation.hpp>

#include <cassert>
#include <utility>

int main() {
    using namespace genomes::destruction;
    using genomes::world::Aabb3;
    using genomes::world::DirtyReason;
    using genomes::world::DirtyReasonMask;
    using genomes::world::WorldId;

    DestructionInvalidationConfig config{};
    config.coordinates.region_size_m = 4.0;
    config.coalesce_distance_m = 0.25F;
    config.max_expansion_m = 2.0F;
    DestructionInvalidationQueue queue{config};
    const WorldId world_id{7U};

    assert(queue.emit(world_id,
                      100U,
                      1U,
                      {{3.5F, 0.0F, 0.5F}, {4.5F, 1.0F, 1.5F}},
                      static_cast<DirtyReasonMask>(DirtyReason::StructuralGeometry) |
                          static_cast<DirtyReasonMask>(DirtyReason::Traversability)));
    assert(queue.pendingCount(InvalidationConsumer::Navigation) == 2U);
    assert(queue.pendingCount(InvalidationConsumer::StaticQuery) == 2U);
    assert(queue.pendingCount(InvalidationConsumer::RenderRepresentation) == 2U);
    assert(queue.pendingCount(InvalidationConsumer::Physics) == 2U);
    const auto boundary = queue.pending(InvalidationConsumer::Navigation);
    assert(boundary[0].region.x != boundary[1].region.x);

    assert(queue.emit(world_id,
                      101U,
                      2U,
                      {{0.0F, 0.0F, 0.0F}, {0.25F, 1.0F, 0.25F}},
                      static_cast<DirtyReasonMask>(DirtyReason::RubbleSurface)));
    assert(queue.emit(world_id,
                      102U,
                      2U,
                      {{0.2F, 0.0F, 0.2F}, {0.45F, 1.0F, 0.45F}},
                      static_cast<DirtyReasonMask>(DirtyReason::Cover)));
    assert(queue.emit(world_id,
                      103U,
                      3U,
                      {{0.2F, 0.0F, 0.2F}, {0.45F, 1.0F, 0.45F}},
                      static_cast<DirtyReasonMask>(DirtyReason::Cover)));
    queue.coalesce(InvalidationConsumer::RenderRepresentation);
    const auto render_pending = queue.pending(InvalidationConsumer::RenderRepresentation);
    assert(render_pending.size() == 4U);

    const auto before_failed_consumer = queue.pendingCount(InvalidationConsumer::Physics);
    assert(!queue.acknowledge(InvalidationConsumer::Physics,
                              {Aabb3{{99.0F, 0.0F, 99.0F}, {100.0F, 1.0F, 100.0F}},
                               static_cast<DirtyReasonMask>(DirtyReason::StaticCollision),
                               world_id,
                               {},
                               {},
                               999U,
                               1U}));
    assert(queue.pendingCount(InvalidationConsumer::Physics) == before_failed_consumer);
    const auto physics_pending = queue.pending(InvalidationConsumer::Physics);
    assert(queue.acknowledge(InvalidationConsumer::Physics, physics_pending.front()));
    assert(queue.pendingCount(InvalidationConsumer::Physics) + 1U == before_failed_consumer);

    const auto field_result = RubbleField::create();
    assert(field_result);
    RubbleField field = std::move(field_result.value());
    assert(field.deposit({200U, {0.0F, 0.0F, 0.0F}, 0.0F,
                           {{MaterialId::fromName("brick"), 1.0}}}));
    assert(queue.emitRubbleTile(world_id, 200U, 4U, field, {0, 0}));
    assert(queue.pendingCount(InvalidationConsumer::Navigation) >= 3U);
    return 0;
}
