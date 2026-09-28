#pragma once

#include <genomes/combat/TacticalSignal.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace genomes::combat {

struct SquadContact final {
    simulation::EntityId subject{};
    foundation::Vec3 last_known_position{};
    simulation::EntityId reported_by{};
    foundation::SimulationTick observed_tick{};
    foundation::SimulationTick received_tick{};
    float confidence{0.0F};
    TacticalSignalType type{TacticalSignalType::Contact};
    std::uint64_t provenance_sequence{0U};
    bool shared{true};

    [[nodiscard]] bool valid() const noexcept;
};

struct SquadState final {
    SquadId id{0U};
    std::vector<simulation::EntityId> members;
    std::vector<SquadContact> contacts;
    foundation::Vec3 order_location{};
    foundation::SimulationTick order_tick{};
    std::uint32_t order_revision{0U};
    bool has_order{false};
    std::uint64_t revision{0U};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool containsMember(simulation::EntityId entity) const noexcept;
};

class SquadSystem final {
public:
    explicit SquadSystem(SignalPropagationPolicy policy = {}) noexcept : policy_(policy) {}

    [[nodiscard]] foundation::Result<void, foundation::Error> create(
        SquadId id, std::span<const simulation::EntityId> members);
    [[nodiscard]] foundation::Result<void, foundation::Error> setMembers(
        SquadId id, std::span<const simulation::EntityId> members);
    [[nodiscard]] foundation::Result<void, foundation::Error> removeMember(
        SquadId id, simulation::EntityId member);
    [[nodiscard]] foundation::Result<void, foundation::Error> remove(SquadId id);

    [[nodiscard]] foundation::Result<void, foundation::Error> schedule(TacticalSignal signal);
    [[nodiscard]] foundation::Result<std::uint32_t, foundation::Error> deliver(
        foundation::SimulationTick tick);

    [[nodiscard]] const SquadState* find(SquadId id) const noexcept;
    [[nodiscard]] SquadState* find(SquadId id) noexcept;
    [[nodiscard]] const SignalPropagationPolicy& policy() const noexcept { return policy_; }
    [[nodiscard]] std::uint32_t deliveredCount() const noexcept { return delivered_count_; }
    [[nodiscard]] std::uint32_t droppedCount() const noexcept { return dropped_count_; }
    [[nodiscard]] std::size_t pendingCount() const noexcept { return pending_.size(); }

private:
    [[nodiscard]] static bool validMembers(std::span<const simulation::EntityId> members) noexcept;
    [[nodiscard]] SquadState* findMutable(SquadId id) noexcept;
    void decayContacts(SquadState& squad, foundation::SimulationTick tick) const noexcept;
    void merge(SquadState& squad, const TacticalSignal& signal, foundation::SimulationTick tick);

    SignalPropagationPolicy policy_{};
    std::vector<SquadState> squads_;
    std::vector<TacticalSignal> pending_;
    std::uint64_t next_sequence_{1U};
    std::uint32_t delivered_count_{0U};
    std::uint32_t dropped_count_{0U};
};

} // namespace genomes::combat
