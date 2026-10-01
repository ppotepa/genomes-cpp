#include <genomes/runtime/UnitLabModelRequestGate.hpp>

#include <cassert>

int main() {
    using namespace genomes::runtime;
    UnitLabModelRequestGate gate;

    const auto a = gate.submit(10U);
    assert(a.token && !a.queued && gate.hasActive());
    const auto b = gate.submit(20U);
    const auto c = gate.submit(30U);
    assert(b.queued && c.queued && gate.hasPending());

    const auto stale = gate.complete(b.token);
    assert(stale.action == UnitLabModelRequestAction::Ignore);
    const auto promote = gate.complete(a.token);
    assert(promote.action == UnitLabModelRequestAction::StartPending);
    assert(promote.next == c.token && gate.hasActive() && !gate.hasPending());

    const auto publish = gate.complete(c.token);
    assert(publish.action == UnitLabModelRequestAction::Publish);
    assert(!gate.hasActive());

    const auto d = gate.submit(40U);
    gate.cancel();
    assert(gate.complete(d.token).action == UnitLabModelRequestAction::Ignore);
    return 0;
}
