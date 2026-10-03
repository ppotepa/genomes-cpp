#include <genomes/render/RenderExtraction.hpp>

#include <cassert>

int main() {
    using namespace genomes;

    render::SnapshotExchange exchange{2};
    assert(exchange.slotCount() == 2);
    assert(!exchange.acquireLatestRead());

    auto first_writer = exchange.acquireWrite();
    assert(first_writer);
    first_writer.value().snapshot().frame_number = 1;
    assert(exchange.publish(std::move(first_writer.value())));

    {
        auto first_read = exchange.acquireLatestRead();
        assert(first_read);
        assert(first_read.value().snapshot().frame_number == 1);
        assert(exchange.state(0) == render::SnapshotSlotState::Reading ||
               exchange.state(1) == render::SnapshotSlotState::Reading);

        auto second_writer = exchange.acquireWrite();
        assert(second_writer);
        second_writer.value().snapshot().frame_number = 2;
        assert(exchange.publish(std::move(second_writer.value())));

        auto held_read = exchange.acquireLatestRead();
        assert(held_read);
        auto blocked_writer = exchange.acquireWrite();
        assert(!blocked_writer);
    }
    assert(exchange.acquireWrite());

    render::RenderExtractor extractor;
    render::PresentationSnapshot snapshot{};
    const foundation::StableId object_id = foundation::stable_id("extract.object");
    snapshot.frame_number = 10;
    snapshot.instances.push_back({object_id, foundation::stable_id("mesh"),
                                  foundation::stable_id("material"), {}, {1.0F, 1.0F, 1.0F},
                                  0.0F, 7});

    const auto added = extractor.extract(snapshot);
    assert(added && added.value().changes.size() == 1);
    assert(added.value().changes.front().kind == render::RenderChangeKind::Added);

    const auto unchanged = extractor.extract(snapshot);
    assert(unchanged && unchanged.value().empty());

    snapshot.instances.front().position.x = 3.0F;
    snapshot.instances.front().revision = 8;
    const auto updated = extractor.extract(snapshot);
    assert(updated && updated.value().changes.size() == 1);
    assert(updated.value().changes.front().kind == render::RenderChangeKind::Updated);

    const foundation::StableId second_object_id = foundation::stable_id("extract.object.second");
    snapshot.instances.push_back({second_object_id, foundation::stable_id("mesh"),
                                  foundation::stable_id("material"), {}, {1.0F, 1.0F, 1.0F},
                                  0.0F, 7});
    const auto second_added = extractor.extract(snapshot);
    assert(second_added && second_added.value().changes.size() == 1);
    assert(second_added.value().changes.front().semantic_id == second_object_id);

    snapshot.instances.clear();
    const auto removed = extractor.extract(snapshot);
    assert(removed && removed.value().changes.size() == 2);
    assert(removed.value().changes[0].kind == render::RenderChangeKind::Removed);
    assert(removed.value().changes[1].kind == render::RenderChangeKind::Removed);
    assert(removed.value().changes[0].semantic_id < removed.value().changes[1].semantic_id);
    assert(extractor.trackedCount() == 0);
    return 0;
}
