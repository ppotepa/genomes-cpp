#include <genomes/combat/TacticalAI.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::combat {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

} // namespace

foundation::Result<void, foundation::Error> AIModelRegistry::registerModel(
    AIModelDefinition definition) {
    if (definition.id == 0U || definition.name.empty() || definition.evaluate == nullptr ||
        find(definition.id) != nullptr || find(definition.name) != nullptr) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid or duplicate AI model"});
    }
    models_.push_back(definition);
    return foundation::Result<void, foundation::Error>::success();
}

const AIModelDefinition* AIModelRegistry::find(AIModelId id) const noexcept {
    for (const AIModelDefinition& model : models_) {
        if (model.id == id) {
            return &model;
        }
    }
    return nullptr;
}

const AIModelDefinition* AIModelRegistry::find(std::string_view name) const noexcept {
    for (const AIModelDefinition& model : models_) {
        if (model.name == name) {
            return &model;
        }
    }
    return nullptr;
}

bool TacticalAIProfile::valid() const noexcept {
    return observation_period_ticks > 0U && memory_ticks > 0U && finite(target_switch_ratio) &&
           target_switch_ratio > 0.0F && target_switch_ratio <= 1.0F && finite(fire_alignment_cos) &&
           fire_alignment_cos >= -1.0F && fire_alignment_cos <= 1.0F;
}

bool VisibleTarget::valid() const noexcept {
    return id.isValid() && finite(position) && finite(distance_squared) && distance_squared >= 0.0F;
}

bool Observation::valid() const noexcept {
    return self.isValid() && finite(position) && finite(heading_radians) && profile.valid() &&
           finite(memory.last_known_position) && std::all_of(visible_targets.begin(), visible_targets.end(),
                                                               [](const VisibleTarget& target) {
                                                                   return target.valid();
                                                               });
}

bool AIIntent::valid() const noexcept {
    return self.isValid() && finite(readiness) && readiness >= 0.0F && readiness <= 1.0F &&
           (!target.has_value() || target->isValid()) &&
           (!aim_target.has_value() || finite(aim_target.value()));
}

foundation::Result<AIIntent, foundation::Error> evaluateSimpleCombat(
    const Observation& observation, const AIState& state) noexcept {
    if (!observation.valid()) {
        return foundation::Result<AIIntent, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid tactical observation"});
    }
    const VisibleTarget* nearest = nullptr;
    for (const VisibleTarget& candidate : observation.visible_targets) {
        if (!candidate.line_of_sight) {
            continue;
        }
        if (nearest == nullptr || candidate.distance_squared < nearest->distance_squared ||
            (candidate.distance_squared == nearest->distance_squared && candidate.id < nearest->id)) {
            nearest = &candidate;
        }
    }
    const VisibleTarget* chosen = nearest;
    if (state.memory.valid) {
        for (const VisibleTarget& candidate : observation.visible_targets) {
            if (candidate.id == state.memory.target && candidate.line_of_sight) {
                if (nearest == nullptr ||
                    candidate.distance_squared <= nearest->distance_squared / observation.profile.target_switch_ratio) {
                    chosen = &candidate;
                }
                break;
            }
        }
    }
    AIIntent intent{};
    intent.self = observation.self;
    intent.weapon_id = observation.weapon_id;
    intent.readiness = chosen != nullptr ? 1.0F : 0.0F;
    intent.tick = observation.tick;
    if (chosen != nullptr) {
        intent.target = chosen->id;
        intent.aim_target = chosen->position;
        const foundation::Vec3 delta{chosen->position.x - observation.position.x,
                                     chosen->position.y - observation.position.y,
                                     chosen->position.z - observation.position.z};
        const float horizontal = std::sqrt(delta.x * delta.x + delta.z * delta.z);
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const float forward_x = std::sin(observation.heading_radians);
        const float forward_z = std::cos(observation.heading_radians);
        const float alignment = horizontal > 1.0e-5F
                                    ? (forward_x * delta.x + forward_z * delta.z) / horizontal
                                    : 1.0F;
        intent.trigger = length > 1.0e-5F && alignment >= observation.profile.fire_alignment_cos;
    }
    return foundation::Result<AIIntent, foundation::Error>::success(intent);
}

foundation::Result<void, foundation::Error> TacticalAISystem::registerDefaults() noexcept {
    if (!profile_.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid tactical AI profile"});
    }
    if (registry_.find(SimpleCombatModelId) == nullptr) {
        return registry_.registerModel({SimpleCombatModelId, "simple_combat", evaluateSimpleCombat});
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<std::vector<AIIntent>, foundation::Error> TacticalAISystem::evaluate(
    std::span<TacticalAIEntity> entities, foundation::SimulationTick tick) {
    if (!profile_.valid() || registry_.find(SimpleCombatModelId) == nullptr) {
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "tactical AI defaults are not registered"});
    }
    std::vector<AIIntent> intents;
    for (TacticalAIEntity& entity : entities) {
        if (!entity.id.isValid() || entity.state == nullptr || !finite(entity.position) ||
            !finite(entity.heading_radians)) {
            return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid tactical AI entity"});
        }
        if (entity.state->cadence_phase == 0U) {
            entity.state->cadence_phase = static_cast<std::uint32_t>(entity.id.packed() %
                                                                       profile_.observation_period_ticks);
        }
        if ((tick.value + entity.state->cadence_phase) % profile_.observation_period_ticks != 0U) {
            continue;
        }
        Observation observation{entity.id, entity.position, entity.heading_radians, tick, profile_,
                                entity.state->memory, entity.visible_targets, entity.weapon_id};
        const AIModelDefinition* model = registry_.find(entity.state->model_id);
        AIIntent intent{};
        if (model == nullptr || model->evaluate == nullptr) {
            intent.self = entity.id;
            intent.passive = true;
            intent.tick = tick;
            entity.state->passive = true;
        } else {
            const auto evaluated = model->evaluate(observation, *entity.state);
            if (!evaluated) {
                intent.self = entity.id;
                intent.passive = true;
                intent.tick = tick;
                entity.state->passive = true;
            } else {
                intent = evaluated.value();
                entity.state->passive = false;
            }
        }
        const VisibleTarget* seen = nullptr;
        for (const VisibleTarget& candidate : entity.visible_targets) {
            if (candidate.line_of_sight &&
                (seen == nullptr || candidate.distance_squared < seen->distance_squared ||
                 (candidate.distance_squared == seen->distance_squared && candidate.id < seen->id))) {
                seen = &candidate;
            }
        }
        if (seen != nullptr) {
            entity.state->memory = {seen->id, seen->position, tick, true};
        } else if (entity.state->memory.valid &&
                   tick.value - entity.state->memory.last_seen.value > profile_.memory_ticks) {
            entity.state->memory = {};
        }
        entity.state->last_observation = tick;
        intents.push_back(intent.valid() ? intent : AIIntent{entity.id, {}, {}, 0U, 0.0F, false, true, tick});
        ++evaluated_count_;
    }
    std::stable_sort(intents.begin(), intents.end(), [](const AIIntent& left, const AIIntent& right) {
        return left.self.packed() < right.self.packed();
    });
    return foundation::Result<std::vector<AIIntent>, foundation::Error>::success(std::move(intents));
}

} // namespace genomes::combat
