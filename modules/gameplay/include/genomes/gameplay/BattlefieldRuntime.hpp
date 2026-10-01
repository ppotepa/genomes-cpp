#pragma once

#include <genomes/gameplay/BattlefieldScenario.hpp>

#include <memory>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

// Production ownership boundary for the authoritative battlefield pipeline.
// The legacy BattlefieldScenario remains a deterministic fixture facade; this
// runtime owns its ECS, infantry, combat, ballistics and SystemGraph lifetime.
class BattlefieldRuntime final {
public:
    BattlefieldRuntime(const BattlefieldRuntime&) = delete;
    BattlefieldRuntime& operator=(const BattlefieldRuntime&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldRuntime>,
                                             foundation::Error>
    start(const BattlefieldScenarioConfig& config = {}, jobs::JobSystem* jobs = nullptr);

    void fixedUpdate(double dt = 1.0 / 60.0) noexcept { scenario_->fixedUpdate(dt); }
    void fixedUpdate(const simulation::TickContext& context) noexcept {
        scenario_->fixedUpdate(context);
    }
    [[nodiscard]] const BattlefieldScenarioSnapshot& snapshot() const noexcept {
        return scenario_->snapshot();
    }
    [[nodiscard]] bool complete() const noexcept { return scenario_->complete(); }
    [[nodiscard]] const simulation::WorldEcs& ecs() const noexcept { return scenario_->ecs(); }
    [[nodiscard]] const combat::TacticalAIProfile& tacticalProfile() const noexcept {
        return scenario_->tacticalProfile();
    }
    [[nodiscard]] const std::vector<infantry::InfantryRenderState>& renderStates() const noexcept {
        return scenario_->renderStates();
    }

private:
    explicit BattlefieldRuntime(std::unique_ptr<BattlefieldScenario> scenario) noexcept
        : scenario_{std::move(scenario)} {}

    std::unique_ptr<BattlefieldScenario> scenario_;
};

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
