#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/combat/CombatCommands.hpp>
#include <genomes/combat/CombatEvents.hpp>
#include <genomes/simulation/EntityStore.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace genomes::combat {

struct DamageEvent final {
    simulation::EntityId source{};
    simulation::EntityId target{};
    float amount{0.0F};
    DamageType type{DamageType::Kinetic};
    foundation::SimulationTick tick{};
};

class DamageBuffer final {
public:
    void clear() noexcept { events_.clear(); }
    void push(DamageEvent event) { events_.push_back(event); }

    [[nodiscard]] const std::vector<DamageEvent>& events() const noexcept { return events_; }

private:
    friend class CombatSystem;
    std::vector<DamageEvent> events_;
};

struct CombatApplyResult final {
    std::uint32_t accepted_events{0};
    std::uint32_t killed_entities{0};
};

// Damage is collected by parallel systems and committed in one ordered phase.
// This keeps two impacts on the same entity deterministic regardless of worker
// completion order.
class CombatSystem final {
public:
    explicit CombatSystem(simulation::EntityStore& entities) noexcept : entities_{entities} {}

    [[nodiscard]] CombatApplyResult apply(DamageBuffer&) noexcept;

private:
    simulation::EntityStore& entities_;
};

// Deliberately named fixture-only damage path. Production weapon flow must
// use CombatCommandFlow and ballistics; this adapter exists for the legacy
// BattlefieldScene fallback until that path is fully migrated.
class FixtureHitscan final {
public:
    explicit FixtureHitscan(simulation::EntityStore& entities) noexcept
        : system_{entities} {}

    [[nodiscard]] CombatApplyResult apply(DamageBuffer& buffer) noexcept {
        return system_.apply(buffer);
    }

private:
    CombatSystem system_;
};

// Typed command/event bridge. It has no renderer or physics ownership; each
// phase returns a stable value buffer for the next authoritative stage.
class CombatCommandFlow final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> submitFire(
        const weapons::FireIntent&) noexcept;
    [[nodiscard]] foundation::Result<std::vector<FireRequest>, foundation::Error> commitFire(
        foundation::StableId match_seed);
    [[nodiscard]] foundation::Result<void, foundation::Error> submitImpact(
        ImpactEvent) noexcept;
    [[nodiscard]] foundation::Result<std::vector<DamageCommand>, foundation::Error> commitDamage();
    [[nodiscard]] const std::vector<CombatPresentationEvent>& presentation() const noexcept {
        return presentation_;
    }
    void clear() noexcept;

private:
    std::vector<weapons::FireIntent> fire_intents_;
    std::vector<ImpactEvent> impacts_;
    std::vector<CombatPresentationEvent> presentation_;
    std::vector<std::pair<foundation::StableId, std::uint64_t>> committed_shots_;
};

} // namespace genomes::combat
