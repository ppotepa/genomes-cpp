#include <genomes/gameplay/BattlefieldRuntime.hpp>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>
BattlefieldRuntime::start(const BattlefieldScenarioConfig& config, jobs::JobSystem* jobs) {
    auto scenario = BattlefieldScenario::startBattlefieldScenario(config, jobs);
    if (!scenario) {
        return foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>::failure(
            scenario.error());
    }
    return foundation::Result<std::unique_ptr<BattlefieldRuntime>, foundation::Error>::success(
        std::unique_ptr<BattlefieldRuntime>(new BattlefieldRuntime(std::move(scenario.value()))));
}

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
