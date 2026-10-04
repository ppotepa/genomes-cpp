#pragma once

#include <genomes/gameplay/BattlefieldSession.hpp>
#include <genomes/jobs/JobGraph.hpp>
#include <genomes/jobs/SchedulerClient.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/terrain/HeightField.hpp>
#include <genomes/infantry/PresentationAnimation.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace genomes::game_scenes {

#if GENOMES_HAS_INFANTRY

struct MassBattlePresentationBatch final {
    std::vector<render::RenderInstance> instances;
    std::vector<gameplay::BattlefieldUnitPresentation> states;
    std::uint64_t tick{0U};
};

class MassBattlePresentationScheduler final {
public:
    using PoseAtlas = std::array<std::shared_ptr<render::RenderMesh>, 44U>;
    MassBattlePresentationScheduler() = default;
    ~MassBattlePresentationScheduler() = default;

    MassBattlePresentationScheduler(const MassBattlePresentationScheduler&) = delete;
    MassBattlePresentationScheduler& operator=(const MassBattlePresentationScheduler&) = delete;

    void bind(jobs::SchedulerClient jobs) noexcept { jobs_ = jobs; }

    void cancel() noexcept;
    [[nodiscard]] bool active() const noexcept { return completion_.valid(); }
    [[nodiscard]] bool complete() const noexcept {
        return completion_.valid() && completion_.isComplete();
    }
    [[nodiscard]] bool failed() const noexcept {
        return completion_.valid() && completion_.failed();
    }
    void reset() noexcept;

    void schedule(std::shared_ptr<const std::vector<gameplay::BattlefieldUnitPresentation>> states,
                  std::shared_ptr<const terrain::HeightField> terrain,
                  float model_height, foundation::StableId mesh_id,
                  foundation::StableId blue_material, foundation::StableId red_material,
                  std::uint64_t tick);

    [[nodiscard]] std::shared_ptr<const MassBattlePresentationBatch> take() noexcept;

    void scheduleAtlas(
        std::shared_ptr<const render::SkinnedMeshPrototype> prototype,
        std::shared_ptr<const infantry::InfantryModelArtifact> artifact,
        std::shared_ptr<const std::array<std::optional<infantry::AnimationPose>, 44U>> samples);
    [[nodiscard]] bool atlasActive() const noexcept { return atlas_completion_.valid(); }
    [[nodiscard]] bool atlasComplete() const noexcept {
        return atlas_completion_.valid() && atlas_completion_.isComplete();
    }
    [[nodiscard]] bool atlasFailed() const noexcept {
        return atlas_completion_.valid() && atlas_completion_.failed();
    }
    [[nodiscard]] std::shared_ptr<const PoseAtlas> takeAtlas() noexcept;

private:
    jobs::SchedulerClient jobs_{};
    jobs::JobCompletion completion_;
    std::shared_ptr<MassBattlePresentationBatch> pending_;
    std::shared_ptr<const MassBattlePresentationBatch> ready_;
    jobs::JobCompletion atlas_completion_;
    std::shared_ptr<PoseAtlas> pending_atlas_;
};

#endif

} // namespace genomes::game_scenes
