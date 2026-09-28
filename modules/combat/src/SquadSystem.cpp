#include <genomes/combat/SquadState.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace genomes::combat {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] std::uint64_t subjectKey(const TacticalSignal& signal) noexcept {
    return signal.subject.has_value() ? signal.subject->packed() : 0U;
}

[[nodiscard]] bool memberLess(simulation::EntityId left, simulation::EntityId right) noexcept {
    return left.packed() < right.packed();
}

} // namespace

bool TacticalSignal::valid() const noexcept {
    return source.isValid() && squad != 0U && finite(location) &&
           (subject.has_value() ? subject->isValid() : type == TacticalSignalType::Order) &&
           delivery_tick >= created_tick && ttl_ticks > 0U && hops <= 32U && finite(confidence) &&
           confidence >= 0.0F && confidence <= 1.0F && finite(range_m) && range_m >= 0.0F;
}

bool SignalPropagationPolicy::valid() const noexcept {
    return max_hops > 0U && max_pending_signals > 0U && finite(max_range_m) && max_range_m >= 0.0F &&
           finite(confidence_decay_per_tick) && confidence_decay_per_tick > 0.0F &&
           confidence_decay_per_tick <= 1.0F;
}

bool SquadContact::valid() const noexcept {
    return subject.isValid() && reported_by.isValid() && finite(last_known_position) &&
           finite(confidence) && confidence >= 0.0F && confidence <= 1.0F;
}

bool SquadState::valid() const noexcept {
    if (id == 0U || members.empty()) {
        return false;
    }
    for (std::size_t index = 0U; index < members.size(); ++index) {
        if (!members[index].isValid() || (index > 0U && members[index - 1U] == members[index])) {
            return false;
        }
    }
    for (const SquadContact& contact : contacts) {
        if (!contact.valid()) {
            return false;
        }
    }
    return !has_order || finite(order_location);
}

bool SquadState::containsMember(simulation::EntityId entity) const noexcept {
    return std::binary_search(members.begin(), members.end(), entity, memberLess);
}

bool SquadSystem::validMembers(std::span<const simulation::EntityId> members) noexcept {
    if (members.empty()) {
        return false;
    }
    for (std::size_t index = 0U; index < members.size(); ++index) {
        if (!members[index].isValid() ||
            (index > 0U && !(members[index - 1U] < members[index]))) {
            return false;
        }
    }
    return true;
}

SquadState* SquadSystem::findMutable(SquadId id) noexcept {
    for (SquadState& squad : squads_) {
        if (squad.id == id) {
            return &squad;
        }
    }
    return nullptr;
}

const SquadState* SquadSystem::find(SquadId id) const noexcept {
    for (const SquadState& squad : squads_) {
        if (squad.id == id) {
            return &squad;
        }
    }
    return nullptr;
}

SquadState* SquadSystem::find(SquadId id) noexcept { return findMutable(id); }

foundation::Result<void, foundation::Error> SquadSystem::create(
    SquadId id, std::span<const simulation::EntityId> members) {
    if (id == 0U || find(id) != nullptr || !validMembers(members)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid or duplicate squad"});
    }
    squads_.push_back({id, {members.begin(), members.end()}, {}, {}, {}, 0U, false, 0U});
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> SquadSystem::setMembers(
    SquadId id, std::span<const simulation::EntityId> members) {
    SquadState* squad = findMutable(id);
    if (squad == nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "squad not found"});
    }
    if (!validMembers(members)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid squad members"});
    }
    squad->members.assign(members.begin(), members.end());
    squad->revision++;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> SquadSystem::removeMember(
    SquadId id, simulation::EntityId member) {
    SquadState* squad = findMutable(id);
    if (squad == nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "squad not found"});
    }
    const auto iterator = std::lower_bound(squad->members.begin(), squad->members.end(), member,
                                           memberLess);
    if (iterator == squad->members.end() || *iterator != member) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "squad member not found"});
    }
    squad->members.erase(iterator);
    squad->revision++;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> SquadSystem::remove(SquadId id) {
    const auto iterator = std::find_if(squads_.begin(), squads_.end(),
                                       [id](const SquadState& squad) { return squad.id == id; });
    if (iterator == squads_.end()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "squad not found"});
    }
    squads_.erase(iterator);
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                  [id](const TacticalSignal& signal) { return signal.squad == id; }),
                   pending_.end());
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> SquadSystem::schedule(TacticalSignal signal) {
    if (!policy_.valid() || !signal.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid signal or propagation policy"});
    }
    if (!policy_.enabled) {
        return foundation::Result<void, foundation::Error>::success();
    }
    SquadState* squad = findMutable(signal.squad);
    if (squad == nullptr || !squad->containsMember(signal.source)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "signal source is not a squad member"});
    }
    if (signal.hops > policy_.max_hops || signal.range_m > policy_.max_range_m ||
        pending_.size() >= policy_.max_pending_signals) {
        ++dropped_count_;
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "signal propagation budget exceeded"});
    }
    signal.stable_sequence = next_sequence_++;
    pending_.push_back(signal);
    return foundation::Result<void, foundation::Error>::success();
}

void SquadSystem::decayContacts(SquadState& squad, foundation::SimulationTick tick) const noexcept {
    for (SquadContact& contact : squad.contacts) {
        if (tick <= contact.received_tick) {
            continue;
        }
        const std::uint64_t delta = tick.value - contact.received_tick.value;
        contact.confidence *= std::pow(policy_.confidence_decay_per_tick,
                                       static_cast<float>(delta));
        contact.received_tick = tick;
    }
    squad.contacts.erase(std::remove_if(squad.contacts.begin(), squad.contacts.end(),
                                        [](const SquadContact& contact) {
                                            return contact.confidence < 0.01F;
                                        }),
                         squad.contacts.end());
}

void SquadSystem::merge(SquadState& squad, const TacticalSignal& signal,
                        foundation::SimulationTick tick) {
    if (signal.type == TacticalSignalType::Order) {
        if (!squad.has_order || signal.created_tick >= squad.order_tick) {
            squad.order_location = signal.location;
            squad.order_tick = signal.created_tick;
            squad.order_revision++;
            squad.has_order = true;
            squad.revision++;
        }
        return;
    }
    if (!signal.subject.has_value()) {
        return;
    }
    const auto iterator = std::find_if(squad.contacts.begin(), squad.contacts.end(),
                                       [&signal](const SquadContact& contact) {
                                           return contact.subject == *signal.subject &&
                                                  contact.type == signal.type;
                                       });
    if (iterator == squad.contacts.end()) {
        squad.contacts.push_back({*signal.subject, signal.location, signal.source,
                                  signal.created_tick, tick, signal.confidence, signal.type,
                                  signal.stable_sequence, true});
        squad.revision++;
        return;
    }
    const bool newer = signal.created_tick > iterator->observed_tick;
    const bool same_tick_better = signal.created_tick == iterator->observed_tick &&
                                  signal.confidence > iterator->confidence;
    if (newer || same_tick_better) {
        *iterator = {*signal.subject, signal.location, signal.source, signal.created_tick, tick,
                     signal.confidence, signal.type, signal.stable_sequence, true};
        squad.revision++;
    }
}

foundation::Result<std::uint32_t, foundation::Error> SquadSystem::deliver(
    foundation::SimulationTick tick) {
    if (!policy_.valid()) {
        return foundation::Result<std::uint32_t, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid propagation policy"});
    }
    for (SquadState& squad : squads_) {
        decayContacts(squad, tick);
    }
    std::vector<TacticalSignal> due;
    auto iterator = pending_.begin();
    while (iterator != pending_.end()) {
        if (iterator->delivery_tick <= tick) {
            due.push_back(*iterator);
            iterator = pending_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    std::stable_sort(due.begin(), due.end(), [](const TacticalSignal& left,
                                                const TacticalSignal& right) noexcept {
        const std::uint64_t left_subject = subjectKey(left);
        const std::uint64_t right_subject = subjectKey(right);
        return std::tuple{left.squad, left_subject, left.created_tick.value,
                          left.source.packed(), left.stable_sequence} <
               std::tuple{right.squad, right_subject, right.created_tick.value,
                          right.source.packed(), right.stable_sequence};
    });
    std::uint32_t delivered = 0U;
    for (const TacticalSignal& signal : due) {
        SquadState* squad = findMutable(signal.squad);
        const bool expired = tick.value > signal.created_tick.value + signal.ttl_ticks;
        if (squad == nullptr || !squad->containsMember(signal.source) || expired) {
            ++dropped_count_;
            continue;
        }
        merge(*squad, signal, tick);
        ++delivered;
    }
    delivered_count_ += delivered;
    return foundation::Result<std::uint32_t, foundation::Error>::success(delivered);
}

} // namespace genomes::combat
