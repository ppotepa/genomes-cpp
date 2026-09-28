#include <genomes/compute/FieldAtlas.hpp>
#include <genomes/foundation/Types.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void run(std::uint32_t side) {
    using namespace genomes;
    compute::FieldAtlas atlas;
    const auto id = foundation::stable_id("benchmark.field_atlas") + side;
    const compute::FieldAtlasDescriptor descriptor{
        id, side, side, 1U, {}, {1.0F, 1.0F}, compute::FieldFormat::R32Float,
        compute::FieldAuthority::DerivedAdvisory};
    const auto define_begin = Clock::now();
    const auto defined = atlas.define(descriptor);
    const auto define_time = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - define_begin);
    if (!defined) {
        std::cerr << "field_atlas define failed for " << side << '\n';
        return;
    }

    const compute::FieldRegion full{0U, 0U, 0U, side, side};
    std::vector<float> values(static_cast<std::size_t>(side) * side, 1.0F);
    const auto full_begin = Clock::now();
    const auto full_write = atlas.writeFloat(id, full, values);
    const auto full_time = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - full_begin);

    const compute::FieldRegion partial{0U, side / 2U - 32U, side / 2U - 32U, 64U, 64U};
    const std::vector<float> partial_values(64U * 64U, 2.0F);
    const auto partial_begin = Clock::now();
    const auto partial_write = atlas.writeFloat(id, partial, partial_values);
    const auto partial_time = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - partial_begin);

    const auto readback_begin = Clock::now();
    const auto token = atlas.requestReadback(id, partial);
    const auto snapshot = token ? atlas.pollReadback(token.value())
                                : foundation::Result<compute::FieldReadback,
                                                     foundation::Error>::failure(
                                      {foundation::ErrorCode::Internal,
                                       "requestReadback failed"});
    const auto readback_time = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - readback_begin);
    if (token) {
        (void)atlas.releaseReadback(token.value());
    }

    std::cout << "field_atlas side=" << side << " cells=" << values.size()
              << " define_us=" << define_time.count() << " full_write_us="
              << full_time.count() << " partial_write_us=" << partial_time.count()
              << " readback_us=" << readback_time.count() << " dirty="
              << atlas.consumeDirtyRegions().size() << " status="
              << (full_write && partial_write && snapshot ? "ok" : "error") << '\n';
}

} // namespace

int main() {
    run(1024U);
    run(2048U);
    run(4096U);
    return 0;
}
