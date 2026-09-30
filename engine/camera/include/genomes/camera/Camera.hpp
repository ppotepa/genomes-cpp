#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/math/Bounds.hpp>
#include <genomes/math/Mat4.hpp>
#include <cstdint>
#include <utility>

namespace genomes::camera {

enum class CameraPreset : std::uint8_t { Custom, UnitLab, BuildingLab, Battlefield };
enum class CameraMode : std::uint8_t { Fixed, Orbit, Fly, RTS };
struct ViewportNormalized final { float x{0}; float y{0}; float width{1}; float height{1}; };
struct CameraLens final { float vertical_fov{1.04719755F}; float near_plane{0.05F}; float far_plane{1000.0F}; };
struct CameraRequest final {
    CameraPreset preset{CameraPreset::Custom};
    CameraMode mode{CameraMode::Orbit};
    genomes::math::Vec3 position{0,1,3};
    genomes::math::Vec3 target{0,1,0};
    genomes::math::Vec3 up{0,1,0};
    CameraLens lens{};
    ViewportNormalized viewport{};
};
struct PixelViewport final { int x{0}; int y{0}; int width{0}; int height{0}; };
struct ResolvedCamera final {
    PixelViewport viewport{};
    genomes::math::Mat4 view{};
    genomes::math::Mat4 projection{};
    genomes::math::Mat4 view_projection{};
    genomes::math::Mat4 inverse_view{};
    genomes::math::Mat4 inverse_projection{};
    genomes::math::Mat4 inverse_view_projection{};
    genomes::math::Frustum frustum{};
};

[[nodiscard]] foundation::Result<ResolvedCamera, foundation::Error>
resolve(const CameraRequest&, int framebuffer_width, int framebuffer_height);
[[nodiscard]] foundation::Result<math::Vec3, foundation::Error>
project(const ResolvedCamera&, math::Vec3 world);
[[nodiscard]] foundation::Result<math::Vec3, foundation::Error>
unproject(const ResolvedCamera&, math::Vec3 screen);
[[nodiscard]] foundation::Result<std::pair<math::Vec3, math::Vec3>, foundation::Error>
screenRay(const ResolvedCamera&, float pixel_x, float pixel_y);
[[nodiscard]] foundation::Result<CameraRequest, foundation::Error>
fitToBounds(const math::Aabb&, const CameraRequest& template_request);

} // namespace genomes::camera
