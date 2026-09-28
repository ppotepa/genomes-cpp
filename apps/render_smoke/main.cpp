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
    if (!backend->begin_frame() || !backend->end_frame() || !backend->wait_idle()) {
        std::cerr << "Diligent frame smoke failed\n";
        return 1;
    }
    std::cout << "Diligent headless backend initialized\n";
    backend->shutdown();
    return 0;
}
