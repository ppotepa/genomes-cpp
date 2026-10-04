#include <genomes/game_scenes/MassBattlePresentationScheduler.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/render/SkinnedDeformer.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::game_scenes {

#if GENOMES_HAS_INFANTRY

void MassBattlePresentationScheduler::cancel() noexcept {
    completion_.cancel();
    atlas_completion_.cancel();
    completion_ = {};
    atlas_completion_ = {};
    pending_.reset();
    ready_.reset();
    pending_atlas_.reset();
}

void MassBattlePresentationScheduler::reset() noexcept {
    pending_.reset();
    ready_.reset();
    completion_ = {};
}

void MassBattlePresentationScheduler::schedule(
    std::shared_ptr<const std::vector<gameplay::BattlefieldUnitPresentation>> states,
    std::shared_ptr<const terrain::HeightField> terrain, float model_height,
    foundation::StableId mesh_id, foundation::StableId blue_material,
    foundation::StableId red_material, std::uint64_t tick) {
    if (!states || states->empty() || completion_.valid() || jobs_ == nullptr) return;

    auto batch = std::make_shared<MassBattlePresentationBatch>();
    batch->states = *states;
    batch->tick = tick;
    constexpr std::size_t range_size = 128U;
    auto ranges = std::make_shared<std::vector<std::vector<render::RenderInstance>>>();
    ranges->resize(states->size() / range_size +
                   (states->size() % range_size != 0U ? 1U : 0U));

    jobs::JobGraphBuilder builder;
    jobs::JobOptions options;
    options.work_class = jobs::WorkClass::Presentation;
    options.lane = jobs::ExecutionLane::Worker;
    std::vector<jobs::JobGraphNode> range_nodes;
    range_nodes.reserve(ranges->size());
    for (std::size_t range = 0U; range < ranges->size(); ++range) {
        const std::size_t begin = range * range_size;
        const std::size_t end = begin + std::min(states->size() - begin, range_size);
        range_nodes.push_back(builder.add(
            [states, ranges, begin, end, range, terrain, model_height, mesh_id,
             blue_material, red_material, tick](jobs::JobContext&) {
                auto& output = (*ranges)[range];
                output.reserve(end - begin);
                for (std::size_t index = begin; index < end; ++index) {
                    const auto& state = (*states)[index];
                    foundation::Vec3 position = state.position;
                    if (terrain) position.y = terrain->sampleBilinear(position.x, position.z) + 0.02F;
                    const auto object_id = foundation::stable_id("entity.infantry") ^
                                           state.entity.packed();
                    const auto flags = render::RenderInstanceFlagDynamic |
                                       render::RenderInstanceFlagCastShadow |
                                       render::RenderInstanceFlagReceiveShadow |
                                       (state.team == infantry::Team::Red
                                            ? render::RenderInstanceFlagTeamRed : 0U);
                    const float scale = state.height / model_height;
                    output.push_back({object_id, mesh_id,
                                      state.team == infantry::Team::Blue ? blue_material : red_material,
                                      position, {scale, scale, scale}, -state.heading, tick, flags,
                                      state.team == infantry::Team::Red
                                          ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                                          : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                }
            }, options));
    }
    const auto merge = builder.add(
        [batch, ranges](jobs::JobContext&) {
            std::size_t total = 0U;
            for (const auto& range : *ranges) total += range.size();
            batch->instances.reserve(total);
            for (auto& range : *ranges) {
                batch->instances.insert(batch->instances.end(), range.begin(), range.end());
            }
        }, options);
    for (const auto node : range_nodes) builder.precedes(node, merge);

    pending_ = std::move(batch);
    completion_ = std::move(builder).build().start(*jobs_);
}

std::shared_ptr<const MassBattlePresentationBatch>
MassBattlePresentationScheduler::take() noexcept {
    if (!complete()) return {};
    if (!failed()) ready_ = std::move(pending_);
    pending_.reset();
    completion_ = {};
    auto result = std::move(ready_);
    ready_.reset();
    return result;
}

void MassBattlePresentationScheduler::scheduleAtlas(
    std::shared_ptr<const render::SkinnedMeshPrototype> prototype,
    std::shared_ptr<const infantry::InfantryModelArtifact> artifact,
    std::shared_ptr<const std::array<std::optional<infantry::AnimationPose>, 44U>> samples) {
    if (atlas_completion_.valid() || !prototype || !artifact || !samples || jobs_ == nullptr) return;
    auto baked = std::make_shared<PoseAtlas>();
    pending_atlas_ = baked;
    jobs::JobGraphBuilder builder;
    (void)builder.add(
        [baked, prototype = std::move(prototype), artifact = std::move(artifact),
         samples = std::move(samples)](jobs::JobContext&) {
            for (std::size_t slot = 0U; slot < baked->size(); ++slot) {
                if (!(*samples)[slot].has_value()) return;
                const auto palette = infantry_presentation::makePalette(
                    artifact->skeleton, std::span<const infantry::RigTransform>(
                        (*samples)[slot]->bones));
                auto mesh = std::make_shared<render::RenderMesh>(
                    render::deformSkinnedCPU(*prototype, palette));
                mesh->mesh_id = foundation::stableHashCombine(
                    foundation::stable_id("mesh.infantry.mass-battle.pose-atlas"), slot + 1U);
                mesh->revision = foundation::stableHashCombine(prototype->revision, mesh->mesh_id);
                (*baked)[slot] = std::move(mesh);
            }
        }, jobs::JobOptions{jobs::ExecutionLane::Render, jobs::WorkClass::Render});
    atlas_completion_ = std::move(builder).build().start(*jobs_);
}

std::shared_ptr<const MassBattlePresentationScheduler::PoseAtlas>
MassBattlePresentationScheduler::takeAtlas() noexcept {
    if (!atlasComplete()) return {};
    auto result = atlasFailed() ? std::shared_ptr<const PoseAtlas>{}
                                : std::move(pending_atlas_);
    pending_atlas_.reset();
    atlas_completion_ = {};
    return result;
}

#endif

} // namespace genomes::game_scenes
