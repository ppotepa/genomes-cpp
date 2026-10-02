#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>

#if GENOMES_HAS_INFANTRY

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
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
InfantryMassBattleRuntime::start(const InfantryMassBattleConfig& config, jobs::JobSystem* jobs) {
    if (!config.valid()) {
        return foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>,
                                  foundation::Error>::failure(
            invalidMassBattleError("invalid infantry mass battle configuration"));
    }
    auto runtime = std::unique_ptr<InfantryMassBattleRuntime>(
        new InfantryMassBattleRuntime(config, jobs));
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
    render_states_.reserve(total);

    const std::uint32_t columns = static_cast<std::uint32_t>(std::ceil(
        std::sqrt(static_cast<float>(config_.units_per_team))));
    const float spacing = std::max(10.0F,
                                   static_cast<float>(config_.map_size_m) * 0.018F);
    const float formation_depth = static_cast<float>(columns - 1U) * spacing;
    const float start_z = -formation_depth * 0.5F;
    const float start_x = static_cast<float>(config_.map_size_m) * 0.32F;
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
            unit.base_speed_mps = 0.75F +
                unitRandom01(config_.seed, stable_key, 2U) * 1.65F;
            unit.animation_speed = 0.78F +
                unitRandom01(config_.seed, stable_key, 3U) * 0.48F;
            unit.animation_phase = unitRandom01(config_.seed, stable_key, 4U);
            unit.animation_variant = static_cast<std::uint8_t>(
                unitRandom01(config_.seed, stable_key, 5U) * 5.0F);
            units_.push_back(unit);
        }
    }
    rebuildRenderStates(0U);
    return foundation::Result<void, foundation::Error>::success();
}

void InfantryMassBattleRuntime::fixedUpdate(const simulation::TickContext& context) noexcept {
    if (!std::isfinite(context.fixed_dt_seconds) || context.fixed_dt_seconds <= 0.0) {
        return;
    }
    for (Unit& unit : units_) {
        updateUnit(unit, context);
    }
    rebuildRenderStates(context.tick.value);
}

void InfantryMassBattleRuntime::updateUnit(Unit& unit,
                                           const simulation::TickContext& context) noexcept {
    foundation::Vec3* position = entities_.position(unit.entity);
    foundation::Vec3* velocity = entities_.velocity(unit.entity);
    float* heading = entities_.heading(unit.entity);
    if (position == nullptr || velocity == nullptr || heading == nullptr) {
        return;
    }

    const std::uint64_t interval = 90U +
        static_cast<std::uint64_t>(unit.entity.index % 150U);
    const std::uint64_t direction_epoch = context.tick.value / interval;
    if (direction_epoch != unit.last_direction_epoch) {
        unit.last_direction_epoch = direction_epoch;
        ++unit.direction_changes;
    }
    const float direction = (unitRandom01(
        config_.seed, unit.entity.packed(), direction_epoch + 11U) * kTau) - kHalfTau;
    const float current = *heading;
    const float delta = wrappedAngle(direction - current);
    const float turn_limit = (0.65F +
        unitRandom01(config_.seed, unit.entity.packed(), direction_epoch + 17U) * 0.75F) *
        static_cast<float>(context.fixed_dt_seconds);
    *heading = wrappedAngle(current + std::clamp(delta, -turn_limit, turn_limit));

    const float speed_variation = 0.82F +
        unitRandom01(config_.seed, unit.entity.packed(), direction_epoch + 23U) * 0.24F;
    const float speed = unit.base_speed_mps * speed_variation;
    const float dt = static_cast<float>(context.fixed_dt_seconds);
    unit.animation_phase += unit.animation_speed * dt * 1.25F;
    unit.animation_phase -= std::floor(unit.animation_phase);
    velocity->x = std::sin(*heading) * speed;
    velocity->y = 0.0F;
    velocity->z = std::cos(*heading) * speed;
    position->x += velocity->x * dt;
    position->z += velocity->z * dt;

    const float limit = static_cast<float>(config_.map_size_m) * 0.5F - 12.0F;
    if (position->x < -limit || position->x > limit) {
        position->x = std::clamp(position->x, -limit, limit);
        *heading = wrappedAngle(-*heading);
    }
    if (position->z < -limit || position->z > limit) {
        position->z = std::clamp(position->z, -limit, limit);
        *heading = wrappedAngle(kHalfTau - *heading);
    }
}

void InfantryMassBattleRuntime::rebuildRenderStates(std::uint64_t tick) noexcept {
    render_states_.clear();
    render_states_.reserve(units_.size());
    snapshot_ = {};
    snapshot_.tick = tick;
    snapshot_.total_units = units_.size();
    snapshot_.blue_units = config_.units_per_team;
    snapshot_.red_units = config_.units_per_team;
    float total_speed = 0.0F;
    for (const Unit& unit : units_) {
        simulation::EntityReadView state{};
        if (!entities_.read(unit.entity, state)) {
            continue;
        }
        const float speed = std::sqrt(state.velocity.x * state.velocity.x +
                                      state.velocity.z * state.velocity.z);
        total_speed += speed;
        render_states_.push_back({unit.entity, unit.team, state.position, state.heading,
                                  1.75F, infantry::AgentState::Advance,
                                  unit.animation_phase, unit.animation_speed,
                                  unit.animation_variant});
        snapshot_.direction_changes += unit.direction_changes;
    }
    snapshot_.average_speed_mps = render_states_.empty()
        ? 0.0F : total_speed / static_cast<float>(render_states_.size());
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
