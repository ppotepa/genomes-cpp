#include "DiligentSceneRenderer.hpp"

namespace genomes::render {

void DiligentSceneRenderer::begin_frame() {
    if (frame_started_) {
        end_frame();
    }
    const auto result = backend_.begin_frame();
    healthy_ = static_cast<bool>(result);
    frame_started_ = healthy_;
    if (!healthy_) {
        last_error_ = result.error();
    }
}

void DiligentSceneRenderer::submit(const PresentationSnapshot& snapshot,
                                  const ui::UiDocument& document) {
    if (!healthy_ || !frame_started_) {
        return;
    }
    submitted_instances_ += snapshot.instances.size();
    submitted_ui_nodes_ += document.nodes.size();
    const auto accept = [this](const RenderResult& result) {
        if (!result) {
            healthy_ = false;
            last_error_ = result.error();
        }
        return static_cast<bool>(result);
    };
    if (!accept(backend_.draw_meshes(snapshot))) return;
    if (!accept(backend_.draw_instances(snapshot))) return;
    (void)accept(backend_.draw_ui(document));
}

void DiligentSceneRenderer::end_frame() {
    // Submission failure does not undo begin_frame. Close the backend frame
    // even when a pass failed, preserving the first error for the caller.
    if (!frame_started_) return;
    frame_started_ = false;
    const auto result = backend_.end_frame();
    if (!result && healthy_) last_error_ = result.error();
    healthy_ = healthy_ && static_cast<bool>(result);
    if (healthy_) last_error_ = {};
}

} // namespace genomes::render
