#include <genomes/render/PresentationPipeline.hpp>
#include <genomes/camera/Camera.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>

int main() {
    using namespace genomes;
    render::PresentationSnapshot snapshot{};
    snapshot.camera.enabled = true;
    snapshot.camera.position = {0.0F, 30.0F, 0.0F};
    snapshot.camera.target = {0.0F, 0.0F, 0.0F};
    snapshot.instances.reserve(100000U);
    const auto timed = [](auto&& work) {
        const auto begin = std::chrono::steady_clock::now();
        work();
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - begin)
            .count();
    };
    const auto generation_us = timed([&] {
        snapshot.instances.clear();
        for (std::uint32_t index = 0U; index < 100000U; ++index) {
            snapshot.instances.push_back({static_cast<std::uint64_t>(index + 1U), 1U, 1U,
                                          {static_cast<float>(index % 1000U), 0.0F,
                                           static_cast<float>(index / 1000U)}});
        }
    });
    camera::CameraRequest camera_request{};
    camera_request.position = {0.0F, 30.0F, 0.0F};
    camera_request.target = {0.0F, 0.0F, 0.0F};
    const auto camera_update_us = timed([&] {
        (void)camera::resolve(camera_request, 1920, 1080);
    });
    render::PresentationPipeline pipeline;
    std::optional<foundation::Result<render::PresentationFrame, foundation::Error>> result;
    const auto snapshot_us = timed([&] {
        result = pipeline.prepare(snapshot, {1.0F, 25000U, 4096U, 10000.0F, false});
    });
    std::cout << "full_presentation instances=" << snapshot.instances.size()
              << " generation_us=" << generation_us
              << " camera_update_us=" << camera_update_us
              << " snapshot_us=" << snapshot_us
              << " palette_upload=not_applicable gpu_draw=not_applicable"
              << " output=" << (result && *result ? result->value().snapshot.instances.size() : 0U)
              << '\n';
    return result && *result ? 0 : 1;
}
