#include <genomes/geometry/MeshOptimizer.hpp>
#include <genomes/geometry/PolygonOps.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <genomes/geometry/TangentSpace.hpp>
#if GENOMES_BENCH_HAS_CSG
#include <genomes/geometry/SolidOps.hpp>
#endif
#if GENOMES_BENCH_HAS_ASSETS
#include <genomes/assets/GltfImporter.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Sample = std::chrono::duration<double, std::micro>;

template <typename Work>
double measure(Work&& work, std::size_t samples = 25U) {
    std::vector<double> timings;
    timings.reserve(samples);
    for (std::size_t index = 0U; index < samples; ++index) {
        const auto begin = Clock::now();
        work();
        timings.push_back(Sample(Clock::now() - begin).count());
    }
    std::sort(timings.begin(), timings.end());
    const auto percentile = [&timings](double p) {
        const auto index = static_cast<std::size_t>(p * static_cast<double>(timings.size() - 1U));
        return timings[index];
    };
    std::cout << "median_us=" << percentile(0.50) << " p95_us=" << percentile(0.95)
              << " samples=" << timings.size();
    return percentile(0.50);
}

} // namespace

int main(int argc, char** argv) {
    using namespace genomes;
#if !GENOMES_BENCH_HAS_ASSETS
    (void)argc;
    (void)argv;
#endif
    const auto box = geometry::makeBoxResult({{2.0F, 3.0F, 1.5F}});
    if (!box) {
        return 1;
    }

    std::cout << "primitive ";
    measure([&] { (void)geometry::makeBoxResult({{2.0F, 3.0F, 1.5F}}); });
    std::cout << '\n';

    geometry::Polygon2 polygon{};
    polygon.outer = {{-1.0F, -1.0F}, {1.0F, -1.0F}, {1.0F, 1.0F}, {-1.0F, 1.0F}};
    std::cout << "polygon ";
    measure([&] { (void)geometry::triangulate(polygon); });
    std::cout << '\n';

    std::cout << "tangent ";
    measure([&] { (void)geometry::generateTangents(box.value()); });
    std::cout << '\n';

    std::cout << "optimize ";
    measure([&] { (void)geometry::optimizeMesh(box.value(), geometry::OptimizationPolicy::Static); });
    std::cout << '\n';
#if GENOMES_BENCH_HAS_CSG
    std::cout << "csg ";
    measure([&] { (void)geometry::booleanSolid(box.value(), box.value(), geometry::SolidBoolean::Union); });
    std::cout << '\n';
#endif
#if GENOMES_BENCH_HAS_ASSETS
    if (argc > 1) {
        std::cout << "gltf ";
        measure([&] { (void)assets::importStaticGltf(argv[1]); });
        std::cout << '\n';
    }
#endif
    return 0;
}
