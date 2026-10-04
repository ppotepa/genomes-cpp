#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/gameplay/WorldScenario.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/weapons/WeaponProcedural.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

namespace {

constexpr float kSpawnOffset = 8.0F;
constexpr float kTargetHalfExtent = 0.5F;

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                         foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] simulation::EntityId fromPacked(std::uint64_t packed) noexcept {
    return {static_cast<std::uint32_t>(packed & 0xFFFF'FFFFU),
            static_cast<std::uint32_t>(packed >> 32U)};
}

[[nodiscard]] foundation::Error scenarioError(foundation::ErrorCode code,
                                               const char* message) {
    return {code, message};
}

[[nodiscard]] float sampleWorldGround(const void* context, float x, float z) noexcept {
    const auto* artifact = static_cast<const ResolvedWorldArtifacts*>(context);
    return artifact != nullptr && artifact->terrain != nullptr
               ? artifact->terrain->sampleBilinear(x, z)
               : 0.0F;
}

} // namespace

BattlefieldRuntime::BattlefieldRuntime(BattlefieldScenarioConfig config,
                                         jobs::JobSystem& jobs,
                                         BattlefieldExecutionMode execution_mode,
                                         proc::ProceduralRuntime* procedural_runtime)
    : config_{config}, execution_mode_{execution_mode}, jobs_{&jobs},
      procedural_runtime_{procedural_runtime}, combat_{entities_},
      tactical_ai_{config.tactical_ai_profile} {}

foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>
BattlefieldRuntime::start(const BattlefieldScenarioConfig& config,
                           jobs::JobSystem& jobs,
                           BattlefieldExecutionMode execution_mode,
                           proc::ProceduralRuntime* procedural_runtime) {
    if (!config.valid()) {
        return foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidArgument,
                          "invalid 25x25 battlefield scenario configuration"));
    }
    auto scenario = std::unique_ptr<BattlefieldRuntime>(
        new BattlefieldRuntime(config, jobs, execution_mode, procedural_runtime));
    const auto initialized = scenario->initialize();
    if (!initialized) {
        return foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>::failure(
            initialized.error());
    }
    return foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>::success(
        std::move(scenario));
}

api::CommandReceipt BattlefieldRuntime::submit(api::CommandEnvelope command) {
    return api_commands_.enqueue(std::move(command), simulation_tick_);
}

api::SnapshotView BattlefieldRuntime::snapshotView() const noexcept {
    return {simulation_snapshot_.metadata.tick == 0U
                ? foundation::SimulationTick{}
                : foundation::SimulationTick{simulation_snapshot_.metadata.tick},
            simulation_snapshot_.metadata.scene_epoch,
            simulation_snapshot_.semantic_hash,
            api_snapshot_bytes_ ? std::span<const std::uint8_t>{*api_snapshot_bytes_}
                                : std::span<const std::uint8_t>{},
            api_snapshot_bytes_};
}

api::SnapshotView BattlefieldRuntime::query(api::ApiId query_id,
                                            const api::EncodedValue& arguments) const {
    if (query_id == foundation::stable_id("world.snapshot")) {
        return snapshotView();
    }
    if (query_id == foundation::stable_id("world.query") &&
        arguments.type == api::ValueType::String && arguments.valid()) {
        const auto key = arguments.asString();
        if (!key || !api_world_snapshot_values_) return {};
        const auto found = api_world_snapshot_values_->find(*key);
        if (found == api_world_snapshot_values_->end()) return {};
        auto result = std::make_shared<const std::vector<std::uint8_t>>(found->second);
        return {foundation::SimulationTick{simulation_snapshot_.metadata.tick},
                simulation_snapshot_.metadata.scene_epoch,
                simulation_snapshot_.semantic_hash,
                std::span<const std::uint8_t>{*result}, std::move(result)};
    }
    return {};
}

foundation::Result<void, foundation::Error> BattlefieldRuntime::initialize() {
    navigation_ = std::make_unique<navigation::GridNavigationWorld>(
        navigation::NavGridSpec{4U, 4U, 6.25F, {-12.5F, 0.0F, -12.5F}});
    if (!navigation_ || !navigation_->valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::Internal,
                          "battlefield navigation setup failed"));
    }
    infantry_ = std::make_unique<infantry::InfantrySimulation>(
        entities_, navigation_.get(), &physics_,
        execution_mode_ == BattlefieldExecutionMode::Parallel ? jobs_ : nullptr,
        true);
    const infantry::InfantryGenome genome{1.75F, 1.75, 3.0F, 24.0F, 24.0F, 100.0F, 0U};
    const auto blue = infantry_->spawn({infantry::Team::Blue, {0.0F, 0.0F, -kSpawnOffset},
                                        genome, infantry::SquadKey{infantry::Team::Blue, 1U},
                                        weapons::weapon_id("carbine")});
    const auto red = infantry_->spawn({infantry::Team::Red, {0.0F, 0.0F, kSpawnOffset},
                                       genome, infantry::SquadKey{infantry::Team::Red, 2U},
                                       weapons::weapon_id("carbine")});
    if (!blue || !red) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::Internal, "battlefield infantry spawn failed"));
    }
    const simulation::EntityId blue_id = blue.value();
    const simulation::EntityId red_id = red.value();
    // The two actors face one another on the 25x25 lane, so perception and
    // tactical alignment can be observed on the first AI cadence.
    *entities_.heading(blue_id) = 0.0F;
    *entities_.heading(red_id) = 3.14159265358979323846F;
    teams_[blue_id.packed()] = 0U;
    teams_[red_id.packed()] = 1U;
    ai_state_by_entity_.emplace(blue_id.packed(), combat::AIState{});
    ai_state_by_entity_.emplace(red_id.packed(), combat::AIState{});

    std::vector<simulation::EntityId> blue_members{blue_id};
    std::vector<simulation::EntityId> red_members{red_id};
    if (!squads_.create(1U, blue_members) || !squads_.create(2U, red_members)) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::Internal, "battlefield squad setup failed"));
    }

    weapon_id_ = weapons::weapon_id("carbine");
    const weapons::WeaponDefinition* weapon = weapons::WeaponCatalog::find(weapon_id_);
    if (weapon == nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::NotFound, "carbine missing from weapon catalog"));
    }
    weapon_definition_ = weapon;
    weapon_spec_ = {weapon->rounds_per_second, weapon->muzzle_velocity_mps, weapon->damage,
                    weapon->range_m, 30U};
    const weapons::WeaponVariant weapon_variant{
        proc::Seed(foundation::stableHashCombine(config_.seed, 0xB17U)), 1.0F, 0.0F, 0U};
    foundation::Result<weapons::WeaponArtifact, foundation::Error> artifact =
        weapons::WeaponGeometryGenerator::build(*weapon, weapon_variant);
    if (procedural_runtime_ != nullptr &&
        procedural_runtime_->registry().find(proc::generatorId("weapons.artifact")) != nullptr) {
        proc::GenerationRequest<weapons::WeaponGenerationRequest, weapons::WeaponArtifact>
            generation;
        generation.generator = proc::generatorId("weapons.artifact");
        generation.input = std::make_shared<const weapons::WeaponGenerationRequest>(
            weapons::WeaponGenerationRequest{*weapon, weapon_variant});
        generation.seed_path = proc::SeedPath(weapon_variant.seed);
        generation.options.input_hash = foundation::stableHashCombine(
            foundation::stable_id("weapon.carbine"), weapon_variant.seed);
        generation.options.retained_bytes = sizeof(weapons::WeaponArtifact);
        auto ticket = procedural_runtime_->request(std::move(generation));
        ticket.wait();
        const auto generated = ticket.artifact();
        if (generated) {
            artifact = foundation::Result<weapons::WeaponArtifact, foundation::Error>::success(
                *generated);
        } else {
            artifact = foundation::Result<weapons::WeaponArtifact, foundation::Error>::failure(
                ticket.error());
        }
    }
    if (!artifact) {
        return foundation::Result<void, foundation::Error>::failure(artifact.error());
    }
    weapon_artifact_ = std::make_shared<const weapons::WeaponArtifact>(artifact.value());
    const auto make_runtime_state = [this](simulation::EntityId entity) {
        weapons::WeaponRuntimeState state{};
        state.selected_weapon = weapon_id_;
        state.active_weapon = weapon_id_;
        state.handling = weapons::WeaponHandlingState::Held;
        state.readiness = 1.0F;
        state.requested_readiness = 1.0F;
        weapon_runtime_states_[entity.packed()] = state;
        weapon_pose_tasks_[entity.packed()] = {};
    };
    make_runtime_state(blue_id);
    make_runtime_state(red_id);

    const auto query_result = configureWorldQuery();
    if (!query_result) {
        return query_result;
    }
    const auto ammunition_result = configureAmmunition();
    if (!ammunition_result) {
        return ammunition_result;
    }
    const auto graph_result = configureGraph();
    if (!graph_result) {
        return graph_result;
    }

    snapshot_.map_size_m = config_.map_size_m;
    snapshot_.spawned = 2U;
    snapshot_.ecs_entities = entities_.ecs().entityCount();
    snapshot_.alive_units = 2U;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> BattlefieldRuntime::configureWorldQuery() {
    const world_core::WorldId world_id{foundation::stableHashCombine(config_.seed, 25U)};
    const world_core::WorldCoordinateConfig coordinates{static_cast<double>(config_.map_size_m)};
    const world_core::RegionCoord coordinate{};
    const world_core::RegionId region_id = world_core::regionId(world_id, coordinate);
    world_core::QueryRegion region{};
    region.coordinate = coordinate;
    region.id = region_id;
    region.revision = 1U;
    region.resident = true;

    entities_.forEachLive([&](simulation::EntityId entity) {
        const foundation::Vec3* position = entities_.position(entity);
        if (position == nullptr) {
            return;
        }
        region.candidates.push_back(
            {entity.packed(),
             {{position->x - kTargetHalfExtent, position->y - 1.0F,
               position->z - kTargetHalfExtent},
              {position->x + kTargetHalfExtent, position->y + 1.0F,
               position->z + kTargetHalfExtent}},
             world_core::QuerySourceKind::Static, region_id, 1U});
    });
    const auto snapshot = world_core::WorldQuerySnapshot::create(world_id, coordinates, {region});
    if (!snapshot) {
        return foundation::Result<void, foundation::Error>::failure(snapshot.error());
    }
    query_snapshot_ = std::move(snapshot.value());

    targets_.reserve(2U);
    entities_.forEachLive([this](simulation::EntityId entity) {
        const auto team = teams_.find(entity.packed());
        const foundation::Vec3* position = entities_.position(entity);
        if (team == teams_.end() || position == nullptr) {
            return;
        }
        const foundation::Vec3 outward = team->second == 0U
                                             ? foundation::Vec3{0.0F, 0.0F, 1.0F}
                                             : foundation::Vec3{0.0F, 0.0F, -1.0F};
        destruction::MaterialFrame frame{};
        frame.origin = {position->x + outward.x * kTargetHalfExtent,
                        position->y + outward.y * kTargetHalfExtent,
                        position->z + outward.z * kTargetHalfExtent};
        frame.normal = outward;
        frame.tangent = {1.0F, 0.0F, 0.0F};
        frame.bitangent = {0.0F, 1.0F, 0.0F};
        const destruction::MaterialCatalog materials = destruction::MaterialCatalog::makeDefault();
        const auto assembly = destruction::MaterialAssembly::create(
            proc::Seed(config_.seed ^ entity.packed()), frame,
            {{destruction::MaterialId::fromName("tissue"),
              destruction::PhysicalSolidId::fromName("infantry.tissue"), 0.0F, 1.0F,
              destruction::LayerKind::Solid}},
            materials);
        if (!assembly) {
            return;
        }
        const auto damage_field = destruction::DamageField::create(
            {proc::Seed(config_.seed ^ entity.packed()), {-1.0F, -1.0F, -1.0F},
             {1.0F, 1.0F, 1.0F}, {0.5F, 0.5F, 0.5F}, 4U, 4U, 4U, 32U});
        if (!damage_field) {
            return;
        }
        targets_.push_back({entity,
                            team->second == 0U ? infantry::Team::Blue : infantry::Team::Red,
                            std::move(assembly.value()), std::move(damage_field.value())});
    });
    if (targets_.size() != 2U) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::Internal, "battlefield target setup failed"));
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> BattlefieldRuntime::configureAmmunition() {
    ballistics::AmmunitionCatalog catalog;
    ballistics::AmmunitionStrategy strategy{};
    strategy.id = ballistics::strategy_id("battlefield.556.fmj");
    strategy.caliber_id = ballistics::caliber_id("battlefield.556");
    strategy.variant_id = ballistics::variant_id("battlefield.standard");
    strategy.construction = ballistics::ConstructionKind::FullMetalJacket;
    strategy.drag_coefficient = 0.3F;
    strategy.contact_work_scale = 1.0F;
    strategy.ricochet_threshold = 0.35F;
    const ballistics::AmmunitionDefinition definition{
        ballistics::ammunition_id("ammo_556"), strategy.id, strategy.caliber_id,
        strategy.variant_id, 0.004F, 0.00556F, 0.00556F, weapon_spec_.muzzle_velocity, 0.25F,
        1U, "FAST_VIABILITY_AGENT_PLAN", 0.0F};
    if (!catalog.add(definition, strategy) || !catalog.freeze()) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield ammunition catalog setup failed"));
    }
    owned_ballistics_ = std::make_unique<ballistics::BallisticsWorld>(
        std::move(catalog), &query_snapshot_,
        ballistics::BallisticsProfile{64U, 256U, config_.fixed_step_seconds, 2.0F, 2.0F},
        ballistics::FlightEnvironment{}, destruction::MaterialCatalog::makeDefault(),
        &BattlefieldRuntime::provideContact, this);
    ballistics_ = owned_ballistics_.get();
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> BattlefieldRuntime::configureGraph() {
    if (!tactical_ai_.registerDefaults()) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState, "battlefield AI setup failed"));
    }
    const auto add_system = [this](simulation::SystemDescriptor descriptor) {
        const auto added = graph_.add(std::move(descriptor));
        if (!added) {
            return false;
        }
        return true;
    };
    constexpr simulation::CadencePolicy every_tick{simulation::CadenceKind::EveryTick};
    const bool inline_execution = execution_mode_ == BattlefieldExecutionMode::Inline;

    simulation::SystemDescriptor sense{};
    sense.id = foundation::stable_id("battlefield.sense");
    sense.phase = simulation::SystemPhase::Sense;
    sense.access.reads = {
        foundation::stable_id("component.entity.position"),
        foundation::stable_id("component.entity.heading"),
        foundation::stable_id("component.entity.flags")};
    sense.access.resource_writes = {foundation::stable_id("battlefield.perception")};
    sense.cadence = every_tick;
    // These callbacks are simulation-owned and do not require OS/main-thread
    // affinity.  The execution plan runs them on the central worker lane;
    // graph dependencies provide the ordering and the commit phase remains
    // the sole authoritative state publication point.
    sense.main_thread_only = inline_execution;
    sense.callback = [this](simulation::SystemContext&) {
        if (snapshot_.error.empty()) {
            runPerception();
        }
    };
    if (!add_system(std::move(sense))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState, "battlefield sense graph setup failed"));
    }

    simulation::SystemDescriptor decide{};
    decide.id = foundation::stable_id("battlefield.decide");
    decide.phase = simulation::SystemPhase::Decide;
    decide.access.reads = {foundation::stable_id("component.entity.position"),
                           foundation::stable_id("component.entity.heading")};
    decide.access.writes = {foundation::stable_id("component.ai.state")};
    decide.access.resource_reads = {foundation::stable_id("battlefield.perception")};
    decide.access.resource_writes = {foundation::stable_id("battlefield.intent")};
    decide.cadence = every_tick;
    // AIJobPipeline uses bounded nested bulk work. Worker waits are
    // cooperative and execute runnable child batches, so decision evaluation
    // no longer needs artificial main-lane affinity.
    decide.main_thread_only = inline_execution;
    decide.after = {foundation::stable_id("battlefield.sense")};
    decide.callback = [this](simulation::SystemContext&) {
        if (snapshot_.error.empty()) {
            runDecision();
        }
    };
    if (!add_system(std::move(decide))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState, "battlefield decision graph setup failed"));
    }

    simulation::SystemDescriptor navigate{};
    navigate.id = foundation::stable_id("battlefield.navigate");
    navigate.phase = simulation::SystemPhase::Navigate;
    navigate.access.reads = {foundation::stable_id("component.entity.health"),
                             foundation::stable_id("component.entity.position")};
    navigate.access.writes = {foundation::stable_id("component.entity.position"),
                              foundation::stable_id("component.entity.velocity")};
    navigate.access.resource_writes = {foundation::stable_id("battlefield.infantry")};
    navigate.cadence = every_tick;
    // InfantrySimulation performs bounded batch execution through the same
    // scheduler. Cooperative worker waits keep the parent live while child
    // ranges execute, including on a one-worker configuration.
    navigate.main_thread_only = inline_execution;
    navigate.after = {foundation::stable_id("battlefield.decide")};
    navigate.callback = [this](simulation::SystemContext& context) {
        if (snapshot_.error.empty()) {
            infantry_->fixedUpdate(context.fixed_dt, context.tick);
        }
    };
    if (!add_system(std::move(navigate))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield navigation graph setup failed"));
    }

    // Keep the movement command boundary explicit even though the current
    // infantry adapter computes navigation and movement intents together. A
    // later split can move the route solver behind Navigate without changing
    // the authoritative phase contract or physics ownership.
    simulation::SystemDescriptor move_intent{};
    move_intent.id = foundation::stable_id("battlefield.move-intent");
    move_intent.phase = simulation::SystemPhase::MoveIntent;
    move_intent.access.reads = {foundation::stable_id("component.entity.position"),
                                foundation::stable_id("component.entity.velocity")};
    move_intent.access.resource_reads = {foundation::stable_id("battlefield.infantry")};
    move_intent.access.resource_writes = {foundation::stable_id("battlefield.physics")};
    move_intent.cadence = every_tick;
    move_intent.main_thread_only = inline_execution;
    move_intent.after = {foundation::stable_id("battlefield.navigate")};
    move_intent.callback = [](simulation::SystemContext&) {
        // InfantrySimulation::fixedUpdate has already emitted the command
        // buffer in Navigate; this phase is the typed hand-off boundary.
    };
    if (!add_system(std::move(move_intent))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield move-intent graph setup failed"));
    }

    simulation::SystemDescriptor physics_commands{};
    physics_commands.id = foundation::stable_id("battlefield.physics-commands");
    physics_commands.phase = simulation::SystemPhase::PhysicsCommands;
    physics_commands.access.reads = {foundation::stable_id("component.entity.velocity")};
    physics_commands.access.resource_reads = {foundation::stable_id("battlefield.infantry")};
    physics_commands.access.resource_writes = {foundation::stable_id("battlefield.physics")};
    physics_commands.cadence = every_tick;
    physics_commands.main_thread_only = inline_execution;
    physics_commands.after = {foundation::stable_id("battlefield.move-intent")};
    physics_commands.callback = [this](simulation::SystemContext&) {
        if (snapshot_.error.empty() && infantry_) {
            infantry_->applyPhysicsCommands();
        }
    };
    if (!add_system(std::move(physics_commands))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield physics-command graph setup failed"));
    }

    simulation::SystemDescriptor physics_step{};
    physics_step.id = foundation::stable_id("battlefield.physics-step");
    physics_step.phase = simulation::SystemPhase::PhysicsStep;
    physics_step.access.reads = {foundation::stable_id("component.entity.velocity")};
    physics_step.access.writes = {foundation::stable_id("component.entity.position"),
                                  foundation::stable_id("component.entity.velocity")};
    physics_step.access.resource_reads = {foundation::stable_id("battlefield.infantry")};
    physics_step.access.resource_writes = {foundation::stable_id("battlefield.physics")};
    physics_step.cadence = every_tick;
    physics_step.main_thread_only = inline_execution;
    physics_step.after = {foundation::stable_id("battlefield.physics-commands")};
    physics_step.callback = [this](simulation::SystemContext& context) {
        if (snapshot_.error.empty() && infantry_) {
            // The runtime is the sole owner of this world step. Infantry only
            // submits commands and consumes the post-step snapshot; keeping
            // the actual step here prevents a second physics owner from
            // entering the authoritative pipeline.
            physics_.step(static_cast<float>(context.fixed_dt));
        }
    };
    if (!add_system(std::move(physics_step))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield physics graph setup failed"));
    }

    simulation::SystemDescriptor combat{};
    combat.id = foundation::stable_id("battlefield.combat-ballistics");
    combat.phase = simulation::SystemPhase::CombatBallistics;
    combat.access.reads = {foundation::stable_id("component.entity.position"),
                           foundation::stable_id("component.entity.health")};
    combat.access.resource_reads = {foundation::stable_id("battlefield.intent")};
    combat.access.resource_writes = {foundation::stable_id("battlefield.projectiles")};
    combat.cadence = every_tick;
    combat.main_thread_only = inline_execution;
    combat.after = {foundation::stable_id("battlefield.physics-step")};
    combat.callback = [this](simulation::SystemContext& context) {
        if (snapshot_.error.empty()) {
            queueFire(static_cast<float>(context.fixed_dt));
            advanceBallistics();
        }
    };
    if (!add_system(std::move(combat))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield ballistics graph setup failed"));
    }

    simulation::SystemDescriptor damage{};
    damage.id = foundation::stable_id("battlefield.damage-destruction");
    damage.phase = simulation::SystemPhase::DamageDestruction;
    damage.access.reads = {foundation::stable_id("component.entity.position")};
    damage.access.writes = {foundation::stable_id("component.entity.health"),
                            foundation::stable_id("component.entity.flags")};
    damage.access.resource_reads = {foundation::stable_id("battlefield.projectiles")};
    damage.access.resource_writes = {foundation::stable_id("battlefield.health")};
    damage.cadence = every_tick;
    damage.main_thread_only = inline_execution;
    damage.after = {foundation::stable_id("battlefield.combat-ballistics")};
    damage.callback = [this](simulation::SystemContext&) {
        if (snapshot_.error.empty()) {
            applyImpactDamage();
        }
    };
    if (!add_system(std::move(damage))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield damage graph setup failed"));
    }

    simulation::SystemDescriptor commit{};
    commit.id = foundation::stable_id("battlefield.commit");
    commit.phase = simulation::SystemPhase::Commit;
    commit.access.reads = {foundation::stable_id("component.entity.health"),
                           foundation::stable_id("component.entity.flags")};
    commit.access.resource_reads = {foundation::stable_id("battlefield.projectiles"),
                                    foundation::stable_id("battlefield.health")};
    commit.access.resource_writes = {foundation::stable_id("battlefield.snapshot")};
    commit.cadence = every_tick;
    commit.main_thread_only = inline_execution;
    commit.after = {foundation::stable_id("battlefield.damage-destruction")};
    commit.callback = [](simulation::SystemContext&) {
        // Authoritative ECS writes are applied by CommandCommitter after the
        // execution plan drains. Snapshot publication therefore happens only
        // after that deterministic commit boundary in fixedUpdate().
    };
    if (!add_system(std::move(commit))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield commit graph setup failed"));
    }

    simulation::SystemDescriptor presentation{};
    presentation.id = foundation::stable_id("battlefield.presentation-extract");
    presentation.phase = simulation::SystemPhase::PresentationExtract;
    presentation.access.reads = {foundation::stable_id("component.entity.position"),
                                 foundation::stable_id("component.entity.heading")};
    presentation.access.resource_reads = {foundation::stable_id("battlefield.snapshot")};
    presentation.access.resource_writes = {
        foundation::stable_id("battlefield.presentation")};
    presentation.cadence = every_tick;
    presentation.main_thread_only = inline_execution;
    presentation.after = {foundation::stable_id("battlefield.commit")};
    presentation.callback = [](simulation::SystemContext&) {
        // Presentation extraction is deliberately delayed until after the
        // authoritative command commit in fixedUpdate().
    };
    if (!add_system(std::move(presentation))) {
        return foundation::Result<void, foundation::Error>::failure(
            scenarioError(foundation::ErrorCode::InvalidState,
                          "battlefield presentation graph setup failed"));
    }
    const auto compiled = graph_.compile();
    if (!compiled) {
        return foundation::Result<void, foundation::Error>::failure(compiled.error());
    }
    execution_plan_ = graph_.executionPlan();
    return foundation::Result<void, foundation::Error>::success();
}

bool BattlefieldRuntime::bindWorldArtifactRevision(
    world::WorldArtifactRevision revision) noexcept {
    if (revision == 0U || navigation_ == nullptr) {
        return false;
    }
    if (world_artifact_revision_ != 0U && world_artifact_revision_ != revision) {
        return false;
    }
    physics_.bindWorldRevision(revision);
    navigation_->bindWorldRevision(revision);
    world_artifact_revision_ = revision;
    return true;
}

bool BattlefieldRuntime::bindWorldArtifact(
    std::shared_ptr<const ResolvedWorldArtifacts> artifact) noexcept {
    if (artifact == nullptr || !artifact->valid() || navigation_ == nullptr ||
        !bindWorldArtifactRevision(artifact->revision)) {
        return false;
    }

    world_artifact_ = std::move(artifact);
    physics_.setGroundHeightQuery({world_artifact_.get(), sampleWorldGround});

    const navigation::NavGridSpec& navigation_spec = navigation_->spec();
    for (std::uint32_t z = 0U; z < navigation_spec.height; ++z) {
        for (std::uint32_t x = 0U; x < navigation_spec.width; ++x) {
            const float world_x = navigation_spec.origin.x +
                                  (static_cast<float>(x) + 0.5F) * navigation_spec.cell_size;
            const float world_z = navigation_spec.origin.z +
                                  (static_cast<float>(z) + 0.5F) * navigation_spec.cell_size;
            const LandscapeSample landscape = world_artifact_->sampleLandscape(world_x, world_z);
            const foundation::Vec3 normal = world_artifact_->terrain->normal(world_x, world_z);
            const bool too_steep = normal.y < 0.70710678F;
            if (!navigation_->setBlocked(x, z, !landscape.traversable() || too_steep)) {
                return false;
            }
        }
    }

    entities_.forEachLive([this](simulation::EntityId entity) {
        foundation::Vec3* position = entities_.position(entity);
        if (position == nullptr) return;
        position->y = world_artifact_->sampleLandscape(position->x, position->z).ground_y;
    });
    infantry_->extractPresentation();
    publishPresentationSnapshot();
    return true;
}

void BattlefieldRuntime::fixedUpdate(double dt) noexcept {
    if (!std::isfinite(dt) || dt <= 0.0) {
        return;
    }
    simulation::TickContext context{};
    context.tick = simulation_tick_;
    context.tick.increment();
    context.fixed_dt_seconds = dt;
    context.tick_rate_hz = simulation::SessionSimulationTickRateHz;
    fixedUpdate(context);
}

void BattlefieldRuntime::fixedUpdate(const simulation::TickContext& context) noexcept {
    if (state_ != BattlefieldRuntimeState::Running || snapshot_.complete ||
        !std::isfinite(context.fixed_dt_seconds) ||
        context.fixed_dt_seconds <= 0.0 || context.tick_rate_hz == 0U ||
        context.tick.value <= simulation_tick_.value) {
        return;
    }
    const BattlefieldScenarioSnapshot last_good_snapshot = snapshot_;
    const foundation::SimulationTick last_good_tick = simulation_tick_;
    simulation_tick_ = context.tick;
    snapshot_.tick = context.tick.value;
    applyApiCommands();
    const auto result = execution_plan_.run(
        simulation_tick_, context.fixed_dt_seconds,
        jobs_,
        &command_buffers_);
    if (!result || !snapshot_.error.empty()) {
        const std::string error = result ? snapshot_.error : std::string{result.error().message};
        snapshot_ = last_good_snapshot;
        snapshot_.error = error;
        snapshot_.complete = true;
        state_ = BattlefieldRuntimeState::Failed;
        simulation_tick_ = last_good_tick;
        return;
    }
    simulation::CommandCommitter committer;
    const auto committed = committer.commit(entities_.ecs(), command_buffers_.buffers());
    if (!committed) {
        snapshot_ = last_good_snapshot;
        snapshot_.error = committed.error().message;
        snapshot_.complete = true;
        state_ = BattlefieldRuntimeState::Failed;
        simulation_tick_ = last_good_tick;
        return;
    }
    // Publish consumer-shaped state only after all worker command buffers have
    // been applied to the authoritative ECS.
    commitSnapshot();
    if (infantry_ != nullptr && snapshot_.error.empty()) {
        infantry_->syncPhysicsState();
        infantry_->extractPresentation();
        publishPresentationSnapshot();
    }
    if (snapshot_.complete) {
        state_ = BattlefieldRuntimeState::Completed;
    }
}

void BattlefieldRuntime::applyApiCommands() noexcept {
    const auto commands = api_commands_.take(simulation_tick_);
    for (const api::CommandEnvelope& command : commands) {
        if (command.module == foundation::stable_id("module.infantry") &&
            command.verb == foundation::stable_id("battlefield.restart")) {
            restart_requested_ = true;
            continue;
        }
        if (command.module == foundation::stable_id("module.world") &&
            command.verb == foundation::stable_id("world.regenerate")) {
            world_regenerate_requested_ = true;
            continue;
        }
        if (command.module == foundation::stable_id("module.world") &&
            command.verb == foundation::stable_id("world.set")) {
            const auto values = command.payload.asTuple();
            if (!values || values->size() != 2U || (*values)[0].type != api::ValueType::String ||
                (*values)[1].type != api::ValueType::Bytes) {
                continue;
            }
            const auto key = (*values)[0].asString();
            if (!key) continue;
            api_world_values_[std::string(*key)] = (*values)[1].bytes;
            continue;
        }
        if (command.module != foundation::stable_id("module.infantry") ||
            command.verb != foundation::stable_id("units.issue") ||
            command.payload.type != api::ValueType::Bytes ||
            command.payload.bytes.empty() ||
            infantry_ == nullptr) {
            continue;
        }
        const auto decoded = simulation::decodeEntityOrder(command.payload.bytes);
        if (!decoded) continue;
        simulation::EntityOrder order = *decoded;
        order.source = command.source;
        order.priority = command.priority;
        order.issued_tick = simulation_tick_.value;
        order.expires_tick = std::max(order.expires_tick, simulation_tick_.value);
        (void)infantry_->setOrder(order);
    }
}

void BattlefieldRuntime::publishPresentationSnapshot() noexcept {
    BattlefieldPresentationSnapshot candidate{};
    candidate.metadata.tick = simulation_tick_.value;
    candidate.metadata.scene_epoch = scene_epoch_;
    candidate.metadata.revision = simulation_tick_.value;
    if (infantry_ == nullptr) {
        candidate.states.clear();
        return;
    }
    candidate.states = infantry_->renderStates();
    if (auto write = presentation_snapshot_exchange_.acquireWrite(); write) {
        write.value().snapshot() = candidate;
        if (const auto published = presentation_snapshot_exchange_.publish(
                std::move(write.value())); published) {
            presentation_snapshot_ = std::move(candidate);
            presentation_snapshot_.metadata.generation =
                presentation_snapshot_exchange_.publishedSerial();
        }
    }
}

void BattlefieldRuntime::commitSnapshot() noexcept {
    snapshot_.ecs_entities = entities_.ecs().entityCount();
    snapshot_.active_projectiles = ballistics_ == nullptr ? 0U : ballistics_->activeCount();
    snapshot_.physics_steps = physics_.stepCount();
    snapshot_.alive_units = 0U;
    entities_.forEachLive([this](simulation::EntityId entity) {
        const std::uint32_t* flags = entities_.flags(entity);
        if (flags != nullptr && (*flags & simulation::EntityAlive) != 0U) {
            ++snapshot_.alive_units;
        }
    });
    snapshot_.complete = snapshot_.deaths > 0U || simulation_tick_.value >= config_.max_ticks;

    // Publish only consumer-shaped immutable state. The authoritative ECS
    // remains private to the runtime, and a saturated exchange may skip this
    // presentation snapshot without invalidating the committed tick.
    if (auto write = simulation_snapshot_exchange_.acquireWrite(); write) {
        auto& published = write.value().snapshot();
        published.metadata.tick = simulation_tick_.value;
        published.metadata.scene_epoch = scene_epoch_;
        published.metadata.revision = simulation_tick_.value;
        published.semantic_hash = foundation::stableHashU64(simulation_tick_.value);
        published.entities.reserve(entities_.ecs().entityCount());
        entities_.forEachLive([this, &published](simulation::EntityId entity) {
            const auto* position = entities_.position(entity);
            const auto* velocity = entities_.velocity(entity);
            const auto* heading = entities_.heading(entity);
            const auto* flags = entities_.flags(entity);
            if (position == nullptr || velocity == nullptr || heading == nullptr ||
                flags == nullptr) {
                return;
            }
            published.entities.push_back({entity, *position, *velocity, *heading, *flags});
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, entity.packed());
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(position->x));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(position->y));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(position->z));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(velocity->x));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(velocity->y));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(velocity->z));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashFloat(*heading));
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, *flags);
        });
        for (const auto& [key, value] : api_world_values_) {
            published.semantic_hash = foundation::stableHashCombine(
                published.semantic_hash, foundation::stableHashString(key));
            for (const std::uint8_t byte : value) {
                published.semantic_hash = foundation::stableHashCombine(
                    published.semantic_hash, byte);
            }
        }
        if (const auto published_result = simulation_snapshot_exchange_.publish(
                std::move(write.value())); published_result) {
            simulation_snapshot_ = published;
            simulation_snapshot_.metadata.generation =
                simulation_snapshot_exchange_.publishedSerial();
            std::vector<api::SnapshotEntity> wire;
            wire.reserve(simulation_snapshot_.entities.size());
            for (const auto& entity : simulation_snapshot_.entities) {
                wire.push_back({entity.entity.packed(), entity.position.x, entity.position.y,
                                entity.position.z, entity.velocity.x, entity.velocity.y,
                                entity.velocity.z, entity.heading_radians, entity.flags});
            }
            api_snapshot_bytes_ = std::make_shared<const std::vector<std::uint8_t>>(
                api::encodeSnapshot(foundation::SimulationTick{simulation_snapshot_.metadata.tick},
                                    simulation_snapshot_.metadata.scene_epoch,
                                    simulation_snapshot_.semantic_hash, wire));
            api_world_snapshot_values_ = std::make_shared<const std::map<
                std::string, std::vector<std::uint8_t>, std::less<>>>(api_world_values_);
        }
    }
}

void BattlefieldRuntime::runPerception() noexcept {
    ++snapshot_.perceived;
    (void)squads_.deliver(simulation_tick_);
    perception_agents_.clear();
    entities_.forEachLive([this](simulation::EntityId entity) {
        const foundation::Vec3* position = entities_.position(entity);
        const float* heading = entities_.heading(entity);
        const std::uint32_t* flags = entities_.flags(entity);
        const auto team = teams_.find(entity.packed());
        if (position == nullptr || heading == nullptr || flags == nullptr || team == teams_.end() ||
            (*flags & simulation::EntityAlive) == 0U) {
            return;
        }
        perception_agents_.push_back({entity, *position, *heading, team->second,
                                      0xFFFF'FFFFU, true});
    });
    std::sort(perception_agents_.begin(), perception_agents_.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });
    const auto rebuilt = perception_.rebuild(perception_agents_, perception_grid_);
    if (!rebuilt) {
        snapshot_.error = rebuilt.error().message;
        return;
    }
    visible_targets_.clear();
    visible_targets_.resize(perception_agents_.size());
    const combat::PerceptionProfile profile{24.0F, 3.14159265358979323846F, 8U,
                                            0xFFFF'FFFFU};
    for (std::size_t index = 0; index < perception_agents_.size(); ++index) {
        const auto candidates = perception_.query(perception_agents_[index], profile, perception_grid_);
        if (!candidates) {
            continue;
        }
        std::vector<combat::LOSRequest> requests;
        requests.reserve(candidates.value().size());
        for (const combat::PerceptionCandidate& candidate : candidates.value()) {
            const foundation::Vec3* observer_position = entities_.position(perception_agents_[index].id);
            const foundation::Vec3* target_position = entities_.position(candidate.target);
            if (observer_position == nullptr || target_position == nullptr) {
                continue;
            }
            requests.push_back({perception_agents_[index].id.packed(), candidate.target.packed(),
                                *observer_position, *target_position, 0xFFFF'FFFFU});
        }
        const auto los = combat::LineOfSight::query(query_snapshot_, requests);
        if (!los) {
            continue;
        }
        for (std::size_t candidate_index = 0; candidate_index < requests.size(); ++candidate_index) {
            const auto target_entity = fromPacked(requests[candidate_index].target_id);
            const foundation::Vec3* observer_position = entities_.position(perception_agents_[index].id);
            const foundation::Vec3* target_position = entities_.position(target_entity);
            if (observer_position == nullptr || target_position == nullptr) {
                continue;
            }
            const foundation::Vec3 delta = subtract(*target_position, *observer_position);
            const float distance_squared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            visible_targets_[index].push_back(
                {target_entity, *target_position, distance_squared, los.value()[candidate_index].visible});
            if (los.value()[candidate_index].visible) {
                const auto team = teams_.find(perception_agents_[index].id.packed());
                if (team != teams_.end()) {
                    (void)squads_.schedule({perception_agents_[index].id,
                                            team->second + 1U,
                                            target_entity,
                                            *target_position,
                                            simulation_tick_,
                                            {simulation_tick_.value + 3U},
                                            60U,
                                            0U,
                                            1.0F,
                                            24.0F,
                                            combat::TacticalSignalType::Contact,
                                            combat::TacticalSignalChannel::Radio,
                                            0U});
                }
            }
        }
    }
}

void BattlefieldRuntime::runDecision() noexcept {
    ai_entities_.clear();
    ai_states_.clear();
    ai_entities_.reserve(perception_agents_.size());
    ai_states_.reserve(perception_agents_.size());
    for (std::size_t index = 0; index < perception_agents_.size(); ++index) {
        const simulation::EntityId entity = perception_agents_[index].id;
        combat::AIState& state = ai_state_by_entity_[entity.packed()];
        ai_states_.push_back(state);
        ai_entities_.push_back({entity,
                                perception_agents_[index].position,
                                perception_agents_[index].heading_radians,
                                weapon_id_,
                                visible_targets_[index],
                                &ai_states_.back()});
    }
    const combat::AIJobPipelineConfig config{
        2U, execution_mode_ == BattlefieldExecutionMode::Parallel, {}, {}, {}};
    const auto evaluated = ai_pipeline_.evaluate(*jobs_, tactical_ai_, ai_entities_,
                                                 simulation_tick_, config);
    if (!evaluated) {
        snapshot_.error = evaluated.error().message;
        intents_.clear();
        return;
    }
    intents_ = std::move(evaluated.value());
    snapshot_.intents += intents_.size();
    for (const combat::AIIntent& intent : intents_) {
        if (!intent.self.isValid()) continue;
        simulation::EntityOrder order{};
        order.entity = intent.self;
        order.target = intent.target.value_or(simulation::EntityId{});
        order.kind = intent.target.has_value()
            ? simulation::EntityOrderKind::MoveTo
            : simulation::EntityOrderKind::Hold;
        if (intent.target.has_value()) {
            if (const foundation::Vec3* target_position =
                    entities_.position(intent.target.value());
                target_position != nullptr) {
                order.destination = *target_position;
            } else {
                order.kind = simulation::EntityOrderKind::Hold;
                order.target = {};
            }
        } else if (intent.aim_target.has_value()) {
            order.destination = *intent.aim_target;
        } else if (const foundation::Vec3* self_position = entities_.position(intent.self);
                   self_position != nullptr) {
            order.destination = *self_position;
        }
        order.action = intent.readiness > 0.01F || intent.trigger
            ? foundation::stable_id("infantry.action.weapon-ready")
            : foundation::stable_id("infantry.action.advance");
        order.source = foundation::stable_id("ai.tactical");
        order.issued_tick = simulation_tick_.value;
        order.expires_tick = simulation_tick_.value;
        order.priority = 200U;
        if (infantry_ != nullptr) (void)infantry_->setOrder(order);
    }
    for (std::size_t index = 0; index < ai_entities_.size(); ++index) {
        ai_state_by_entity_[ai_entities_[index].id.packed()] = ai_states_[index];
    }
}

void BattlefieldRuntime::queueFire(float fixed_dt_seconds) noexcept {
    if (weapon_definition_ == nullptr || weapon_artifact_ == nullptr || infantry_ == nullptr) {
        snapshot_.error = "battlefield weapon handling bridge is not initialized";
        return;
    }
    for (auto& [packed_entity, state] : weapon_runtime_states_) {
        const simulation::EntityId entity = fromPacked(packed_entity);
        const foundation::Vec3* origin = entities_.position(entity);
        if (origin == nullptr) {
            continue;
        }
        infantry::InfantryWeaponHandlingView view{};
        if (!infantry_->readWeaponHandlingView(entity, view)) {
            continue;
        }
        const combat::AIIntent* intent = nullptr;
        for (const combat::AIIntent& candidate : intents_) {
            if (candidate.self == entity) {
                intent = &candidate;
                break;
            }
        }
        const bool engaging = view.state == infantry::AgentState::Engage;
        const bool target_matches = view.target.isValid() && intent != nullptr &&
                                    intent->target.has_value() && intent->target.value() == view.target;
        const foundation::Vec3* target_position = view.target.isValid()
                                                       ? entities_.position(view.target)
                                                       : nullptr;
        const bool request_fire = engaging && target_matches && intent->trigger &&
                                  target_position != nullptr;
        std::optional<foundation::Vec3> aim_target;
        if (target_position != nullptr) {
            aim_target = *target_position;
        } else if (intent != nullptr && intent->aim_target.has_value()) {
            aim_target = intent->aim_target;
        }
        (void)weapon_handling_.requestReadiness(state, engaging ? 1.0F : 0.0F);
        weapons::WeaponStepOutput output{};
        const weapons::WeaponHandlingInput input{
            static_cast<foundation::StableId>(entity.packed()), weapon_definition_,
            weapon_artifact_.get(), *origin, aim_target,
            {std::sqrt(view.velocity.x * view.velocity.x + view.velocity.z * view.velocity.z) >
                 4.0F,
             false, false,
             std::sqrt(view.velocity.x * view.velocity.x + view.velocity.y * view.velocity.y +
                        view.velocity.z * view.velocity.z)},
            request_fire};
        const auto stepped = weapon_handling_.step(
            state, input, simulation_tick_, fixed_dt_seconds, output);
        if (!stepped) {
            snapshot_.error = stepped.error().message;
            return;
        }
        weapon_pose_tasks_[packed_entity] = output.pose;
        if (output.fire.has_value() && !combat_flow_.submitFire(*output.fire)) {
            snapshot_.error = "duplicate authoritative fire intent";
            return;
        }
    }
    const auto committed = combat_flow_.commitFire(config_.seed);
    if (!committed) {
        snapshot_.error = committed.error().message;
        return;
    }
    for (const combat::FireRequest& request : committed.value()) {
        const std::uint64_t projectile_value = foundation::stableHashCombine(
            request.source, request.shot_sequence);
        const std::uint64_t trace_value = foundation::stableHashCombine(projectile_value, 0x51U);
        ballistics::FireRequest ballistic_request{
            ballistics::ProjectileId{projectile_value}, ballistics::ShotId{request.shot_sequence},
            ballistics::TraceId{trace_value}, {},
            ballistics::AmmunitionId{request.ammunition_id}, request.origin, request.direction,
            request.tick.value, request.seed, false, {}, {}, {}, {}};
        if (ballistics_ != nullptr && ballistics_->queueFire(ballistic_request).accepted) {
            projectile_sources_[projectile_value] = fromPacked(request.source);
            snapshot_.last_shot_source = fromPacked(request.source);
            ++snapshot_.fired;
        }
    }
}

const weapons::WeaponPoseTasks* BattlefieldRuntime::weaponPoseTasks(
    simulation::EntityId entity) const noexcept {
    const auto iterator = weapon_pose_tasks_.find(entity.packed());
    return iterator == weapon_pose_tasks_.end() ? nullptr : &iterator->second;
}

void BattlefieldRuntime::advanceBallistics() noexcept {
    ballistic_contacts_.clear();
    if (ballistics_ == nullptr) {
        return;
    }
    const ballistics::BallisticsTickResult result = ballistics_->advanceFixed(false);
    ballistic_contacts_ = result.contacts;
    snapshot_.impacts += ballistic_contacts_.size();
}

void BattlefieldRuntime::applyImpactDamage() noexcept {
    for (const ballistics::BallisticsContact& contact : ballistic_contacts_) {
        const auto source = projectile_sources_.find(contact.trace.projectile_id);
        const simulation::EntityId target_entity = fromPacked(contact.trace.semantic_id);
        TargetRuntime* target_runtime = target(target_entity);
        if (source == projectile_sources_.end() || target_runtime == nullptr ||
            !entities_.contains(target_entity)) {
            continue;
        }
        const foundation::Vec3* target_position = entities_.position(target_entity);
        if (target_position != nullptr) {
            const foundation::Vec3 local_point = subtract(contact.trace.point, *target_position);
            const auto destruction = target_runtime->damage_field.apply(
                {foundation::stableHashCombine(contact.trace.projectile_id, contact.trace.impact_index),
                 local_point, contact.trace.normal, 0.75F, 0.6F, weapon_spec_.damage,
                 {1.0F, 1.0F, 0.5F, 0.25F}, false, true});
            snapshot_.destruction_damage += destruction.aggregate_damage_delta;
            snapshot_.destruction_holes = target_runtime->damage_field.holes().size();
        }
        const combat::ImpactEvent impact{source->second,
                                         target_entity,
                                         combat::ImpactTargetKind::Infantry,
                                         foundation::stable_id("infantry.tissue"),
                                         contact.trace.point,
                                         contact.trace.normal,
                                         weapon_spec_.damage,
                                         simulation_tick_,
                                         contact.trace.impact_index};
        snapshot_.last_impact_source = source->second;
        snapshot_.last_impact_target = target_entity;
        (void)combat_flow_.submitImpact(impact);
    }
    const auto damage_commands = combat_flow_.commitDamage();
    if (!damage_commands) {
        snapshot_.error = damage_commands.error().message;
        ballistic_contacts_.clear();
        return;
    }
    for (const combat::DamageCommand& damage : damage_commands.value()) {
        damage_buffer_.push({damage.source, damage.target, damage.amount, damage.type, damage.tick});
    }
    const combat::CombatApplyResult result = combat_.apply(damage_buffer_);
    snapshot_.accepted_damage += result.accepted_events;
    snapshot_.deaths += result.killed_entities;
    ballistic_contacts_.clear();
}

BattlefieldRuntime::TargetRuntime* BattlefieldRuntime::target(simulation::EntityId entity) noexcept {
    const auto iterator = std::find_if(targets_.begin(), targets_.end(), [entity](const auto& value) {
        return value.entity == entity;
    });
    return iterator == targets_.end() ? nullptr : &*iterator;
}

const BattlefieldRuntime::TargetRuntime* BattlefieldRuntime::target(
    simulation::EntityId entity) const noexcept {
    const auto iterator = std::find_if(targets_.begin(), targets_.end(), [entity](const auto& value) {
        return value.entity == entity;
    });
    return iterator == targets_.end() ? nullptr : &*iterator;
}

bool BattlefieldRuntime::provideContact(void* context, const world_core::QuerySegmentHit& hit,
                                         ballistics::ContactCandidate& candidate) noexcept {
    auto* scenario = static_cast<BattlefieldRuntime*>(context);
    if (scenario == nullptr) {
        return false;
    }
    const simulation::EntityId entity = fromPacked(hit.candidate.id);
    TargetRuntime* target_runtime = scenario->target(entity);
    const auto team = scenario->teams_.find(entity.packed());
    if (target_runtime == nullptr || team == scenario->teams_.end()) {
        return false;
    }
    const foundation::Vec3 normal = team->second == 0U
                                        ? foundation::Vec3{0.0F, 0.0F, 1.0F}
                                        : foundation::Vec3{0.0F, 0.0F, -1.0F};
    candidate.contact_id = entity.packed();
    candidate.point = hit.point;
    candidate.normal = normal;
    candidate.entry_point = hit.point;
    candidate.exit_point = {hit.point.x - normal.x, hit.point.y - normal.y,
                            hit.point.z - normal.z};
    candidate.target_linear_velocity = {};
    candidate.target_angular_velocity = {};
    candidate.target_center_of_mass = scenario->entities_.position(entity) == nullptr
                                          ? foundation::Vec3{}
                                          : *scenario->entities_.position(entity);
    candidate.assembly = &target_runtime->assembly;
    candidate.limits = {32U, 32U, 1.0e-4F};
    return true;
}

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
