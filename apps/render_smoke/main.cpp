#include <DiligentBackend.hpp>

#include <iostream>

int main() {
    genomes::render::RenderConfig config{};
#if defined(_WIN32)
    config.backend = genomes::render::RenderBackendKind::D3D12;
#else
    config.backend = genomes::render::RenderBackendKind::Vulkan;
#endif
    config.headless = true;

    auto created = genomes::render::DiligentBackend::create(config);
    if (!created) {
        std::cout << "Diligent unavailable: " << created.error().message << '\n';
        return 0;
    }

    auto backend = std::move(created.value());
    constexpr int frame_count=600;
    for(int frame=0;frame<frame_count;++frame) {
        if(!backend->begin_frame()||!backend->end_frame()) {
            std::cerr << "Diligent frame smoke failed at frame " << frame << '\n';
            return 1;
        }
    }
    if(!backend->wait_idle()) {
        std::cerr << "Diligent wait-idle smoke failed\n";
        return 1;
    }
    std::cout << "Diligent headless backend completed " << frame_count << " frames\n";
    backend->shutdown();
    return 0;
}
