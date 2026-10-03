#include <genomes/runtime/RenderCoordinator.hpp>

#include <genomes/runtime/SceneDirector.hpp>

namespace genomes::runtime {

foundation::Result<void, foundation::Error> RenderCoordinator::present() noexcept {
    // Device-loss and stopped states are terminal for the presentation path.
    // Do not even enter SceneDirector::present(), since that would invoke the
    // backend begin/end hooks after the renderer has declared itself unsafe.
    if (!director_.rendererHealthy()) {
        const auto error = director_.rendererLastError();
        return foundation::Result<void, foundation::Error>::failure(
            error.code == foundation::ErrorCode::None
                ? foundation::Error{foundation::ErrorCode::InvalidState,
                                    "renderer is unhealthy before presentation"}
                : error);
    }
    director_.present();
    if (director_.rendererHealthy()) {
        return foundation::Result<void, foundation::Error>::success();
    }
    const auto error = director_.rendererLastError();
    return foundation::Result<void, foundation::Error>::failure(
        error.code == foundation::ErrorCode::None
            ? foundation::Error{foundation::ErrorCode::Internal,
                                "renderer became unhealthy during presentation"}
            : error);
}

} // namespace genomes::runtime
