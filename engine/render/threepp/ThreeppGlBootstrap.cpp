#include "ThreeppGlBootstrap.hpp"

#include <genomes/platform/Platform.hpp>

// Reviewed PRIVATE upstream seam. Only this translation unit sees it.
#include <threepp/utils/LoadGlad.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace genomes::render {
namespace {
bool initialized_by_genomes = false; // GLAD is process-global; SDL/main thread only.

void* sdl_proc_address(const char* name) {
    // GLAD1 uses void*, SDL3 uses a function pointer. This isolated conversion
    // relies on the desktop Windows/Linux GL ABI; do not duplicate it elsewhere.
    return reinterpret_cast<void*>(platform::SdlPlatform::gl_proc_address(name));
}
std::string gl_string(GLenum name) {
    const auto* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "";
}
} // namespace

foundation::Result<ThreeppGlInfo, foundation::Error>
initializeThreeppGl(platform::SdlPlatform& platform) {
    using Result = foundation::Result<ThreeppGlInfo, foundation::Error>;
    if (!platform.gl_context_current()) {
        return Result::failure({foundation::ErrorCode::InvalidState,
                                 "threepp bootstrap requires the current owning SDL GL context"});
    }
    if (threepp::gladLoaded() && !initialized_by_genomes) {
        return Result::failure({foundation::ErrorCode::InvalidState,
                                 "OpenGL was loaded outside the Genomes SDL bootstrap"});
    }
    try {
        if (initialized_by_genomes) {
            // Upstream loadGlad is first-load-wins. SDL may unload the driver on
            // closing its last window; a new context must not reuse old pointers.
            if (!gladLoadGLLoader(sdl_proc_address)) {
                return Result::failure({foundation::ErrorCode::Unsupported,
                                         "could not reload GLAD for the recreated SDL context"});
            }
        } else {
            threepp::loadGlad(sdl_proc_address);
            initialized_by_genomes = true;
        }
    } catch (const std::exception& error) {
        std::cerr << "threepp GLAD bootstrap: " << error.what() << '\n';
        return Result::failure({foundation::ErrorCode::Unsupported, "threepp GLAD bootstrap failed"});
    }
    if (!GLAD_GL_VERSION_3_3 || glGetString == nullptr || glGetIntegerv == nullptr) {
        return Result::failure({foundation::ErrorCode::Unsupported, "desktop OpenGL 3.3 is required"});
    }
    ThreeppGlInfo info{};
    info.vendor = gl_string(GL_VENDOR);
    info.renderer = gl_string(GL_RENDERER);
    info.version = gl_string(GL_VERSION);
    info.shading_language = gl_string(GL_SHADING_LANGUAGE_VERSION);
    glGetIntegerv(GL_MAJOR_VERSION, &info.major);
    glGetIntegerv(GL_MINOR_VERSION, &info.minor);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &info.max_vertex_attributes);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &info.max_texture_size);
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &info.vertex_texture_units);
    // Default framebuffer attachment queries are valid in the 3.3 core profile.
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &info.default_depth_bits);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL,
        GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &info.default_stencil_bits);
    if (info.vendor.empty() || info.renderer.empty() || info.max_vertex_attributes < 16 ||
        info.vertex_texture_units < 1 || info.default_depth_bits < 24 || info.default_stencil_bits < 8) {
        return Result::failure({foundation::ErrorCode::Unsupported,
                                 "SDL GL context does not meet the declared threepp profile"});
    }
    return Result::success(std::move(info));
}
} // namespace genomes::render
