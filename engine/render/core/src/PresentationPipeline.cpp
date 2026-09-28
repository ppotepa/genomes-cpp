#include <genomes/render/PresentationPipeline.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::render {

bool RenderQualityProfile::valid() const noexcept {
    return std::isfinite(render_scale) && render_scale > 0.1F && render_scale <= 2.0F &&
           max_instances > 0U && max_shadow_casters > 0U && std::isfinite(lod_far_distance) &&
           lod_far_distance > 0.0F;
}

foundation::Result<PresentationFrame, foundation::Error> PresentationPipeline::prepare(
    const PresentationSnapshot& source, RenderQualityProfile profile) const {
    if (!profile.valid()) {
        return foundation::Result<PresentationFrame, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid render quality profile"});
    }
    PresentationFrame frame{};
    frame.snapshot = source;
    frame.stats.input_instances = source.instances.size();
    frame.stats.debug_overlays = profile.debug_overlays;
    if (source.instances.size() > profile.max_instances) {
        const RenderCamera& camera = source.camera;
        if (camera.enabled && camera.valid()) {
            const auto distance_squared = [&camera](const RenderInstance& instance) {
                const float dx = instance.position.x - camera.position.x;
                const float dy = instance.position.y - camera.position.y;
                const float dz = instance.position.z - camera.position.z;
                return dx * dx + dy * dy + dz * dz;
            };
            std::stable_sort(frame.snapshot.instances.begin(), frame.snapshot.instances.end(),
                             [&distance_squared](const RenderInstance& left,
                                                 const RenderInstance& right) {
                                 const float left_distance = distance_squared(left);
                                 const float right_distance = distance_squared(right);
                                 return left_distance < right_distance ||
                                        (left_distance == right_distance &&
                                         left.object_id < right.object_id);
                             });
        } else {
            std::stable_sort(frame.snapshot.instances.begin(), frame.snapshot.instances.end(),
                             [](const RenderInstance& left, const RenderInstance& right) {
                                 return left.object_id < right.object_id;
                             });
        }
        frame.snapshot.instances.resize(profile.max_instances);
    }
    frame.stats.output_instances = frame.snapshot.instances.size();
    frame.stats.culled_instances = frame.stats.input_instances - frame.stats.output_instances;
    return foundation::Result<PresentationFrame, foundation::Error>::success(std::move(frame));
}

} // namespace genomes::render
