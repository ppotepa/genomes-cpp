#include <genomes/infantry/InfantrySimulation.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::infantry {

namespace {

constexpr std::uint64_t kContactMemoryTicks = 180;

[[nodiscard]] float distance_squared(foundation::Vec3 left,
                                     foundation::Vec3 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return x * x + y * y + z * z;
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                        foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] foundation::Vec3 normalize_horizontal(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(std::max(0.000001F, value.x * value.x + value.z * value.z));
    return {value.x / length, 0.0F, value.z / length};
}

[[nodiscard]] Team opposite(Team team) noexcept {
    return team == Team::Blue ? Team::Red : Team::Blue;
}

} // namespace

InfantrySimulation::InfantrySimulation(simulation::EntityStore& entities,
                                       navigation::NavigationWorld* navigation,
                                       physics::PhysicsWorld* physics,
                                       jobs::JobSystem* jobs) noexcept
    : entities_{entities}, navigation_{navigation}, physics_{physics}, jobs_{jobs} {}

bool InfantrySimulation::contactMemoryFresh(const Agent& record,
                                             foundation::SimulationTick tick) noexcept {
    return record.has_contact_memory && tick.value >= record.last_contact_tick.value &&
           tick.value - record.last_contact_tick.value <= kContactMemoryTicks;
}

foundation::Result<simulation::EntityId, foundation::Error> InfantrySimulation::spawn(
    const InfantrySpawn& spawn_data) {
    if (!spawn_data.genome.valid()) {
        return foundation::Result<simulation::EntityId, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry genome"});
    }
    const auto entity = entities_.create({spawn_data.position,
                                          {},
                                          0.0F,
                                          spawn_data.genome.max_health,
                                          simulation::EntityAlive | simulation::EntityVisible});
    if (!entity) {
        return entity;
    }
    physics::BodyHandle body{};
    if (physics_ != nullptr) {
        const auto body_result = physics_->createBody(
            {physics::BodyType::Dynamic, {physics::ShapeKind::Sphere, {0.45F, 0.45F, 0.45F}, 0.45F},
             spawn_data.position, {}, 75.0F, 1u, 0xFFFF'FFFFu});
        if (!body_result) {
            entities_.destroy(entity.value());
            return foundation::Result<simulation::EntityId, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "could not create infantry physics body"});
        }
        body = body_result.value();
    }
    if (entity.value().index >= agents_.size()) {
        agents_.resize(static_cast<std::size_t>(entity.value().index) + 1);
    }
    Agent& record = agents_[entity.value().index];
    record = Agent{spawn_data.genome,
                   spawn_data.team,
                   {},
                   body,
                   {},
                   {2.0F, 700.0F, 4.0F, spawn_data.genome.attack_range, 1000},
                   {1000, {}},
                   AgentState::Idle,
                   true,
                   {},
                   0,
                   {}};
    record.squad_id = spawn_data.squad_id;
    squad_contacts_.try_emplace(record.squad_id);
    ++active_count_;
    return entity;
}

void InfantrySimulation::remove(simulation::EntityId entity) noexcept {
    Agent* record = agent(entity);
    if (record == nullptr) {
        return;
    }
    record->active = false;
    record->state = AgentState::Dead;
    record->target = {};
    record->route.clear();
    record->route_cursor = 0;
    record->last_known_target_position = {};
    record->last_contact_tick = {};
    record->has_contact_memory = false;
    if (physics_ != nullptr && record->body.isValid()) {
        physics_->destroyBody(record->body);
    }
    record->body = {};
    entities_.destroy(entity);
    --active_count_;
}

void InfantrySimulation::fixedUpdate(double dt, foundation::SimulationTick tick) noexcept {
    if (!std::isfinite(dt) || dt <= 0.0) {
        return;
    }
    rebuildSpatialIndex();
    perceive(tick);
    entities_.forEachLive([&](simulation::EntityId entity) {
        Agent* record = agent(entity);
        float* health = entities_.health(entity);
        if (record != nullptr && health != nullptr && *health <= 0.0F) {
            remove(entity);
        }
    });
    physics::PhysicsCommandBuffer physics_commands;
    steer(dt, tick, physics_ != nullptr ? &physics_commands : nullptr);
    if (physics_ != nullptr) {
        physics_->apply(physics_commands);
        physics_->step(static_cast<float>(dt));
        syncPhysics();
    }
    buildRenderStates();
}

void InfantrySimulation::emitCombatEvents(foundation::SimulationTick tick,
                                           combat::DamageBuffer& buffer) noexcept {
    entities_.forEachLive([&](simulation::EntityId entity) {
        Agent* record = agent(entity);
        if (record == nullptr || record->state != AgentState::Engage ||
            !record->target.isValid() || !entities_.contains(record->target)) {
            return;
        }
        const foundation::Vec3* origin = entities_.position(entity);
        const foundation::Vec3* target = entities_.position(record->target);
        if (origin == nullptr || target == nullptr) {
            return;
        }
        const foundation::Vec3 direction{target->x - origin->x, target->y - origin->y,
                                         target->z - origin->z};
        const auto shot = weapons::WeaponController::tryFire(
            static_cast<foundation::StableId>(entity.packed()), *origin, direction,
            record->weapon, record->weapon_state, tick);
        if (shot) {
            buffer.push({entity, record->target, shot.value().damage,
                         combat::DamageType::Kinetic, tick});
        }
    });
}

void InfantrySimulation::perceive(foundation::SimulationTick tick) noexcept {
    std::vector<simulation::EntityId> observers;
    observers.reserve(entities_.size());
    entities_.forEachLive([&](simulation::EntityId observer_id) {
        observers.push_back(observer_id);
    });

    if (jobs_ == nullptr || jobs_->workerCount() == 0 || observers.size() < 8) {
        perceiveRange(observers, 0, observers.size(), tick);
        publishSquadContacts(tick);
        return;
    }

    const std::size_t grain = jobs::chooseParallelGrain(
        0, observers.size(), *jobs_, jobs::ParallelForPolicy{8, 1, 0, 4});
    const auto handles = jobs::parallelFor(
        *jobs_, 0, observers.size(), grain,
        [this, &observers, tick](jobs::BatchRange range) {
            perceiveRange(observers, range.begin, range.end, tick);
        });
    for (const jobs::JobHandle& handle : handles) {
        jobs_->wait(handle);
    }
    publishSquadContacts(tick);
}

void InfantrySimulation::publishSquadContacts(foundation::SimulationTick tick) noexcept {
    entities_.forEachLive([this, tick](simulation::EntityId entity) {
        const Agent* record = agent(entity);
        if (record == nullptr || !record->has_contact_memory ||
            !contactMemoryFresh(*record, tick)) {
            return;
        }
        const auto contact_iterator = squad_contacts_.find(record->squad_id);
        if (contact_iterator == squad_contacts_.end()) {
            return;
        }
        SquadContact& contact = contact_iterator->second;
        const bool replace = !contact.valid ||
                             record->last_contact_tick.value > contact.last_seen.value ||
                             (record->last_contact_tick.value == contact.last_seen.value &&
                              entity.packed() < contact.source.packed());
        if (replace) {
            contact.source = entity;
            contact.position = record->last_known_target_position;
            contact.last_seen = record->last_contact_tick;
            contact.valid = true;
        }
    });
}

void InfantrySimulation::perceiveRange(const std::vector<simulation::EntityId>& observers,
                                        std::size_t begin,
                                        std::size_t end,
                                        foundation::SimulationTick tick) noexcept {
    constexpr simulation::CadencePolicy perception_policy{
        simulation::CadenceKind::AdaptiveTier,
        1,
        60,
        1,
        {1, 12, 30}};
    constexpr simulation::CadencePolicy path_policy{
        simulation::CadenceKind::EveryNTicks,
        30};
    for (std::size_t observer_index = begin; observer_index < end; ++observer_index) {
        const simulation::EntityId observer_id = observers[observer_index];
        Agent* observer = agent(observer_id);
        if (observer == nullptr) {
            continue;
        }
        const simulation::CadenceTier relevance = observer->state == AgentState::Engage
                                                       ? simulation::CadenceTier::Combat
                                                       : simulation::CadenceTier::Active;
        simulation::setCadenceTier(observer->perception_cadence, relevance, tick);
        if (!simulation::evaluateCadence(perception_policy,
                                         observer->perception_cadence,
                                         static_cast<foundation::StableId>(observer_id.packed()),
                                         tick)
                 .due) {
            continue;
        }
        simulation::EntityReadView observer_state{};
        if (!entities_.read(observer_id, observer_state)) {
            continue;
        }
        std::vector<simulation::EntityId> candidates;
        candidates.reserve(64);
        spatial_index_.queryRadius(observer_state.position, observer->genome.perception_radius,
                                   candidates);
        simulation::EntityId best_target{};
        float best_distance = observer->genome.perception_radius * observer->genome.perception_radius;
        for (const simulation::EntityId candidate_id : candidates) {
            if (candidate_id == observer_id) {
                continue;
            }
            const Agent* candidate = agent(candidate_id);
            if (candidate == nullptr || !candidate->active || candidate->team != opposite(observer->team)) {
                continue;
            }
            simulation::EntityReadView candidate_state{};
            if (!entities_.read(candidate_id, candidate_state)) {
                continue;
            }
            const float distance = distance_squared(observer_state.position, candidate_state.position);
            if (distance < best_distance ||
                (distance == best_distance &&
                 (!best_target.isValid() || candidate_id.packed() < best_target.packed()))) {
                best_distance = distance;
                best_target = candidate_id;
            }
        }
        foundation::Vec3 path_target{};
        bool has_path_target = false;
        if (best_target.isValid()) {
            simulation::EntityReadView target_state{};
            if (entities_.read(best_target, target_state)) {
                observer->target = best_target;
                observer->last_known_target_position = target_state.position;
                observer->last_contact_tick = tick;
                observer->has_contact_memory = true;
                path_target = target_state.position;
                has_path_target = true;
            } else {
                observer->target = {};
            }
        } else {
            observer->target = {};
            if (!contactMemoryFresh(*observer, tick)) {
                const auto squad_contact = squad_contacts_.find(observer->squad_id);
                const bool shared_contact =
                    squad_contact != squad_contacts_.end() && squad_contact->second.valid &&
                    tick.value >= squad_contact->second.last_seen.value &&
                    tick.value - squad_contact->second.last_seen.value <= kContactMemoryTicks;
                if (shared_contact) {
                    observer->last_known_target_position = squad_contact->second.position;
                    observer->last_contact_tick = squad_contact->second.last_seen;
                    observer->has_contact_memory = true;
                } else {
                    observer->has_contact_memory = false;
                    observer->route.clear();
                    observer->route_cursor = 0;
                }
            }
        }
        observer->state = observer->target.isValid() ||
                                  contactMemoryFresh(*observer, tick)
                              ? AgentState::Advance
                              : AgentState::Idle;
        if (observer->target.isValid()) {
            observer->state = AgentState::Engage;
        }
        if (!has_path_target && contactMemoryFresh(*observer, tick)) {
            path_target = observer->last_known_target_position;
            has_path_target = true;
        }
        if (navigation_ != nullptr && has_path_target &&
            simulation::evaluateCadence(
                path_policy, observer->path_cadence,
                static_cast<foundation::StableId>(observer_id.packed()), tick)
                .due) {
            auto path = navigation_->findPath({observer_state.position, path_target, 2048});
            if (path && path.value().succeeded()) {
                observer->route = std::move(path.value().points);
                observer->route_cursor = observer->route.size() > 1U ? 1U : 0U;
            } else {
                observer->route.clear();
                observer->route_cursor = 0;
            }
        }
    }
}

void InfantrySimulation::rebuildSpatialIndex() noexcept {
    spatial_index_.clear();
    spatial_index_.reserve(entities_.size());
    entities_.forEachLive([&](simulation::EntityId entity) {
        const foundation::Vec3* position = entities_.position(entity);
        if (position != nullptr) {
            spatial_index_.insert(entity, *position);
        }
    });
}

void InfantrySimulation::steer(double dt,
                               foundation::SimulationTick tick,
                               physics::PhysicsCommandBuffer* physics_commands) noexcept {
    entities_.forEachLive([&](simulation::EntityId entity) {
        Agent* record = agent(entity);
        foundation::Vec3* position = entities_.position(entity);
        foundation::Vec3* velocity = entities_.velocity(entity);
        float* heading = entities_.heading(entity);
        if (record == nullptr || position == nullptr || velocity == nullptr || heading == nullptr) {
            return;
        }
        const foundation::Vec3* target_position =
            record->target.isValid() ? entities_.position(record->target) : nullptr;
        const bool live_target = target_position != nullptr;
        if (!live_target) {
            record->target = {};
        }
        const bool has_memory = contactMemoryFresh(*record, tick);
        if (!live_target && !has_memory) {
            record->has_contact_memory = false;
            record->state = AgentState::Idle;
            *velocity = {};
            if (physics_commands != nullptr && record->body.isValid()) {
                physics_commands->setLinearVelocity(record->body, {});
            }
            return;
        }
        foundation::Vec3 steering_target = live_target ? *target_position
                                                        : record->last_known_target_position;
        if (record->route_cursor < record->route.size()) {
            const foundation::Vec3 route_point = record->route[record->route_cursor];
            const float route_distance = std::sqrt(distance_squared(*position, route_point));
            if (route_distance <= 1.5F && record->route_cursor + 1U < record->route.size()) {
                ++record->route_cursor;
            }
            if (record->route_cursor < record->route.size()) {
                steering_target = record->route[record->route_cursor];
            }
        }
        const foundation::Vec3 direction =
            normalize_horizontal(subtract(steering_target, *position));
        const float distance = std::sqrt(distance_squared(*position, steering_target));
        if (live_target && distance <= record->genome.attack_range) {
            *velocity = {};
            record->state = AgentState::Engage;
            if (physics_commands != nullptr && record->body.isValid()) {
                physics_commands->setLinearVelocity(record->body, {});
            }
            return;
        }
        if (!live_target && distance <= 1.5F) {
            record->state = AgentState::Idle;
            record->has_contact_memory = false;
            record->route.clear();
            record->route_cursor = 0;
            *velocity = {};
            if (physics_commands != nullptr && record->body.isValid()) {
                physics_commands->setLinearVelocity(record->body, {});
            }
            return;
        }
        record->state = AgentState::Advance;
        *velocity = {direction.x * record->genome.move_speed, 0.0F,
                     direction.z * record->genome.move_speed};
        if (physics_commands != nullptr && record->body.isValid()) {
            physics_commands->setLinearVelocity(record->body, *velocity);
        } else {
            position->x += velocity->x * static_cast<float>(dt);
            position->z += velocity->z * static_cast<float>(dt);
        }
        *heading = std::atan2(direction.x, direction.z);
    });
}

void InfantrySimulation::syncPhysics() noexcept {
    if (physics_ == nullptr) {
        return;
    }
    entities_.forEachLive([&](simulation::EntityId entity) {
        Agent* record = agent(entity);
        if (record == nullptr || !record->body.isValid()) {
            return;
        }
        physics::BodyState body_state{};
        if (!physics_->readBody(record->body, body_state)) {
            return;
        }
        if (foundation::Vec3* position = entities_.position(entity)) {
            *position = body_state.position;
        }
        if (foundation::Vec3* velocity = entities_.velocity(entity)) {
            *velocity = body_state.linear_velocity;
        }
    });
}

void InfantrySimulation::buildRenderStates() noexcept {
    render_states_.clear();
    render_states_.reserve(active_count_);
    entities_.forEachLive([&](simulation::EntityId entity) {
        const Agent* record = agent(entity);
        simulation::EntityReadView state{};
        if (record == nullptr || !record->active || !entities_.read(entity, state)) {
            return;
        }
        render_states_.push_back(
            {entity, record->team, state.position, state.heading, record->genome.height, record->state});
    });
}

InfantrySimulation::Agent* InfantrySimulation::agent(simulation::EntityId entity) noexcept {
    if (!entity.isValid() || entity.index >= agents_.size() || !entities_.contains(entity) ||
        !agents_[entity.index].active) {
        return nullptr;
    }
    return &agents_[entity.index];
}

const InfantrySimulation::Agent* InfantrySimulation::agent(simulation::EntityId entity) const noexcept {
    if (!entity.isValid() || entity.index >= agents_.size() || !entities_.contains(entity) ||
        !agents_[entity.index].active) {
        return nullptr;
    }
    return &agents_[entity.index];
}

} // namespace genomes::infantry
