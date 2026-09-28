#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/PresentationSnapshot.hpp>

#include <cstddef>
#include <cstdint>

namespace genomes::render {

struct RenderQualityProfile final {
    float render_scale{1.0F};
    std::size_t max_instances{100'000U};
    std::size_t max_shadow_casters{4096U};
    float lod_far_distance{10'000.0F};
    bool debug_overlays{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct PresentationFrameStats final {
    std::size_t input_instances{0U};
    std::size_t output_instances{0U};
    std::size_t culled_instances{0U};
    bool debug_overlays{false};
};

struct PresentationFrame final {
    PresentationSnapshot snapshot{};
    PresentationFrameStats stats{};
};

class PresentationPipeline final {
public:
    [[nodiscard]] foundation::Result<PresentationFrame, foundation::Error> prepare(
        const PresentationSnapshot& source, RenderQualityProfile profile = {}) const;
};

} // namespace genomes::render
