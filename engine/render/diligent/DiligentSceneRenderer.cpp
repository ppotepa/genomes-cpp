#include "DiligentSceneRenderer.hpp"

namespace genomes::render {

void DiligentSceneRenderer::begin_frame() {
    const RenderResult result = backend_.begin_frame();
    healthy_ = static_cast<bool>(result);
    if (!healthy_) {
        last_error_ = result.error();
    }
}

void DiligentSceneRenderer::submit(const PresentationSnapshot& snapshot,
                                   const ui::UiDocument& document) {
    if (!healthy_) {
        return;
    }
    submitted_instances_ += snapshot.instances.size();
    submitted_ui_nodes_ += document.nodes.size();
    const RenderResult mesh_result = backend_.draw_meshes(snapshot);
    healthy_ = static_cast<bool>(mesh_result);
    if (!healthy_) {
        last_error_ = mesh_result.error();
        return;
    }
    const RenderResult instance_result = backend_.draw_instances(snapshot);
    healthy_ = static_cast<bool>(instance_result);
    if (!healthy_) {
        last_error_ = instance_result.error();
        return;
    }
    const RenderResult result = backend_.draw_ui(document);
    healthy_ = static_cast<bool>(result);
    if (!healthy_) {
        last_error_ = result.error();
    }
}

void DiligentSceneRenderer::end_frame() {
    if (!healthy_) {
        return;
    }
    const RenderResult result = backend_.end_frame();
    healthy_ = static_cast<bool>(result);
    if (!healthy_) {
        last_error_ = result.error();
    }
}

} // namespace genomes::render
