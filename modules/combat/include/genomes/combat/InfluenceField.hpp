#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace genomes::combat {

inline constexpr std::uint32_t InfluenceFieldVersion = 1U;

// A source is an observation already admitted to a faction's knowledge state.
// It deliberately carries no enemy entity handle: callers must not turn an
// omniscient world query into a threat source.
enum class InfluenceSourceType : std::uint8_t {
    KnownContact,
    FriendlyForce,
    Order,
};

struct InfluenceSource final {
    std::uint64_t stable_id{0U};
    foundation::Vec2 position{};
    foundation::Vec2 facing{1.0F, 0.0F};
    float radius_m{0.0F};
    float strength{0.0F};
    float confidence{0.0F};
    foundation::SimulationTick observed_tick{};
    float anisotropy{0.0F};
    InfluenceSourceType type{InfluenceSourceType::KnownContact};
    bool known_to_faction{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct InfluenceFieldSpec final {
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    foundation::Vec2 world_origin{};
    foundation::Vec2 cell_size{1.0F, 1.0F};
    foundation::SimulationTick evaluation_tick{};
    std::uint64_t field_revision{1U};
    std::uint64_t world_field_revision{0U};
    std::uint32_t max_source_age_ticks{120U};
    float confidence_decay_per_tick{0.995F};
    float minimum_confidence{0.0F};
    std::optional<InfluenceSourceType> layer_type{};
    bool enabled{true};

    [[nodiscard]] bool valid() const noexcept;
};

struct InfluenceFieldRevision final {
    std::uint64_t field_revision{0U};
    std::uint64_t world_field_revision{0U};
    foundation::SimulationTick evaluation_tick{};
    foundation::SimulationTick source_snapshot_tick{};
    std::uint32_t accepted_source_count{0U};
};

class InfluenceField final {
public:
    // CPU reference implementation. One result represents one faction and
    // optional source type layer; callers create separate layers as needed.
    [[nodiscard]] static foundation::Result<InfluenceField, foundation::Error> compute(
        InfluenceFieldSpec spec, std::span<const InfluenceSource> sources);

    [[nodiscard]] std::uint32_t width() const noexcept { return spec_.width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return spec_.height; }
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const InfluenceFieldSpec& spec() const noexcept { return spec_; }
    [[nodiscard]] const InfluenceFieldRevision& revision() const noexcept { return revision_; }
    [[nodiscard]] std::span<const float> values() const noexcept { return values_; }

    // Coordinates are row-major (y * width + x). Callers must pass an in-range
    // cell, matching the other native dense-field APIs.
    [[nodiscard]] float at(std::uint32_t x, std::uint32_t y) const noexcept {
        return values_[static_cast<std::size_t>(y) * spec_.width + x];
    }

    [[nodiscard]] foundation::Result<float, foundation::Error> sample(
        foundation::Vec2 world) const;

private:
    InfluenceFieldSpec spec_{};
    InfluenceFieldRevision revision_{};
    std::uint32_t version_{InfluenceFieldVersion};
    std::vector<float> values_{};
};

// Exposed for CPU/GPU conformance tests and for implementations that need to
// evaluate a single source without constructing a complete field.
[[nodiscard]] float influenceSmoothstep01(float value) noexcept;

[[nodiscard]] float influenceContribution(const InfluenceSource& source,
                                          foundation::Vec2 cell_center,
                                          const InfluenceFieldSpec& spec) noexcept;

} // namespace genomes::combat
