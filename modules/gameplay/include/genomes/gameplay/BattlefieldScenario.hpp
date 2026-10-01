#pragma once

#include <genomes/gameplay/BattlefieldRuntime.hpp>

#include <memory>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

// Compatibility and deterministic fixture facade.  All mutable simulation
// state belongs to BattlefieldRuntime; this type deliberately owns no ECS,
// physics world, graph or combat resources.
class BattlefieldScenario final {
public:
    BattlefieldScenario(const BattlefieldScenario&) = delete;
    BattlefieldScenario& operator=(const BattlefieldScenario&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldScenario>,
                                             foundation::Error>
    startBattlefieldScenario(const BattlefieldScenarioConfig& config = {},
                             jobs::JobSystem* jobs = nullptr);

    void fixedUpdate(double dt = 1.0 / 60.0) noexcept;
    void fixedUpdate(const simulation::TickContext& context) noexcept;

    [[nodiscard]] const BattlefieldScenarioSnapshot& snapshot() const noexcept;
    [[nodiscard]] bool complete() const noexcept;
    [[nodiscard]] const simulation::WorldEcs& ecs() const noexcept;
    [[nodiscard]] const combat::TacticalAIProfile& tacticalProfile() const noexcept;
    [[nodiscard]] const std::vector<infantry::InfantryRenderState>& renderStates() const noexcept;

private:
    explicit BattlefieldScenario(std::unique_ptr<BattlefieldRuntime> runtime) noexcept
        : runtime_{std::move(runtime)} {}

    std::unique_ptr<BattlefieldRuntime> runtime_;
};

[[nodiscard]] inline foundation::Result<std::unique_ptr<BattlefieldScenario>, foundation::Error>
startBattlefieldScenario(const BattlefieldScenarioConfig& config = {},
                         jobs::JobSystem* jobs = nullptr) {
    return BattlefieldScenario::startBattlefieldScenario(config, jobs);
}

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
