#include <genomes/render/PresentationPipeline.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    render::PresentationSnapshot source{};
    source.simulation_tick = 42U;
    source.camera.enabled = true;
    source.camera.revision = 77U;
    source.camera.position = {0.0F, 10.0F, 0.0F};
    source.camera.target = {0.0F, 0.0F, 0.0F};
    source.has_resolved_camera = true;
    source.resolved_camera.viewport = {12, 24, 640, 480};
    source.instances = {{3U, 1U, 1U, {30.0F, 0.0F, 0.0F}},
                        {1U, 1U, 1U, {1.0F, 0.0F, 0.0F}},
                        {2U, 1U, 1U, {2.0F, 0.0F, 0.0F}}};
    render::PresentationPipeline pipeline;
    const auto low = pipeline.prepare(source, {1.0F, 2U, 64U, 1000.0F, false});
    assert(low && low.value().stats.culled_instances == 1U);
    assert(low.value().snapshot.simulation_tick == source.simulation_tick);
    assert(source.instances.size() == 3U);
    assert(low.value().snapshot.instances.front().object_id == 1U);
    assert(low.value().snapshot.camera.revision == 77U);
    assert(low.value().snapshot.has_resolved_camera &&
           low.value().snapshot.resolved_camera.viewport.width == 640);
    source.clear_scene_payload();
    assert(source.simulation_tick == 42U && source.camera.enabled);
    assert(source.instances.empty());
    const auto debug = pipeline.prepare(source, {0.75F, 3U, 64U, 1000.0F, true});
    // Clearing a published scene payload is authoritative.  The presentation
    // pipeline must not resurrect instances from a previous snapshot.
    assert(debug && debug.value().stats.input_instances == 0U &&
           debug.value().stats.output_instances == 0U);
    assert(debug.value().stats.debug_overlays);
    assert(debug.value().snapshot.simulation_tick == low.value().snapshot.simulation_tick);
    return 0;
}
