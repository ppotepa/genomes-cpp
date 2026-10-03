#pragma once

#include <genomes/foundation/PublishedSnapshotExchange.hpp>
#include <genomes/render/RenderTypes.hpp>

#include <vector>

namespace genomes::render {

struct PoseSnapshot final {
    foundation::SnapshotMetadata metadata{};
    std::vector<SkinnedBonePalette> palettes;

    void clear() {
        metadata = {};
        palettes.clear();
    }
};

using PoseSnapshotExchange = foundation::PublishedSnapshotExchange<PoseSnapshot>;

} // namespace genomes::render
