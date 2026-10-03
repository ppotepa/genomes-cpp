#include <genomes/foundation/PublishedSnapshotExchange.hpp>

#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>

namespace {

struct Snapshot final {
    genomes::foundation::SnapshotMetadata metadata{};
    std::uint64_t value{0};

    void clear() noexcept {
        metadata = {};
        value = 0;
    }
};

void publish(genomes::foundation::PublishedSnapshotExchange<Snapshot>& exchange,
             std::uint64_t epoch,
             std::uint64_t value) {
    auto write = exchange.acquireWrite();
    assert(write);
    write.value().snapshot().metadata.scene_epoch = epoch;
    write.value().snapshot().metadata.tick = value;
    write.value().snapshot().metadata.revision = value;
    write.value().snapshot().value = value;
    assert(exchange.publish(std::move(write.value())));
}

} // namespace

int main() {
    genomes::foundation::PublishedSnapshotExchange<Snapshot> exchange(3);
    assert(!exchange.acquireLatestRead());
    publish(exchange, 1, 10);
    auto first_result = exchange.acquireLatestRead();
    assert(first_result);
    using ReadLease = genomes::foundation::PublishedSnapshotExchange<Snapshot>::ReadLease;
    std::optional<ReadLease> first(std::move(first_result.value()));
    assert(first->snapshot().metadata.generation == 1);

    publish(exchange, 1, 20);
    auto second_result = exchange.acquireLatestRead();
    assert(second_result);
    std::optional<ReadLease> second(std::move(second_result.value()));
    assert(second->snapshot().value == 20);
    assert(second->snapshot().metadata.generation == 2);

    // Completion order may differ from publication order. A stale revision
    // must be discarded without disturbing the newer readable snapshot.
    auto stale_revision = exchange.acquireWrite();
    assert(stale_revision);
    stale_revision.value().snapshot().metadata.scene_epoch = 1;
    stale_revision.value().snapshot().metadata.revision = 15;
    stale_revision.value().snapshot().value = 15;
    assert(!exchange.publish(std::move(stale_revision.value())));
    assert(second->snapshot().value == 20);

    publish(exchange, 1, 30);
    assert(!exchange.acquireWrite());
    assert(first->snapshot().value == 10);
    assert(second->snapshot().value == 20);

    first.reset();
    exchange.rejectBeforeSceneEpoch(2);
    assert(!exchange.acquireLatestRead());
    auto stale = exchange.acquireWrite();
    assert(stale);
    stale.value().snapshot().metadata.scene_epoch = 1;
    assert(!exchange.publish(std::move(stale.value())));

    second.reset();
    // The reader was still holding the old scene when the epoch advanced;
    // releasing it must not resurrect that stale snapshot as latest.
    assert(!exchange.acquireLatestRead());
    publish(exchange, 2, 40);
    auto latest = exchange.acquireLatestRead();
    assert(latest);
    assert(latest.value().snapshot().value == 40);
    assert(latest.value().snapshot().metadata.generation == 4);

    // Advancing the epoch while the latest slot is held by a reader must not
    // let that stale slot become current again when the reader is released.
    genomes::foundation::PublishedSnapshotExchange<Snapshot> stalled(3);
    publish(stalled, 7, 70);
    auto stalled_result = stalled.acquireLatestRead();
    assert(stalled_result);
    std::optional<ReadLease> stalled_reader(std::move(stalled_result.value()));
    stalled.rejectBeforeSceneEpoch(8);
    assert(!stalled.acquireLatestRead());
    stalled_reader.reset();
    assert(!stalled.acquireLatestRead());
    publish(stalled, 8, 80);
    auto recovered = stalled.acquireLatestRead();
    assert(recovered);
    assert(recovered.value().snapshot().value == 80);
    return 0;
}
