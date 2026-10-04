#include <genomes/gameplay/BattlefieldSession.hpp>

#if GENOMES_HAS_INFANTRY

#include <type_traits>
#include <utility>

namespace genomes::gameplay {

foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>
BattlefieldSession::startTactical(const BattlefieldScenarioConfig& config, jobs::JobSystem& jobs,
                                  BattlefieldExecutionMode execution_mode,
                                  proc::ProceduralRuntime* procedural_runtime) {
    auto runtime = BattlefieldRuntime::start(config, jobs, execution_mode, procedural_runtime);
    if (!runtime) {
        return foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>::failure(
            runtime.error());
    }
    auto session = std::unique_ptr<BattlefieldSession>(
        new BattlefieldSession{Runtime{std::move(runtime.value())}});
    session->refreshPresentationSnapshot();
    return foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>::success(
        std::move(session));
}

foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>
BattlefieldSession::startMassBattle(const InfantryMassBattleConfig& config, jobs::JobSystem& jobs,
                                    proc::ProceduralRuntime* procedural_runtime) {
    auto runtime = InfantryMassBattleRuntime::start(config, jobs, procedural_runtime);
    if (!runtime) {
        return foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>::failure(
            runtime.error());
    }
    auto session = std::unique_ptr<BattlefieldSession>(
        new BattlefieldSession{Runtime{std::move(runtime.value())}});
    session->refreshPresentationSnapshot();
    return foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>::success(
        std::move(session));
}

bool BattlefieldSession::advance(const simulation::TickContext& context) noexcept {
    const bool advanced = std::visit(
        [&context](const auto& runtime) { return runtime != nullptr && runtime->advance(context); },
        runtime_);
    if (advanced) {
        refreshPresentationSnapshot();
    }
    return advanced;
}

api::CommandReceipt BattlefieldSession::submit(api::CommandEnvelope command) {
    return std::visit(
        [&command](const auto& runtime) {
            return runtime != nullptr ? runtime->submit(std::move(command)) : api::CommandReceipt{};
        }, runtime_);
}

api::SnapshotView BattlefieldSession::snapshotView() const noexcept {
    return std::visit(
        [](const auto& runtime) { return runtime != nullptr ? runtime->snapshotView() : api::SnapshotView{}; },
        runtime_);
}

api::SnapshotView BattlefieldSession::query(api::ApiId query,
                                             const api::EncodedValue& arguments) const {
    return std::visit(
        [query, &arguments](const auto& runtime) {
            return runtime != nullptr ? runtime->query(query, arguments) : api::SnapshotView{};
        }, runtime_);
}

void BattlefieldSession::setSceneEpoch(std::uint64_t epoch) noexcept {
    std::visit([epoch](const auto& runtime) {
        if (runtime != nullptr) runtime->setSceneEpoch(epoch);
    }, runtime_);
    refreshPresentationSnapshot();
}

bool BattlefieldSession::bindWorldArtifact(
    std::shared_ptr<const ResolvedWorldArtifacts> artifact) noexcept {
    const bool bound = std::visit(
        [&artifact](const auto& runtime) {
            return runtime != nullptr && runtime->bindWorldArtifact(artifact);
        }, runtime_);
    if (bound) {
        refreshPresentationSnapshot();
    }
    return bound;
}

world::WorldArtifactRevision BattlefieldSession::worldArtifactRevision() const noexcept {
    return std::visit(
        [](const auto& runtime) {
            return runtime != nullptr ? runtime->worldArtifactRevision() : world::WorldArtifactRevision{};
        }, runtime_);
}

bool BattlefieldSession::isMassBattle() const noexcept {
    return std::holds_alternative<std::unique_ptr<InfantryMassBattleRuntime>>(runtime_);
}

const weapons::WeaponArtifact* BattlefieldSession::weaponArtifact() const noexcept {
    return std::visit([](const auto& runtime) {
        return runtime != nullptr ? runtime->weaponArtifact() : nullptr;
    }, runtime_);
}

bool BattlefieldSession::consumeRestartRequest() noexcept {
    return std::visit([](const auto& runtime) {
        return runtime != nullptr && runtime->consumeRestartRequest();
    }, runtime_);
}

bool BattlefieldSession::consumeWorldRegenerateRequest() noexcept {
    return std::visit([](const auto& runtime) {
        return runtime != nullptr && runtime->consumeWorldRegenerateRequest();
    }, runtime_);
}

std::optional<InfantryMassBattleSnapshot> BattlefieldSession::massBattleSnapshot() const noexcept {
    const auto* runtime = std::get_if<std::unique_ptr<InfantryMassBattleRuntime>>(&runtime_);
    if (runtime == nullptr || *runtime == nullptr) {
        return std::nullopt;
    }
    return (*runtime)->snapshot();
}

void BattlefieldSession::refreshPresentationSnapshot() noexcept {
    presentation_snapshot_ = {};
    std::visit([this](const auto& runtime) {
        if (runtime == nullptr) return;
        const auto& source = runtime->presentationSnapshot();
        presentation_snapshot_.metadata = source.metadata;
        presentation_snapshot_.states.reserve(source.states.size());
        for (const auto& state : source.states) {
            BattlefieldUnitPresentation output{};
            output.entity = state.entity;
            output.team = state.team;
            output.position = state.position;
            output.heading = state.heading;
            output.height = state.height;
            output.state = state.state;
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(state)>,
                                         infantry::InfantryRenderState>) {
                output.action = state.action;
            } else {
                output.animation_phase = state.animation_phase;
                output.animation_speed = state.animation_speed;
                output.animation_variant = state.animation_variant;
            }
            presentation_snapshot_.states.push_back(output);
        }
    }, runtime_);
}

} // namespace genomes::gameplay

#endif
