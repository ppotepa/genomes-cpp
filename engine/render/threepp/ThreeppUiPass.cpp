#include "ThreeppUiPass.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace genomes::render {
namespace {
struct GpuUiVertex final {
    float x, y, u, v;
    float r, g, b, a;
};

GLuint compileShader(GLenum kind, const char* source) noexcept {
    const GLuint shader = glCreateShader(kind);
    if (shader == 0U) return 0U;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        glDeleteShader(shader);
        return 0U;
    }
    return shader;
}

GLuint linkProgram(GLuint vs, GLuint fs) noexcept {
    const GLuint program = glCreateProgram();
    if (program == 0U) return 0U;
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        glDeleteProgram(program);
        return 0U;
    }
    return program;
}
} // namespace

struct ThreeppUiPass::Impl final {
    struct Texture final {
        GLuint id{0U};
        std::uint32_t width{0U};
        std::uint32_t height{0U};
        std::uint64_t content_revision{0U};
    };

    GLuint program{0U};
    GLuint vao{0U};
    GLuint vbo{0U};
    GLuint ibo{0U};
    GLint viewport_location{-1};
    GLint textured_location{-1};
    std::size_t vbo_capacity_bytes{0U};
    std::size_t ibo_capacity_bytes{0U};
    ThreeppUiStats stats{};
    std::unordered_map<std::uint64_t, Texture> textures;

    void destroyTexture(Texture& texture) noexcept {
        if (texture.id != 0U) glDeleteTextures(1, &texture.id);
        texture = {};
    }

    void destroy() noexcept {
        for (auto& [_, texture] : textures) destroyTexture(texture);
        textures.clear();
        if (ibo != 0U) glDeleteBuffers(1, &ibo);
        if (vbo != 0U) glDeleteBuffers(1, &vbo);
        if (vao != 0U) glDeleteVertexArrays(1, &vao);
        if (program != 0U) glDeleteProgram(program);
        program = vao = vbo = ibo = 0U;
        viewport_location = textured_location = -1;
        vbo_capacity_bytes = ibo_capacity_bytes = 0U;
        stats = {};
    }
};

ThreeppUiPass::ThreeppUiPass() : impl_(std::make_unique<Impl>()) {}
ThreeppUiPass::~ThreeppUiPass() { shutdown(); }

foundation::Result<void, foundation::Error> ThreeppUiPass::initialize() noexcept {
    using Result = foundation::Result<void, foundation::Error>;
    if (impl_->program != 0U) return Result::success();

    static constexpr const char* vertex_source = R"(#version 330 core
layout(location=0) in vec2 inPosition;
layout(location=1) in vec2 inUv;
layout(location=2) in vec4 inColor;
uniform vec2 uViewport;
out vec2 vUv;
out vec4 vColor;
void main() {
    vec2 p = vec2(inPosition.x / uViewport.x * 2.0 - 1.0,
                  1.0 - inPosition.y / uViewport.y * 2.0);
    gl_Position = vec4(p, 0.0, 1.0);
    vUv = inUv;
    vColor = inColor;
})";
    static constexpr const char* fragment_source = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uTextured;
out vec4 outColor;
void main() {
    vec4 texel = uTextured != 0 ? texture(uTexture, vUv) : vec4(1.0);
    outColor = vColor * texel;
})";

    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragment_source);
    if (vs == 0U || fs == 0U) {
        if (vs != 0U) glDeleteShader(vs);
        if (fs != 0U) glDeleteShader(fs);
        return Result::failure({foundation::ErrorCode::Internal,
                                "threepp UI shader compilation failed"});
    }
    impl_->program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (impl_->program == 0U) {
        return Result::failure({foundation::ErrorCode::Internal,
                                "threepp UI shader link failed"});
    }

    glGenVertexArrays(1, &impl_->vao);
    glGenBuffers(1, &impl_->vbo);
    glGenBuffers(1, &impl_->ibo);
    if (impl_->vao == 0U || impl_->vbo == 0U || impl_->ibo == 0U) {
        impl_->destroy();
        return Result::failure({foundation::ErrorCode::Internal,
                                "threepp UI buffer allocation failed"});
    }

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl_->ibo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GpuUiVertex),
                          reinterpret_cast<const void*>(offsetof(GpuUiVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GpuUiVertex),
                          reinterpret_cast<const void*>(offsetof(GpuUiVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GpuUiVertex),
                          reinterpret_cast<const void*>(offsetof(GpuUiVertex, r)));
    glBindVertexArray(0);

    impl_->viewport_location = glGetUniformLocation(impl_->program, "uViewport");
    impl_->textured_location = glGetUniformLocation(impl_->program, "uTextured");
    glUseProgram(impl_->program);
    const GLint sampler = glGetUniformLocation(impl_->program, "uTexture");
    if (sampler >= 0) glUniform1i(sampler, 0);
    glUseProgram(0);
    return Result::success();
}

foundation::Result<void, foundation::Error> ThreeppUiPass::draw(
    const ui::UiRenderFrame& frame, std::uint32_t width, std::uint32_t height) noexcept {
    using Result = foundation::Result<void, foundation::Error>;
    if (width == 0U || height == 0U) return Result::success();
    if (impl_->program == 0U) {
        auto initialized = initialize();
        if (!initialized) return initialized;
    }
    impl_->stats = {};

    std::unordered_set<std::uint64_t> live_textures;
    for (const auto& source : frame.textures) {
        if (source.id == 0U || source.width == 0U || source.height == 0U ||
            source.rgba.size() != static_cast<std::size_t>(source.width) *
                                  static_cast<std::size_t>(source.height) * 4U) {
            continue;
        }
        live_textures.insert(source.id);
        auto& target = impl_->textures[source.id];
        if (target.id != 0U && target.width == source.width &&
            target.height == source.height &&
            target.content_revision == source.content_revision) {
            continue;
        }
        impl_->destroyTexture(target);
        glGenTextures(1, &target.id);
        if (target.id == 0U) {
            return Result::failure({foundation::ErrorCode::Internal,
                                    "threepp UI texture allocation failed"});
        }
        target.width = source.width;
        target.height = source.height;
        target.content_revision = source.content_revision;
        glBindTexture(GL_TEXTURE_2D, target.id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     static_cast<GLsizei>(source.width), static_cast<GLsizei>(source.height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, source.rgba.data());
        ++impl_->stats.texture_uploads;
        impl_->stats.texture_upload_bytes += source.rgba.size();
    }
    for (auto it = impl_->textures.begin(); it != impl_->textures.end();) {
        if (!live_textures.contains(it->first)) {
            impl_->destroyTexture(it->second);
            it = impl_->textures.erase(it);
        } else {
            ++it;
        }
    }

    // The 3D camera may use a sub-viewport (UnitLab leaves room for panels).
    // UI coordinates are always framebuffer pixels and must cover the complete
    // window independently of the last 3D viewport.
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
#ifdef GL_FRAMEBUFFER_SRGB
    glDisable(GL_FRAMEBUFFER_SRGB);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->program);
    if (impl_->viewport_location >= 0) {
        glUniform2f(impl_->viewport_location, static_cast<float>(width), static_cast<float>(height));
    }
    glBindVertexArray(impl_->vao);
    glActiveTexture(GL_TEXTURE0);

    std::vector<GpuUiVertex> vertices;
    std::vector<std::uint32_t> indices;
    for (const auto& command : frame.commands) {
        vertices.clear();
        indices.clear();
        if (!command.vertices.empty() && !command.indices.empty()) {
            vertices.reserve(command.vertices.size());
            for (const auto& v : command.vertices) {
                vertices.push_back({v.x, v.y, v.u, v.v,
                                    v.color.r, v.color.g, v.color.b, v.color.a});
            }
            indices = command.indices;
        } else if (command.primitive == ui::UiDrawPrimitive::Quad &&
                   command.rect.width > 0.0F && command.rect.height > 0.0F) {
            const float x0 = command.rect.x, y0 = command.rect.y;
            const float x1 = x0 + command.rect.width, y1 = y0 + command.rect.height;
            const auto c = command.color;
            vertices = {{x0,y0,0,0,c.r,c.g,c.b,c.a}, {x1,y0,1,0,c.r,c.g,c.b,c.a},
                        {x1,y1,1,1,c.r,c.g,c.b,c.a}, {x0,y1,0,1,c.r,c.g,c.b,c.a}};
            indices = {0U,1U,2U,0U,2U,3U};
        } else {
            // Legacy text commands require RmlUi geometry. The production
            // threepp game enables RmlUi; headless/widget-only builds remain
            // renderer-neutral and do not fabricate a second glyph system here.
            continue;
        }
        if (vertices.empty() || indices.empty()) continue;
        if (std::any_of(indices.begin(), indices.end(),
                        [&](std::uint32_t index){ return index >= vertices.size(); })) {
            return Result::failure({foundation::ErrorCode::InvalidArgument,
                                    "threepp UI command contains invalid indices"});
        }

        if (command.scissor_enabled) {
            const int sx = std::max(0, static_cast<int>(command.scissor.x));
            const int sy_top = std::max(0, static_cast<int>(command.scissor.y));
            const int sw = std::max(0, static_cast<int>(command.scissor.width));
            const int sh = std::max(0, static_cast<int>(command.scissor.height));
            glEnable(GL_SCISSOR_TEST);
            glScissor(sx, std::max(0, static_cast<int>(height) - sy_top - sh), sw, sh);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        GLuint texture = 0U;
        if (command.texture != 0U) {
            const auto found = impl_->textures.find(command.texture);
            if (found != impl_->textures.end()) texture = found->second.id;
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        if (impl_->textured_location >= 0) glUniform1i(impl_->textured_location, texture != 0U ? 1 : 0);

        const std::size_t vertex_bytes = vertices.size() * sizeof(GpuUiVertex);
        const std::size_t index_bytes = indices.size() * sizeof(std::uint32_t);
        const auto grow = [](std::size_t current, std::size_t required) {
            std::size_t capacity = std::max<std::size_t>(4096U, current);
            while (capacity < required) capacity *= 2U;
            return capacity;
        };

        glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
        if (vertex_bytes > impl_->vbo_capacity_bytes) {
            impl_->vbo_capacity_bytes = grow(impl_->vbo_capacity_bytes, vertex_bytes);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->vbo_capacity_bytes),
                         nullptr, GL_STREAM_DRAW);
            ++impl_->stats.buffer_grows;
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertex_bytes),
                        vertices.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl_->ibo);
        if (index_bytes > impl_->ibo_capacity_bytes) {
            impl_->ibo_capacity_bytes = grow(impl_->ibo_capacity_bytes, index_bytes);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->ibo_capacity_bytes),
                         nullptr, GL_STREAM_DRAW);
            ++impl_->stats.buffer_grows;
        }
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(index_bytes),
                        indices.data());
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT, nullptr);
        ++impl_->stats.draw_calls;
    }

    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
    glDepthMask(GL_TRUE);
    return Result::success();
}

ThreeppUiStats ThreeppUiPass::lastStats() const noexcept {
    return impl_ ? impl_->stats : ThreeppUiStats{};
}

void ThreeppUiPass::shutdown() noexcept {
    if (impl_) impl_->destroy();
}

} // namespace genomes::render
