#include "DiligentSceneRenderer.hpp"
namespace genomes::render {
void DiligentSceneRenderer::begin_frame() { frame_.begin([this] { return backend_.begin_frame(); }); }
void DiligentSceneRenderer::submit(const PresentationSnapshot& scene,const ui::UiRenderFrame& ui) {
    instances_=scene.instances.size(); ui_nodes_=ui.widgets.size()+ui.commands.size();
    backend_.set_camera(scene.camera);
    frame_.submit([&] {
        if (auto r=backend_.draw_meshes(scene);!r) return r;
        if (auto r=backend_.draw_instances(scene);!r) return r;
        return backend_.draw_ui(ui);
    });
}
void DiligentSceneRenderer::end_frame() {
    frame_.end([this] { return backend_.end_frame(); },[this] { return backend_.abort_frame(); });
}
} // namespace genomes::render
