#pragma once

#include <cstdint>

namespace genomes::foundation {

enum class NativeWindowSystem : std::uint8_t {
    Unknown,
    Win32,
    X11,
    Wayland,
};

// Opaque platform handles stay in the platform/render boundary.  No OS or
// graphics SDK header is required by the simulation and world modules.
struct NativeWindowHandle final {
    NativeWindowSystem system{NativeWindowSystem::Unknown};
    void* window{nullptr};
    void* display{nullptr};
    std::uint64_t window_id{0};

    [[nodiscard]] bool valid() const noexcept {
        switch (system) {
        case NativeWindowSystem::Win32:
            return window != nullptr;
        case NativeWindowSystem::X11:
            return display != nullptr && window_id != 0;
        case NativeWindowSystem::Wayland:
            return display != nullptr && window != nullptr;
        case NativeWindowSystem::Unknown:
            return false;
        }
        return false;
    }
};

} // namespace genomes::foundation
