#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <string>

namespace genomes::platform { class SdlPlatform; }
namespace genomes::render {

struct ThreeppGlInfo final {
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string shading_language;
    int major{0};
    int minor{0};
    int max_vertex_attributes{0};
    int max_texture_size{0};
    int vertex_texture_units{0};
    int default_depth_bits{0};
    int default_stencil_bits{0};
};

// One SDL/main-thread context per process. No GL/SDL/threepp type escapes here.
// Call after context creation and before constructing any GLRenderer. It also
// refreshes the same GLAD dispatch on sequential context recreation.
[[nodiscard]] foundation::Result<ThreeppGlInfo, foundation::Error>
initializeThreeppGl(platform::SdlPlatform& platform);

} // namespace genomes::render
