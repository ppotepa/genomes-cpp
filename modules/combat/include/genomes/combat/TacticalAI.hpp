#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/simulation/Entity.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/combat/AIModelRegistry.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace genomes::combat {

inline constexpr AIModelId SimpleCombatModelId = 1U;

struct TacticalAIProfile final {
    std::uint32_t observation_period_ticks{12U};
    std::uint32_t memory_ticks{150U};
    float target_switch_ratio{0.85F};
    float fire_alignment_cos{0.99756405F};

    [[nodiscard]] bool valid() const noexcept;
};

struct TacticalAIProfileSnapshot final {
    TacticalAIProfile profile{};
    std::string id;
    std::filesystem::path source;
    content::FrozenContentSnapshot content;
    foundation::SimConfigHash fingerprint{};
};

[[nodiscard]] foundation::Result<TacticalAIProfileSnapshot, foundation::Error>
loadTacticalAIProfile(const std::filesystem::path& path);

struct VisibleTarget final {
    simulation::EntityId id{};
    foundation::Vec3 position{};
    float distance_squared{0.0F};
    bool line_of_sight{true};

    [[nodiscard]] bool valid() const noexcept;
};

struct TargetMemory final {
    simulation::EntityId target{};
    foundation::Vec3 last_known_position{};
    foundation::SimulationTick last_seen{};
    bool valid{false};
};

struct AIState final {
    AIModelId model_id{SimpleCombatModelId};
    std::uint32_t cadence_phase{0U};
    foundation::SimulationTick last_observation{};
    TargetMemory memory{};
    bool passive{false};
};

struct Observation final {
    simulation::EntityId self{};
    foundation::Vec3 position{};
    float heading_radians{0.0F};
    foundation::SimulationTick tick{};
    TacticalAIProfile profile{};
    TargetMemory memory{};
    std::span<const VisibleTarget> visible_targets{};
    weapons::WeaponId weapon_id{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct AIIntent final {
    simulation::EntityId self{};
    std::optional<simulation::EntityId> target;
    std::optional<foundation::Vec3> aim_target;
    weapons::WeaponId weapon_id{0};
    float readiness{0.0F};
    bool trigger{false};
    bool passive{false};
    foundation::SimulationTick tick{};

    [[nodiscard]] bool valid() const noexcept;
};

struct TacticalAIEntity final {
    simulation::EntityId id{};
    foundation::Vec3 position{};
    float heading_radians{0.0F};
    weapons::WeaponId weapon_id{0};
    std::span<const VisibleTarget> visible_targets{};
    AIState* state{nullptr};
};

class TacticalAISystem final {
public:
    explicit TacticalAISystem(TacticalAIProfile profile = {}) noexcept : profile_(profile) {}

    [[nodiscard]] foundation::Result<void, foundation::Error> registerDefaults() noexcept;
    [[nodiscard]] foundation::Result<std::vector<AIIntent>, foundation::Error> evaluate(
        std::span<TacticalAIEntity> entities, foundation::SimulationTick tick);

    [[nodiscard]] AIModelRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] const TacticalAIProfile& profile() const noexcept { return profile_; }
    [[nodiscard]] std::uint32_t evaluatedCount() const noexcept { return evaluated_count_; }

private:
    TacticalAIProfile profile_{};
    AIModelRegistry registry_{};
    std::uint32_t evaluated_count_{0};
};

[[nodiscard]] foundation::Result<AIIntent, foundation::Error> evaluateSimpleCombat(
    const Observation&, const AIState&) noexcept;

} // namespace genomes::combat
