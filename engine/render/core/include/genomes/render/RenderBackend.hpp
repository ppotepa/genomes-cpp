#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/NativeWindow.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/RenderCapabilities.hpp>

#include <cstdint>

namespace genomes::render {

struct RenderConfig final {
    RenderBackendKind backend{RenderBackendKind::Null};
    bool headless{true};
    bool validation{false};
    std::uint32_t frames_in_flight{2};
    std::uint32_t width{1280};
    std::uint32_t height{720};

    [[nodiscard]] bool valid() const noexcept {
        return frames_in_flight >= 1 && frames_in_flight <= 8 && width >= 320 &&
               width <= 16'384 && height >= 200 && height <= 16'384;
    }
};

using RenderResult = foundation::Result<void, foundation::Error>;

class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    [[nodiscard]] virtual RenderCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual RenderResult begin_frame() noexcept = 0;
    [[nodiscard]] virtual RenderResult end_frame() noexcept = 0;
    [[nodiscard]] virtual RenderResult wait_idle() noexcept = 0;
    virtual void shutdown() noexcept = 0;
};

} // namespace genomes::render
