#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

namespace genomes::runtime {

class SceneDirector;

// Application-side render hand-off. GPU ownership remains in the renderer;
// this boundary turns failed presentation into an explicit frame result.
class RenderCoordinator final {
public:
    explicit RenderCoordinator(SceneDirector& director) noexcept : director_(director) {}

    [[nodiscard]] foundation::Result<void, foundation::Error> present() noexcept;

private:
    SceneDirector& director_;
};

} // namespace genomes::runtime
