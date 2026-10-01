#pragma once

#include <genomes/ballistics/BallisticsWorld.hpp>
#include <genomes/combat/AIJobPipeline.hpp>
#include <genomes/combat/CombatSystem.hpp>
#include <genomes/combat/LineOfSight.hpp>
#include <genomes/combat/PerceptionBroadphase.hpp>
#include <genomes/combat/SquadState.hpp>
#include <genomes/combat/TacticalAI.hpp>
#include <genomes/destruction/DamageField.hpp>
#include <genomes/destruction/MaterialAssembly.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/gameplay/BattlefieldTypes.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/InfantrySimulation.hpp>
#endif
#include <genomes/physics/PhysicsWorld.hpp>
#include <genomes/simulation/EntityStore.hpp>
#include <genomes/simulation/SystemGraph.hpp>
#include <genomes/spatial/SpatialGrid.hpp>
#include <genomes/world_core/WorldQuerySnapshot.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponController.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

// Authoritative battlefield owner.  It owns the ECS, infantry, physics,
// navigation, combat/ballistics pipeline and graph for a complete session.
// BattlefieldScenario is only a compatibility/fixture facade over this type.
class BattlefieldRuntime final {
public:
    BattlefieldRuntime(const BattlefieldRuntime&) = delete;
    BattlefieldRuntime& operator=(const BattlefieldRuntime&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldRuntime>,
                                             foundation::Error>
    start(const BattlefieldScenarioConfig& config = {}, jobs::JobSystem* jobs = nullptr);

    void fixedUpdate(double dt = 1.0 / 60.0) noexcept;
    void fixedUpdate(const simulation::TickContext& context) noexcept;

    [[nodiscard]] const BattlefieldScenarioSnapshot& snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] bool complete() const noexcept { return snapshot_.complete; }
    [[nodiscard]] const simulation::WorldEcs& ecs() const noexcept { return entities_.ecs(); }
    [[nodiscard]] const combat::TacticalAIProfile& tacticalProfile() const noexcept {
        return tactical_ai_.profile();
    }
    [[nodiscard]] const std::vector<infantry::InfantryRenderState>& renderStates() const noexcept {
        return infantry_->renderStates();
    }

private:
    struct TargetRuntime final {
        simulation::EntityId entity{};
        infantry::Team team{infantry::Team::Blue};
        destruction::MaterialAssembly assembly{};
        destruction::DamageField damage_field{};
    };

    BattlefieldRuntime(BattlefieldScenarioConfig config, jobs::JobSystem* jobs);

    [[nodiscard]] foundation::Result<void, foundation::Error> initialize();
    [[nodiscard]] foundation::Result<void, foundation::Error> configureWorldQuery();
    [[nodiscard]] foundation::Result<void, foundation::Error> configureAmmunition();
    [[nodiscard]] foundation::Result<void, foundation::Error> configureGraph();
    void runPerception() noexcept;
    void runDecision() noexcept;
    void queueFire() noexcept;
    void advanceBallistics() noexcept;
    void applyImpactDamage() noexcept;
    [[nodiscard]] TargetRuntime* target(simulation::EntityId entity) noexcept;
    [[nodiscard]] const TargetRuntime* target(simulation::EntityId entity) const noexcept;
    [[nodiscard]] static bool provideContact(void*, const world_core::QuerySegmentHit&,
                                              ballistics::ContactCandidate&) noexcept;

    BattlefieldScenarioConfig config_{};
    std::unique_ptr<jobs::JobSystem> owned_jobs_;
    jobs::JobSystem* jobs_{nullptr};
    simulation::EntityStore entities_;
    physics::SimplePhysicsWorld physics_{};
    std::unique_ptr<navigation::GridNavigationWorld> navigation_;
    std::unique_ptr<infantry::InfantrySimulation> infantry_;
    combat::CombatSystem combat_;
    combat::CombatCommandFlow combat_flow_;
    simulation::SystemGraph graph_;
    foundation::SimulationTick simulation_tick_{};

    spatial::UniformGrid perception_grid_{16.0F};
    combat::PerceptionBroadphase perception_;
    combat::TacticalAISystem tactical_ai_;
    combat::AIJobPipeline ai_pipeline_;
    combat::SquadSystem squads_;
    std::vector<combat::PerceptionAgent> perception_agents_;
    std::vector<combat::TacticalAIEntity> ai_entities_;
    std::vector<combat::AIState> ai_states_;
    std::vector<std::vector<combat::VisibleTarget>> visible_targets_;
    std::vector<combat::AIIntent> intents_;
    std::unordered_map<std::uint64_t, std::uint32_t> teams_;
    std::unordered_map<std::uint64_t, combat::AIState> ai_state_by_entity_;
    std::unordered_map<std::uint64_t, std::uint64_t> shot_sequences_;
    std::unordered_map<std::uint64_t, weapons::WeaponState> weapon_states_;
    weapons::WeaponSpec weapon_spec_{};
    weapons::WeaponId weapon_id_{0};

    world_core::WorldQuerySnapshot query_snapshot_{};
    std::vector<TargetRuntime> targets_;
    ballistics::BallisticsWorld* ballistics_{nullptr};
    std::unique_ptr<ballistics::BallisticsWorld> owned_ballistics_;
    std::unordered_map<std::uint64_t, simulation::EntityId> projectile_sources_;
    std::vector<ballistics::BallisticsContact> ballistic_contacts_;
    combat::DamageBuffer damage_buffer_;

    BattlefieldScenarioSnapshot snapshot_{};
};

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
