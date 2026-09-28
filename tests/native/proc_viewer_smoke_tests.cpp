#include "ProcViewerApp.hpp"

#include <cassert>
#include <thread>

int main() {
    using namespace genomes;
    proc_viewer::ProcViewerApp viewer(2U);
    assert(proc_viewer::ProcViewerApp::modes().size() == 7U);
    assert(viewer.selectMode(proc_viewer::ViewerMode::RoadsCity));
    world::WorldGenerationRequest request{};
    request.seed = 0x7777ULL;
    assert(viewer.regenerate(request));
    for (std::size_t attempt = 0U; attempt < 10000U && viewer.artifact() == nullptr; ++attempt) {
        assert(viewer.poll());
        std::this_thread::yield();
    }
    assert(viewer.artifact() != nullptr);
    const auto hash = viewer.artifact()->semantic.content_hash;
    world::WorldGenerationRequest invalid = request;
    invalid.map_size_m = 64U;
    assert(!viewer.regenerate(invalid));
    assert(viewer.artifact() != nullptr && viewer.artifact()->semantic.content_hash == hash);
    assert(viewer.trace().size() == 1U);
    return 0;
}
