#include <genomes/combat/SquadState.hpp>

#include <cassert>
#include <vector>

int main() {
    using namespace genomes;
    const simulation::EntityId source{1U, 1U};
    const simulation::EntityId receiver{2U, 1U};
    const simulation::EntityId enemy{3U, 1U};
    const std::vector<simulation::EntityId> members{source, receiver};
    combat::SquadSystem system;
    assert(system.create(7U, members));
    combat::TacticalSignal contact{source, 7U, enemy, {10.0F, 1.0F, 5.0F}, {0U}, {3U}, 20U,
                                   0U, 0.9F, 20.0F, combat::TacticalSignalType::Contact,
                                   combat::TacticalSignalChannel::Radio};
    assert(system.schedule(contact));
    assert(system.deliver({2U}).value() == 0U);
    assert(system.deliver({3U}).value() == 1U);
    assert(system.find(7U)->contacts.size() == 1U);
    assert(system.find(7U)->contacts.front().shared);
    contact.delivery_tick = {4U};
    contact.confidence = 0.1F;
    assert(system.schedule(contact));
    assert(system.deliver({4U}));
    assert(system.find(7U)->contacts.front().confidence > 0.8F);
    combat::TacticalSignal order{receiver, 7U, {}, {20.0F, 0.0F, 20.0F}, {5U}, {6U}, 20U, 0U,
                                 1.0F, 10.0F, combat::TacticalSignalType::Order,
                                 combat::TacticalSignalChannel::Radio};
    assert(system.schedule(order));
    assert(system.deliver({6U}));
    assert(system.find(7U)->has_order);
    assert(system.removeMember(7U, source));
    assert(!system.schedule(contact));
    return 0;
}
