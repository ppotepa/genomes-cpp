#include "ProcViewerApp.hpp"

#include <iostream>
#include <thread>

int main() {
    using namespace genomes;
    proc_viewer::ProcViewerApp viewer;
    world::WorldGenerationRequest request{};
    request.seed = 0x50524F43ULL;
    if (!viewer.regenerate(request)) {
        return 1;
    }
    for (std::size_t attempt = 0U; attempt < 100000U && viewer.artifact() == nullptr; ++attempt) {
        const auto result = viewer.poll();
        if (!result) {
            return 1;
        }
        std::this_thread::yield();
    }
    if (viewer.artifact() == nullptr) {
        return 1;
    }
    const auto& semantic = viewer.artifact()->semantic;
    std::cout << "proc_viewer hash=" << semantic.content_hash
              << " seed=" << request.seed
              << " map=" << semantic.map_size_m
              << " features=" << semantic.feature_count
              << " roads=" << semantic.road_count
              << " parcels=" << semantic.parcel_count
              << " buildings=" << semantic.building_count
              << " sites=" << semantic.building_site_count
              << " vegetation=" << semantic.vegetation_count
              << " rivers=" << semantic.river_count
              << " terrain_samples=" << semantic.terrain_sample_count << '\n';
    return 0;
}
