#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>
#include <genomes/gameplay/WorldScenario.hpp>

#if GENOMES_HAS_INFANTRY

#include <genomes/foundation/StableHash.hpp>
#include <genomes/jobs/JobGraph.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponProcedural.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>

namespace genomes::gameplay {

namespace {

constexpr float kTau = 6.28318530717958647692F;
constexpr float kHalfTau = 3.14159265358979323846F;

[[nodiscard]] foundation::Error invalidMassBattleError(const char* message) noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

} // namespace

foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>, foundation::Error>
InfantryMassBattleRuntime::start(const InfantryMassBattleConfig& config, jobs::JobSystem& jobs,
                                 proc::ProceduralRuntime* procedural_runtime) {
    if (!config.valid()) {
        return foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>,
                                  foundation::Error>::failure(
            invalidMassBattleError("invalid infantry mass battle configuration"));
    }
    auto runtime = std::unique_ptr<InfantryMassBattleRuntime>(
        new InfantryMassBattleRuntime(config, jobs, procedural_runtime));
    const auto initialized = runtime->initialize();
    if (!initialized) {
        return foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>,
                                  foundation::Error>::failure(initialized.error());
    }
    return foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>,
                              foundation::Error>::success(std::move(runtime));
}

foundation::Result<void, foundation::Error> InfantryMassBattleRuntime::initialize() {
    const std::size_t total = static_cast<std::size_t>(config_.units_per_team) * 2U;
    units_.reserve(total);
    presentation_snapshot_.states.reserve(total);
    input_states_.resize(total);
    unit_updates_.resize(total);
    simulation_snapshot_.entities.reserve(total);
    const weapons::WeaponDefinition* weapon = weapons::WeaponCatalog::find(
        weapons::weapon_id("carbine"));
    if (weapon == nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            invalidMassBattleError("carbine missing from weapon catalog"));
    }
    const weapons::WeaponVariant variant{
        proc::Seed(foundation::stableHashCombine(config_.seed, 0xB17U)), 1.0F, 0.0F, 0U};
    foundation::Result<weapons::WeaponArtifact, foundation::Error> artifact =
        foundation::Result<weapons::WeaponArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "weapon generator unavailable"});
    if (procedural_runtime_ != nullptr &&
        procedural_runtime_->registry().find(proc::generatorId("weapons.artifact")) != nullptr) {
        proc::GenerationRequest<weapons::WeaponGenerationRequest, weapons::WeaponArtifact>
            generation;
        generation.generator = proc::generatorId("weapons.artifact");
        generation.input = std::make_shared<const weapons::WeaponGenerationRequest>(
            weapons::WeaponGenerationRequest{*weapon, variant});
        generation.seed_path = proc::SeedPath(variant.seed);
        generation.options.retained_bytes = sizeof(weapons::WeaponArtifact);
        const auto generated = procedural_runtime_->generateInline(generation);
        if (generated && generated.value()) {
            artifact = foundation::Result<weapons::WeaponArtifact, foundation::Error>::success(
                *generated.value());
        } else {
            artifact = foundation::Result<weapons::WeaponArtifact, foundation::Error>::failure(
                generated ? foundation::Error{foundation::ErrorCode::Internal,
                                              "weapon generator returned null"}
                          : generated.error());
        }
    } else {
        artifact = weapons::WeaponGeometryGenerator::build(*weapon, variant);
    }
    if (!artifact) {
        return foundation::Result<void, foundation::Error>::failure(artifact.error());
    }
    weapon_artifact_ = std::make_shared<const weapons::WeaponArtifact>(
        std::move(artifact.value()));

    const std::uint32_t columns = static_cast<std::uint32_t>(std::ceil(
        std::sqrt(static_cast<float>(config_.units_per_team))));
    // The battle occupies a readable arena inside the larger streamed world.
    // Scaling formation spacing with the full map made individual soldiers
    // sub-pixel at the camera distance required to see both teams.
    constexpr float spacing = 3.2F;
    const float formation_depth = static_cast<float>(columns - 1U) * spacing;
    const float start_z = -formation_depth * 0.5F;
    const float start_x = std::min(180.0F,
                                   static_cast<float>(config_.map_size_m) * 0.16F);
    for (std::uint32_t team_index = 0U; team_index < 2U; ++team_index) {
        const auto team = team_index == 0U ? infantry::Team::Blue : infantry::Team::Red;
        const float side = team == infantry::Team::Blue ? -1.0F : 1.0F;
        for (std::uint32_t index = 0U; index < config_.units_per_team; ++index) {
            const std::uint32_t row = index / columns;
            const std::uint32_t column = index % columns;
            const std::uint64_t stable_key = foundation::stableHashCombine(
                config_.seed, static_cast<std::uint64_t>(team_index) * 1000000ULL + index);
            const float lateral_jitter =
                (unitRandom01(config_.seed, stable_key, 0U) - 0.5F) * spacing * 0.55F;
            const float depth_jitter =
                (unitRandom01(config_.seed, stable_key, 1U) - 0.5F) * spacing * 0.55F;
            const foundation::Vec3 position{
                side * start_x + lateral_jitter,
                0.0F,
                start_z + static_cast<float>(row) * spacing +
                    static_cast<float>(column) * 0.01F + depth_jitter};
            const float initial_heading = team == infantry::Team::Blue
                ? kHalfTau * 0.5F : -kHalfTau * 0.5F;
            const auto entity = entities_.create({position, {}, initial_heading, 100.0F,
                                                  simulation::EntityAlive |
                                                      simulation::EntityVisible});
            if (!entity) {
                return foundation::Result<void, foundation::Error>::failure(entity.error());
            }
            Unit unit{};
            unit.entity = entity.value();
            unit.team = team;
            const auto sampled_variant = static_cast<std::uint32_t>(
                unitRandom01(config_.seed, stable_key, 5U) * 8.0F);
            unit.animation_variant = static_cast<std::uint8_t>(
                std::min(sampled_variant, 7U));
            const float pace = unitRandom01(config_.seed, stable_key, 2U);
            unit.base_speed_mps = 0.8F + pace * 0.45F;
            unit.goal = position;
            unit.last_direction_epoch = std::numeric_limits<std::uint64_t>::max();
            unit.animation_speed = 0.78F +
                unitRandom01(config_.seed, stable_key, 3U) * 0.48F;
            unit.animation_phase = unitRandom01(config_.seed, stable_key, 4U);
            units_.push_back(unit);
        }
    }
    if (!rebuildRenderStates(0U)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "infantry mass battle snapshot extraction failed"});
    }
    return foundation::Result<void, foundation::Error>::success();
}

bool InfantryMassBattleRuntime::bindWorldArtifact(
    std::shared_ptr<const ResolvedWorldArtifacts> artifact) noexcept {
    if (artifact == nullptr || !artifact->valid() ||
        (world_artifact_ != nullptr && world_artifact_->revision != artifact->revision)) {
        return false;
    }
    world_artifact_ = std::move(artifact);
    for (Unit& unit : units_) {
        foundation::Vec3* position = entities_.position(unit.entity);
        if (position == nullptr) return false;
        position->y = world_artifact_->sampleLandscape(position->x, position->z).ground_y;
        unit.goal.y = position->y;
    }
    return rebuildRenderStates(snapshot_.tick);
}

world::WorldArtifactRevision InfantryMassBattleRuntime::worldArtifactRevision() const noexcept {
    return world_artifact_ != nullptr ? world_artifact_->revision : 0U;
}

bool InfantryMassBattleRuntime::fixedUpdate(const simulation::TickContext& context) noexcept {
    if (!std::isfinite(context.fixed_dt_seconds) || context.fixed_dt_seconds <= 0.0) {
        return false;
    }
    applyApiCommands(context.tick);
    for (std::size_t index = 0; index < units_.size(); ++index) {
        if (!entities_.read(units_[index].entity, input_states_[index])) {
            input_states_[index] = {};
        }
    }
    try {
        std::atomic<bool> controller_failed{false};
        constexpr std::size_t update_grain = 128U;
        const std::size_t range_count =
            units_.size() / update_grain +
            (units_.size() % update_grain != 0U ? 1U : 0U);
        jobs::JobGraphBuilder builder;
        jobs::JobOptions options;
        options.lane = jobs::ExecutionLane::Worker;
        options.work_class = jobs::WorkClass::Simulation;
        options.priority = jobs::JobPriority::Critical;
        for (std::size_t batch = 0U; batch < range_count; ++batch) {
            const std::size_t begin = batch * update_grain;
            const std::size_t end = begin + std::min(units_.size() - begin, update_grain);
            (void)builder.add(
                [this, &context, &controller_failed, begin, end](jobs::JobContext& job) {
                    if (job.isCancellationRequested()) return;
                    for (std::size_t index = begin; index < end; ++index) {
                        unit_updates_[index] =
                            updateUnit(units_[index], input_states_[index], context);
                    }
                    std::array<simulation::EntityControlRequest, update_grain> requests{};
                    std::array<simulation::EntityControlCommand, update_grain> commands{};
                    const std::size_t count = end - begin;
                    for (std::size_t offset = 0U; offset < count; ++offset) {
                        const std::size_t index = begin + offset;
                        const Unit& unit = units_[index];
                        const UnitUpdate& update = unit_updates_[index];
                        auto& request = requests[offset];
                        request.state = input_states_[index];
                        request.enabled = update.valid;
                        request.order.entity = unit.entity;
                        request.order.kind = (update.animation_variant == 1U ||
                                              update.animation_variant == 2U ||
                                              update.animation_variant == 4U ||
                                              update.animation_variant == 6U)
                            ? simulation::EntityOrderKind::MoveTo
                            : simulation::EntityOrderKind::Hold;
                        request.order.destination = update.goal;
                        request.order.action = infantry::infantryUnitActionId(
                            update.animation_variant);
                        request.order.source = foundation::stable_id("controller.mass-battle");
                        request.order.issued_tick = context.tick.value;
                        request.order.expires_tick = context.tick.value;
                        request.requested_speed_mps = update.animation_variant == 1U
                            ? unit.base_speed_mps
                            : update.animation_variant == 2U ? unit.base_speed_mps * 2.0F
                            : update.animation_variant == 4U ? unit.base_speed_mps * 0.55F
                            : update.animation_variant == 6U ? unit.base_speed_mps * 0.25F
                                                             : 0.0F;
                        request.maximum_speed_mps = unit.base_speed_mps * 2.0F;
                        request.acceleration_mps2 = 2.8F;
                        request.turn_rate_radians_per_second = 0.65F +
                            unitRandom01(config_.seed, unit.entity.packed(),
                                         update.last_direction_epoch + 17U) * 0.75F;
                    }
                    const auto controlled = infantry_controller_.updateBatch(
                        std::span<const simulation::EntityControlRequest>(
                            requests.data(), count),
                        std::span<simulation::EntityControlCommand>(
                            commands.data(), count),
                        context);
                    if (!controlled) {
                        controller_failed.store(true, std::memory_order_release);
                        return;
                    }
                    const float limit = std::max(
                        1.0F, static_cast<float>(config_.map_size_m) * 0.5F - 12.0F);
                    for (std::size_t offset = 0U; offset < count; ++offset) {
                        const std::size_t index = begin + offset;
                        UnitUpdate& update = unit_updates_[index];
                        if (!update.valid) continue;
                        const auto& command = commands[offset];
                        if (command.entity != units_[index].entity) {
                            controller_failed.store(true, std::memory_order_release);
                            return;
                        }
                        update.position = command.position;
                        update.velocity = command.velocity;
                        update.heading = command.heading_radians;
                        if (update.position.x < -limit || update.position.x > limit) {
                            update.position.x = std::clamp(update.position.x, -limit, limit);
                            update.heading = wrappedAngle(-update.heading);
                        }
                        if (update.position.z < -limit || update.position.z > limit) {
                            update.position.z = std::clamp(update.position.z, -limit, limit);
                            update.heading = wrappedAngle(kHalfTau - update.heading);
                        }
                        if (world_artifact_ != nullptr) {
                            const LandscapeSample landscape =
                                world_artifact_->sampleLandscape(
                                    update.position.x, update.position.z);
                            if (!landscape.traversable()) {
                                update.position = input_states_[index].position;
                                update.velocity = {};
                            }
                            update.position.y = world_artifact_->sampleLandscape(
                                update.position.x, update.position.z).ground_y;
                        }
                        const float speed = std::sqrt(
                            update.velocity.x * update.velocity.x +
                            update.velocity.z * update.velocity.z);
                        update.velocity = {std::sin(update.heading) * speed, 0.0F,
                                           std::cos(update.heading) * speed};
                    }
                },
                options);
        }
        auto update_group = std::move(builder).build().run(jobs_);
        update_group.wait();
        if (update_group.failed() ||
            controller_failed.load(std::memory_order_acquire)) {
            return false;
        }
    } catch (...) {
        return false;
    }
    for (std::size_t index = 0; index < units_.size(); ++index) {
        Unit& unit = units_[index];
        const UnitUpdate& update = unit_updates_[index];
        if (!update.valid) {
            continue;
        }
        foundation::Vec3* position = entities_.position(unit.entity);
        foundation::Vec3* velocity = entities_.velocity(unit.entity);
        float* heading = entities_.heading(unit.entity);
        if (position == nullptr || velocity == nullptr || heading == nullptr) {
            continue;
        }
        *position = update.position;
        *velocity = update.velocity;
        *heading = update.heading;
        unit.animation_phase = update.animation_phase;
        unit.animation_variant = update.animation_variant;
        unit.goal = update.goal;
        unit.direction_changes = update.direction_changes;
        unit.last_direction_epoch = update.last_direction_epoch;
    }
    return rebuildRenderStates(context.tick.value);
}

api::CommandReceipt InfantryMassBattleRuntime::submit(api::CommandEnvelope command) {
    return api_commands_.enqueue(std::move(command),
                                 foundation::SimulationTick{snapshot_.tick});
}

void InfantryMassBattleRuntime::applyApiCommands(foundation::SimulationTick tick) noexcept {
    const auto commands = api_commands_.take(tick);
    for (const auto& command : commands) {
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
            command.payload.bytes.empty()) {
            continue;
        }

        const auto decoded = simulation::decodeEntityOrder(command.payload.bytes);
        if (!decoded) continue;
        simulation::EntityOrder order = *decoded;
        order.source = command.source;
        order.priority = command.priority;
        order.issued_tick = command.target_tick.value;
        order.expires_tick = std::max(order.expires_tick, order.issued_tick);
        for (auto& unit : units_) {
            if (unit.entity != order.entity) {
                continue;
            }
            if (order.kind == simulation::EntityOrderKind::MoveTo) {
                unit.goal = order.destination;
                unit.animation_variant = 1U;
            } else if (order.kind == simulation::EntityOrderKind::Hold) {
                simulation::EntityReadView state{};
                if (entities_.read(unit.entity, state)) {
                    unit.goal = state.position;
                }
                unit.animation_variant = 0U;
            }
            break;
        }
    }
}

InfantryMassBattleRuntime::UnitUpdate InfantryMassBattleRuntime::updateUnit(
    const Unit& unit,
    const simulation::EntityReadView& state,
    const simulation::TickContext& context) const noexcept {
    UnitUpdate output;
    if (state.id != unit.entity) {
        return output;
    }
    output.position = state.position;
    output.velocity = state.velocity;
    output.heading = state.heading;
    output.animation_phase = unit.animation_phase;
    output.animation_variant = unit.animation_variant;
    output.goal = unit.goal;
    output.direction_changes = unit.direction_changes;
    output.last_direction_epoch = unit.last_direction_epoch;
    output.valid = true;

    const std::uint64_t interval = 150U +
        static_cast<std::uint64_t>(unit.entity.index % 210U);
    const std::uint64_t direction_epoch = context.tick.value / interval;
    if (direction_epoch != output.last_direction_epoch) {
        output.last_direction_epoch = direction_epoch;
        ++output.direction_changes;
        const std::uint64_t key = direction_epoch * 17U;
        const float choice = unitRandom01(config_.seed, unit.entity.packed(), key + 11U);
        // Stances are short-lived decisions, not immutable unit classes.
        output.animation_variant = choice < 0.10F ? 0U : choice < 0.32F ? 1U
            : choice < 0.49F ? 2U : choice < 0.60F ? 3U
            : choice < 0.74F ? 4U : choice < 0.84F ? 5U
            : choice < 0.92F ? 6U : 7U;
        const float team_heading = unit.team == infantry::Team::Blue
            ? kHalfTau * 0.5F : -kHalfTau * 0.5F;
        const float spread = (unitRandom01(config_.seed, unit.entity.packed(), key + 12U)
                              - 0.5F) * 4.8F;
        const float bearing = team_heading + spread;
        const float range = 12.0F + 38.0F *
            unitRandom01(config_.seed, unit.entity.packed(), key + 13U);
        const float limit = std::max(1.0F, static_cast<float>(config_.map_size_m) * 0.5F - 12.0F);
        output.goal.x = std::clamp(output.position.x + std::sin(bearing) * range,
                                   -limit, limit);
        output.goal.z = std::clamp(output.position.z + std::cos(bearing) * range,
                                   -limit, limit);
    }
    if (world_artifact_ != nullptr) {
        const LandscapeSample destination = world_artifact_->sampleLandscape(
            output.goal.x, output.goal.z);
        if (destination.traversable()) {
            output.goal.y = destination.ground_y;
        } else {
            output.goal = output.position;
        }
    }
    const float dt = static_cast<float>(context.fixed_dt_seconds);
    output.animation_phase += unit.animation_speed * dt * 1.25F;
    output.animation_phase -= std::floor(output.animation_phase);
    return output;
}

bool InfantryMassBattleRuntime::rebuildRenderStates(std::uint64_t tick) noexcept {
    InfantryMassBattlePresentationSnapshot candidate{};
    candidate.states.reserve(units_.size());
    candidate.metadata.tick = tick;
    candidate.metadata.generation = tick;
    candidate.metadata.scene_epoch = scene_epoch_;
    candidate.metadata.revision = tick;
    simulation::SimulationSnapshot simulation_candidate{};
    simulation_candidate.metadata.tick = tick;
    simulation_candidate.metadata.generation = tick;
    simulation_candidate.metadata.scene_epoch = scene_epoch_;
    simulation_candidate.metadata.revision = tick;
    simulation_candidate.semantic_hash = foundation::stableHashU64(tick);
    simulation_candidate.entities.reserve(units_.size());
    InfantryMassBattleSnapshot snapshot_candidate{};
    snapshot_candidate.tick = tick;
    snapshot_candidate.total_units = units_.size();
    snapshot_candidate.blue_units = config_.units_per_team;
    snapshot_candidate.red_units = config_.units_per_team;
    struct RangeOutput final {
        std::vector<InfantryMassBattleRenderState> states;
        std::vector<simulation::SimulationSnapshotEntity> entities;
        float total_speed{0.0F};
        std::uint64_t direction_changes{0U};
    };
    constexpr std::size_t extraction_grain = 128U;
    const std::size_t range_count = units_.size() / extraction_grain +
                                    (units_.size() % extraction_grain != 0U ? 1U : 0U);
    std::vector<RangeOutput> ranges(range_count);
    try {
        jobs::JobGraphBuilder builder;
        jobs::JobOptions options;
        options.lane = jobs::ExecutionLane::Worker;
        options.work_class = jobs::WorkClass::Presentation;
        options.priority = jobs::JobPriority::Normal;
        for (std::size_t batch = 0U; batch < range_count; ++batch) {
            const std::size_t begin = batch * extraction_grain;
            const std::size_t end =
                begin + std::min(units_.size() - begin, extraction_grain);
            (void)builder.add(
                [this, &ranges, batch, begin, end](jobs::JobContext& job) {
                    if (job.isCancellationRequested()) return;
                    RangeOutput& output = ranges[batch];
                    output.states.reserve(end - begin);
                    output.entities.reserve(end - begin);
                    for (std::size_t index = begin; index < end; ++index) {
                        const Unit& unit = units_[index];
                        simulation::EntityReadView state{};
                        if (!entities_.read(unit.entity, state)) continue;
                        const float speed = std::sqrt(
                            state.velocity.x * state.velocity.x +
                            state.velocity.z * state.velocity.z);
                        output.total_speed += speed;
                        output.direction_changes += unit.direction_changes;
                        const infantry::AgentState activity =
                            unit.animation_variant == 7U
                                ? infantry::AgentState::Engage
                                : speed > 0.01F ? infantry::AgentState::Advance
                                               : infantry::AgentState::Idle;
                        output.states.push_back(
                            {unit.entity, unit.team, state.position, state.heading, 1.75F,
                             activity, unit.animation_phase,
                             unit.animation_speed, unit.animation_variant});
                        output.entities.push_back(
                            {unit.entity, state.position, state.velocity,
                             state.heading, state.flags});
                    }
                },
                options);
        }
        auto extraction_group = std::move(builder).build().run(jobs_);
        extraction_group.wait();
        if (extraction_group.failed()) return false;
    } catch (...) {
        return false;
    }

    float total_speed = 0.0F;
    for (RangeOutput& range : ranges) {
        total_speed += range.total_speed;
        snapshot_candidate.direction_changes += range.direction_changes;
        candidate.states.insert(candidate.states.end(), range.states.begin(), range.states.end());
        simulation_candidate.entities.insert(simulation_candidate.entities.end(),
                                              range.entities.begin(), range.entities.end());
    }
    for (std::size_t index = 0U; index < simulation_candidate.entities.size(); ++index) {
        const auto& entity = simulation_candidate.entities[index];
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, entity.entity.packed());
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashFloat(entity.position.x));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashFloat(entity.position.y));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashFloat(entity.position.z));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashFloat(entity.velocity.x));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashFloat(entity.velocity.y));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashFloat(entity.velocity.z));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash,
            foundation::stableHashFloat(entity.heading_radians));
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, entity.flags);
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, candidate.states[index].animation_variant);
    }
    for (const auto& [key, value] : api_world_values_) {
        simulation_candidate.semantic_hash = foundation::stableHashCombine(
            simulation_candidate.semantic_hash, foundation::stableHashString(key));
        for (const std::uint8_t byte : value) {
            simulation_candidate.semantic_hash = foundation::stableHashCombine(
                simulation_candidate.semantic_hash, byte);
        }
    }
    snapshot_candidate.average_speed_mps = candidate.states.empty()
        ? 0.0F : total_speed / static_cast<float>(candidate.states.size());
    simulation_snapshot_ = simulation_candidate;
    snapshot_ = snapshot_candidate;
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
    if (auto write = simulation_snapshot_exchange_.acquireWrite(); write) {
        write.value().snapshot() = simulation_snapshot_;
        if (const auto published = simulation_snapshot_exchange_.publish(
                std::move(write.value())); published) {
            simulation_snapshot_.metadata.generation =
                simulation_snapshot_exchange_.publishedSerial();
        }
    }
    if (auto write = presentation_snapshot_exchange_.acquireWrite(); write) {
        write.value().snapshot() = candidate;
        if (const auto published = presentation_snapshot_exchange_.publish(
                std::move(write.value())); published) {
            presentation_snapshot_ = std::move(candidate);
            presentation_snapshot_.metadata.generation =
                presentation_snapshot_exchange_.publishedSerial();
        }
    }
    return true;
}

float InfantryMassBattleRuntime::unitRandom01(std::uint64_t seed,
                                              std::uint64_t entity,
                                              std::uint64_t stream) noexcept {
    const std::uint64_t value = foundation::stableHashCombine(
        foundation::stableHashCombine(seed, entity), stream);
    constexpr double inverse = 1.0 / static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    return static_cast<float>(static_cast<double>(value & 0xffffffffULL) * inverse);
}

float InfantryMassBattleRuntime::wrappedAngle(float angle) noexcept {
    while (angle > kHalfTau) angle -= kTau;
    while (angle < -kHalfTau) angle += kTau;
    return angle;
}

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
