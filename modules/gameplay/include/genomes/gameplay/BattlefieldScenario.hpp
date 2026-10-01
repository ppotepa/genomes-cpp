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
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/InfantrySimulation.hpp>
#endif
#include <genomes/physics/PhysicsWorld.hpp>
#include <genomes/simulation/EntityStore.hpp>
#include <genomes/simulation/SystemGraph.hpp>
#include <genomes/spatial/SpatialGrid.hpp>
#include <genomes/world/WorldQuerySnapshot.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponController.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

// A deliberately small, deterministic cross-module slice used by headless and
// the battlefield scene. It is not a replacement for the large-world runtime.
struct BattlefieldScenarioConfig final {
    std::uint64_t seed{0xC0FFEEU};
    std::uint32_t map_size_m{25U};
    float fixed_step_seconds{1.0F / 60.0F};
    std::uint32_t max_ticks{240U};
    combat::TacticalAIProfile tactical_ai_profile{};

    [[nodiscard]] bool valid() const noexcept {
        return seed != 0U && map_size_m == 25U && fixed_step_seconds > 0.0F &&
               max_ticks > 0U && tactical_ai_profile.valid();
    }
};

struct BattlefieldScenarioSnapshot final {
    std::uint64_t tick{0U};
    std::uint32_t map_size_m{25U};
    std::size_t ecs_entities{0U};
    std::size_t spawned{0U};
    std::size_t perceived{0U};
    std::size_t intents{0U};
    std::size_t fired{0U};
    std::size_t active_projectiles{0U};
    std::uint64_t physics_steps{0U};
    std::size_t impacts{0U};
    std::size_t accepted_damage{0U};
    std::size_t deaths{0U};
    std::size_t alive_units{0U};
    float destruction_damage{0.0F};
    std::size_t destruction_holes{0U};
    bool complete{false};
    std::string error;
};

class BattlefieldScenario final {
public:
    BattlefieldScenario(const BattlefieldScenario&) = delete;
    BattlefieldScenario& operator=(const BattlefieldScenario&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldScenario>,
                                             foundation::Error>
    startBattlefieldScenario(const BattlefieldScenarioConfig& config = {},
                             jobs::JobSystem* jobs = nullptr);

    void fixedUpdate(double dt = 1.0 / 60.0) noexcept;

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

    BattlefieldScenario(BattlefieldScenarioConfig config, jobs::JobSystem* jobs);

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
    [[nodiscard]] static bool provideContact(void*, const world::QuerySegmentHit&,
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

    world::WorldQuerySnapshot query_snapshot_{};
    std::vector<TargetRuntime> targets_;
    ballistics::BallisticsWorld* ballistics_{nullptr};
    std::unique_ptr<ballistics::BallisticsWorld> owned_ballistics_;
    std::unordered_map<std::uint64_t, simulation::EntityId> projectile_sources_;
    std::vector<ballistics::BallisticsContact> ballistic_contacts_;
    combat::DamageBuffer damage_buffer_;

    BattlefieldScenarioSnapshot snapshot_{};
};

[[nodiscard]] inline foundation::Result<std::unique_ptr<BattlefieldScenario>, foundation::Error>
startBattlefieldScenario(const BattlefieldScenarioConfig& config = {},
                         jobs::JobSystem* jobs = nullptr) {
    return BattlefieldScenario::startBattlefieldScenario(config, jobs);
}

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
