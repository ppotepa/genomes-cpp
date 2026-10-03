#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/PublishedSnapshotExchange.hpp>
#include <genomes/render/PresentationSnapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace genomes::render {

using SnapshotSlotState = foundation::SnapshotSlotState;
using SnapshotExchange = foundation::PublishedSnapshotExchange<PresentationSnapshot>;

enum class RenderChangeKind : std::uint8_t {
    Added,
    Updated,
    Removed,
};

struct RenderChange final {
    RenderChangeKind kind{RenderChangeKind::Updated};
    foundation::StableId semantic_id{0};
    RenderInstance instance{};
};

struct RenderExtraction final {
    std::uint64_t frame_number{0};
    std::uint64_t simulation_tick{0};
    double interpolation_alpha{0.0};
    std::vector<RenderChange> changes;

    [[nodiscard]] bool empty() const noexcept { return changes.empty(); }
};

class RenderExtractor final {
public:
    [[nodiscard]] foundation::Result<RenderExtraction, foundation::Error> extract(
        const PresentationSnapshot& snapshot);

    void reset() noexcept;

    [[nodiscard]] std::size_t trackedCount() const noexcept { return previous_.size(); }
    [[nodiscard]] std::uint64_t extractionSerial() const noexcept { return serial_; }

private:
    struct Entry final {
        RenderInstance instance{};
        std::uint64_t last_seen{0};
    };

    std::unordered_map<foundation::StableId, Entry> previous_;
    std::uint64_t serial_{0};
};

} // namespace genomes::render
