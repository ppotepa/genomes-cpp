#include <genomes/render/NullRenderBackend.hpp>

#include <cassert>

int main() {
    genomes::render::RenderConfig config{};
    genomes::render::NullRenderBackend backend{config};

    assert(backend.capabilities().initialized);
    assert(backend.begin_frame());
    assert(!backend.begin_frame());
    assert(backend.end_frame());
    assert(!backend.end_frame());
    assert(backend.wait_idle());

    backend.shutdown();
    assert(!backend.capabilities().initialized);
    assert(!backend.begin_frame());
    return 0;
}
