#include <genomes/render/PresentationPipeline.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    using namespace genomes;
    render::PresentationSnapshot snapshot{};
    snapshot.camera.enabled = true;
    snapshot.camera.position = {0.0F, 30.0F, 0.0F};
    snapshot.camera.target = {0.0F, 0.0F, 0.0F};
    snapshot.instances.reserve(100000U);
    for (std::uint32_t index = 0U; index < 100000U; ++index) {
        snapshot.instances.push_back({static_cast<std::uint64_t>(index + 1U), 1U, 1U,
                                      {static_cast<float>(index % 1000U), 0.0F,
                                       static_cast<float>(index / 1000U)}});
    }
    render::PresentationPipeline pipeline;
    const auto begin = std::chrono::steady_clock::now();
    const auto result = pipeline.prepare(snapshot, {1.0F, 25000U, 4096U, 10000.0F, false});
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "full_presentation instances=" << snapshot.instances.size()
              << " ms=" << elapsed.count()
              << " output=" << (result ? result.value().snapshot.instances.size() : 0U) << '\n';
    return result ? 0 : 1;
}
