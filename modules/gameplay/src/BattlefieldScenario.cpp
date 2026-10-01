#include <genomes/gameplay/BattlefieldScenario.hpp>

#include <utility>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

foundation::Result<std::unique_ptr<BattlefieldScenario>, foundation::Error>
BattlefieldScenario::startBattlefieldScenario(const BattlefieldScenarioConfig& config,
                                               jobs::JobSystem* jobs) {
    auto runtime = BattlefieldRuntime::start(config, jobs);
    if (!runtime) {
        return foundation::Result<std::unique_ptr<BattlefieldScenario>, foundation::Error>::failure(
            runtime.error());
    }
    return foundation::Result<std::unique_ptr<BattlefieldScenario>, foundation::Error>::success(
        std::unique_ptr<BattlefieldScenario>(
            new BattlefieldScenario(std::move(runtime.value()))));
}

void BattlefieldScenario::fixedUpdate(double dt) noexcept {
    runtime_->fixedUpdate(dt);
}

void BattlefieldScenario::fixedUpdate(const simulation::TickContext& context) noexcept {
    runtime_->fixedUpdate(context);
}

const BattlefieldScenarioSnapshot& BattlefieldScenario::snapshot() const noexcept {
    return runtime_->snapshot();
}

bool BattlefieldScenario::complete() const noexcept {
    return runtime_->complete();
}

const simulation::WorldEcs& BattlefieldScenario::ecs() const noexcept {
    return runtime_->ecs();
}

const combat::TacticalAIProfile& BattlefieldScenario::tacticalProfile() const noexcept {
    return runtime_->tacticalProfile();
}

const std::vector<infantry::InfantryRenderState>& BattlefieldScenario::renderStates() const noexcept {
    return runtime_->renderStates();
}

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
