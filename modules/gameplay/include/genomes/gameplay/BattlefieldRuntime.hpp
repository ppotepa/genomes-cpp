#pragma once

#include <genomes/ballistics/BallisticsWorld.hpp>
#include <genomes/api/Api.hpp>
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
#include <genomes/foundation/PublishedSnapshotExchange.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/gameplay/BattlefieldTypes.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/jobs/SchedulerClient.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#include <genomes/proc/GenerationClient.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/InfantrySimulation.hpp>
#endif
#include <genomes/physics/PhysicsWorld.hpp>
#include <genomes/simulation/EntityStore.hpp>
#include <genomes/simulation/EntityController.hpp>
#include <genomes/simulation/SimulationSnapshot.hpp>
#include <genomes/simulation/SystemGraph.hpp>
#include <genomes/spatial/SpatialGrid.hpp>
#include <genomes/world_core/WorldQuerySnapshot.hpp>
#include <genomes/world/WorldArtifactRevision.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponHandlingSystem.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

struct ResolvedWorldArtifacts;

struct BattlefieldPresentationSnapshot final {
    foundation::SnapshotMetadata metadata{};
    std::vector<infantry::InfantryRenderState> states;
};

using BattlefieldPresentationSnapshotExchange =
    foundation::PublishedSnapshotExchange<BattlefieldPresentationSnapshot>;

// Authoritative battlefield owner.  It owns the ECS, infantry, physics,
// navigation, combat/ballistics pipeline and graph for a complete session.
// BattlefieldScenario is only a compatibility/fixture facade over this type.
class BattlefieldRuntime final : public api::SimulationFacade {
public:
    BattlefieldRuntime(const BattlefieldRuntime&) = delete;
    BattlefieldRuntime& operator=(const BattlefieldRuntime&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldRuntime>,
                                             foundation::Error>
    start(const BattlefieldScenarioConfig& config, jobs::SchedulerClient jobs,
          BattlefieldExecutionMode execution_mode = BattlefieldExecutionMode::Parallel,
          proc::GenerationClient generation = {});
    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldRuntime>,
                                             foundation::Error>
    start(const BattlefieldScenarioConfig& config, jobs::JobSystem& jobs,
          BattlefieldExecutionMode execution_mode = BattlefieldExecutionMode::Parallel,
          proc::ProceduralRuntime* procedural_runtime = nullptr);

    void fixedUpdate(double dt = 1.0 / 60.0) noexcept;
    void fixedUpdate(const simulation::TickContext& context) noexcept;
    [[nodiscard]] bool advance(const simulation::TickContext& context) noexcept override {
        fixedUpdate(context);
        return snapshot_.error.empty();
    }

    [[nodiscard]] api::CommandReceipt submit(api::CommandEnvelope command) override;
    [[nodiscard]] const weapons::WeaponArtifact* weaponArtifact() const noexcept {
        return weapon_artifact_.get();
    }
    [[nodiscard]] bool consumeRestartRequest() noexcept {
        const bool requested = restart_requested_;
        restart_requested_ = false;
        return requested;
    }
    [[nodiscard]] bool consumeWorldRegenerateRequest() noexcept {
        const bool requested = world_regenerate_requested_;
        world_regenerate_requested_ = false;
        return requested;
    }
    [[nodiscard]] api::SnapshotView snapshotView() const noexcept override;
    [[nodiscard]] api::SnapshotView query(api::ApiId query,
                                          const api::EncodedValue& arguments) const override;

    void setSceneEpoch(std::uint64_t epoch) noexcept {
        // Scene epochs identify a runtime lifetime and therefore only move
        // forward. Ignore stale lifecycle notifications rather than allowing
        // old snapshots to be tagged with a newer scene's predecessor.
        const std::uint64_t next_epoch = std::max(scene_epoch_, epoch);
        scene_epoch_ = next_epoch;
        simulation_snapshot_exchange_.rejectBeforeSceneEpoch(next_epoch);
        presentation_snapshot_exchange_.rejectBeforeSceneEpoch(next_epoch);
        if (presentation_snapshot_.metadata.tick != 0U &&
            presentation_snapshot_.metadata.scene_epoch < next_epoch) {
            presentation_snapshot_ = {};
        }
        if (simulation_snapshot_.metadata.tick != 0U &&
            simulation_snapshot_.metadata.scene_epoch < next_epoch) {
            simulation_snapshot_ = {};
            api_snapshot_bytes_.reset();
            api_world_snapshot_values_.reset();
        }
    }

    // Bind the authoritative resolved-world revision to the collision and
    // navigation consumers owned by this runtime. A runtime may be bound
    // once; attempting to switch it to a different world is rejected so a
    // tick cannot observe mixed world revisions.
    [[nodiscard]] bool bindWorldArtifactRevision(
        world::WorldArtifactRevision revision) noexcept;
    [[nodiscard]] bool bindWorldArtifact(
        std::shared_ptr<const ResolvedWorldArtifacts> artifact) noexcept;
    [[nodiscard]] world::WorldArtifactRevision worldArtifactRevision() const noexcept {
        return world_artifact_revision_;
    }

    [[nodiscard]] const BattlefieldScenarioSnapshot& snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] const simulation::SimulationSnapshot& simulationSnapshot() const noexcept {
        return simulation_snapshot_;
    }
    [[nodiscard]] simulation::SimulationSnapshotExchange& simulationSnapshotExchange() noexcept {
        return simulation_snapshot_exchange_;
    }
    [[nodiscard]] const BattlefieldPresentationSnapshot& presentationSnapshot() const noexcept {
        return presentation_snapshot_;
    }
    [[nodiscard]] BattlefieldPresentationSnapshotExchange& presentationSnapshotExchange() noexcept {
        return presentation_snapshot_exchange_;
    }
    [[nodiscard]] std::uint64_t sceneEpoch() const noexcept { return scene_epoch_; }
    [[nodiscard]] bool complete() const noexcept { return snapshot_.complete; }
    [[nodiscard]] BattlefieldRuntimeState state() const noexcept { return state_; }
    [[nodiscard]] const simulation::WorldEcs& ecs() const noexcept { return entities_.ecs(); }
    [[nodiscard]] const combat::TacticalAIProfile& tacticalProfile() const noexcept {
        return tactical_ai_.profile();
    }
    [[nodiscard]] const std::vector<infantry::InfantryRenderState>& renderStates() const noexcept {
        return presentation_snapshot_.states;
    }
    [[nodiscard]] const weapons::WeaponPoseTasks* weaponPoseTasks(
        simulation::EntityId entity) const noexcept;

private:
    struct TargetRuntime final {
        simulation::EntityId entity{};
        infantry::Team team{infantry::Team::Blue};
        destruction::MaterialAssembly assembly{};
        destruction::DamageField damage_field{};
    };

    BattlefieldRuntime(BattlefieldScenarioConfig config, jobs::SchedulerClient jobs,
                       BattlefieldExecutionMode execution_mode,
                       proc::GenerationClient generation);

    [[nodiscard]] foundation::Result<void, foundation::Error> initialize();
    [[nodiscard]] foundation::Result<void, foundation::Error> configureWorldQuery();
    [[nodiscard]] foundation::Result<void, foundation::Error> configureAmmunition();
    [[nodiscard]] foundation::Result<void, foundation::Error> configureGraph();
    void runPerception() noexcept;
    void runDecision() noexcept;
    void queueFire(float fixed_dt_seconds) noexcept;
    void advanceBallistics() noexcept;
    void applyImpactDamage() noexcept;
    void commitSnapshot() noexcept;
    void publishPresentationSnapshot() noexcept;
    void applyApiCommands() noexcept;
    [[nodiscard]] TargetRuntime* target(simulation::EntityId entity) noexcept;
    [[nodiscard]] const TargetRuntime* target(simulation::EntityId entity) const noexcept;
    [[nodiscard]] static bool provideContact(void*, const world_core::QuerySegmentHit&,
                                              ballistics::ContactCandidate&) noexcept;

    BattlefieldScenarioConfig config_{};
    BattlefieldExecutionMode execution_mode_{BattlefieldExecutionMode::Parallel};
    jobs::SchedulerClient jobs_{};
    proc::GenerationClient generation_{};
    simulation::EntityStore entities_;
    physics::SimplePhysicsWorld physics_{};
    std::unique_ptr<navigation::GridNavigationWorld> navigation_;
    std::shared_ptr<const ResolvedWorldArtifacts> world_artifact_;
    std::unique_ptr<infantry::InfantrySimulation> infantry_;
    combat::CombatSystem combat_;
    combat::CombatCommandFlow combat_flow_;
    simulation::SystemGraph graph_;
    simulation::SystemExecutionPlan execution_plan_;
    simulation::CommandBufferSet command_buffers_{};
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
    // Fire requests are consumed in key order so the authoritative ballistic
    // command stream is independent of unordered-container bucket layout.
    std::map<std::uint64_t, weapons::WeaponRuntimeState> weapon_runtime_states_;
    std::unordered_map<std::uint64_t, weapons::WeaponPoseTasks> weapon_pose_tasks_;
    weapons::WeaponHandlingSystem weapon_handling_{};
    const weapons::WeaponDefinition* weapon_definition_{nullptr};
    std::shared_ptr<const weapons::WeaponArtifact> weapon_artifact_;
    weapons::WeaponSpec weapon_spec_{};
    weapons::WeaponId weapon_id_{0};

    world_core::WorldQuerySnapshot query_snapshot_{};
    std::vector<TargetRuntime> targets_;
    ballistics::BallisticsWorld* ballistics_{nullptr};
    std::unique_ptr<ballistics::BallisticsWorld> owned_ballistics_;
    std::unordered_map<std::uint64_t, simulation::EntityId> projectile_sources_;
    std::vector<ballistics::BallisticsContact> ballistic_contacts_;
    combat::DamageBuffer damage_buffer_;
    world::WorldArtifactRevision world_artifact_revision_{0U};

    BattlefieldScenarioSnapshot snapshot_{};
    simulation::SimulationSnapshot simulation_snapshot_{};
    simulation::SimulationSnapshotExchange simulation_snapshot_exchange_{};
    BattlefieldPresentationSnapshot presentation_snapshot_{};
    BattlefieldPresentationSnapshotExchange presentation_snapshot_exchange_{};
    std::shared_ptr<const std::vector<std::uint8_t>> api_snapshot_bytes_;
    std::shared_ptr<const std::map<std::string, std::vector<std::uint8_t>, std::less<>>>
        api_world_snapshot_values_;
    std::uint64_t scene_epoch_{0U};
    api::CommandQueue api_commands_{};
    std::map<std::string, std::vector<std::uint8_t>, std::less<>> api_world_values_;
    bool restart_requested_{false};
    bool world_regenerate_requested_{false};
    BattlefieldRuntimeState state_{BattlefieldRuntimeState::Running};
};

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
