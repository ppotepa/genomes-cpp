#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/world/GridLayout.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    jobs::JobSystem jobs(2U);
    gameplay::WorldScenario scenario(jobs);
    world::WorldGenerationRequest request{};
    request.seed = 0x12345678ULL;
    request.map_size_m = 600U;
    assert(scenario.startNew(request));
    assert(scenario.activePlan() != nullptr);
    const auto first_hash = scenario.status().active_content_hash;
    assert(first_hash != 0U && scenario.activeRequest() != nullptr);
    assert(scenario.activeRequest()->seed == request.seed);
    const auto* artifact = scenario.activeArtifact();
    const auto artifact_handle = scenario.activeArtifactHandle();
    assert(artifact != nullptr && artifact->valid());
    const auto layout = world::GridLayout::forMap(request.map_size_m);
    assert(layout.valid());
    assert(artifact->terrain->width() == layout.sample_count);
    assert(artifact->terrain->height() == layout.sample_count);
    assert(artifact_handle.get() == artifact);
    assert(artifact->revision == world::artifactRevision(artifact->plan));
    assert(artifact->destruction_invalidations != nullptr);
    assert(artifact->destruction_invalidations->worldRevision() == artifact->revision);
    const auto stale_revision = artifact->revision == 1U ? 2U : artifact->revision - 1U;
    assert(!artifact->destruction_invalidations->emit(
        world::WorldId{1U}, foundation::stable_id("stale"), stale_revision,
        {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}},
        static_cast<world::DirtyReasonMask>(world::DirtyReason::Cover)));
    assert(scenario.activePlan() == &artifact->plan);
    assert(artifact->resolved_buildings->size() == artifact->plan.building_sites.size());

    const auto deterministic_artifact = gameplay::WorldScenario::compileArtifact(
        artifact->plan, request);
    assert(deterministic_artifact && deterministic_artifact.value().valid());
    assert(deterministic_artifact.value().revision == artifact->revision);
    assert(deterministic_artifact.value().terrain->width() == artifact->terrain->width());
    assert(deterministic_artifact.value().terrain_mesh->indices.size() ==
           artifact->terrain_mesh->indices.size());

    world::WorldGenerationRequest invalid = request;
    invalid.map_size_m = 64U;
    assert(!scenario.startNew(invalid));
    assert(scenario.status().active_content_hash == first_hash);
    assert(scenario.activePlan() != nullptr);

    assert(scenario.requestNew(request));
    assert(scenario.status().generation_pending);
    scenario.cancelPending();
    assert(!scenario.status().generation_pending);
    assert(scenario.status().active_content_hash == first_hash);
    return 0;
}
