#include <genomes/render/RenderExtraction.hpp>

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

namespace genomes::render {

namespace {

[[nodiscard]] foundation::Error extractionError(const char* message) noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] bool samePresentation(const RenderInstance& current,
                                    const RenderInstance& previous) noexcept {
    if (current.revision != 0 && current.revision == previous.revision) {
        return true;
    }
    return current == previous;
}

} // namespace

foundation::Result<RenderExtraction, foundation::Error> RenderExtractor::extract(
    const PresentationSnapshot& snapshot) {
    std::unordered_set<foundation::StableId> seen;
    seen.reserve(snapshot.instances.size());
    for (const RenderInstance& instance : snapshot.instances) {
        if (instance.object_id == 0 || !seen.insert(instance.object_id).second) {
            return foundation::Result<RenderExtraction, foundation::Error>::failure(
                extractionError("presentation snapshot contains invalid or duplicate instance"));
        }
    }

    if (serial_ == std::numeric_limits<std::uint64_t>::max()) {
        serial_ = 0;
        for (auto& [id, entry] : previous_) {
            (void)id;
            entry.last_seen = 0;
        }
    }
    const std::uint64_t serial = ++serial_;
    RenderExtraction extraction{};
    extraction.frame_number = snapshot.frame_number;
    extraction.simulation_tick = snapshot.simulation_tick;
    extraction.interpolation_alpha = snapshot.interpolation_alpha;
    extraction.changes.reserve(snapshot.instances.size());

    for (const RenderInstance& instance : snapshot.instances) {
        auto iterator = previous_.find(instance.object_id);
        if (iterator == previous_.end()) {
            previous_.emplace(instance.object_id, Entry{instance, serial});
            extraction.changes.push_back(
                {RenderChangeKind::Added, instance.object_id, instance});
            continue;
        }

        Entry& entry = iterator->second;
        if (!samePresentation(instance, entry.instance)) {
            extraction.changes.push_back(
                {RenderChangeKind::Updated, instance.object_id, instance});
        }
        entry.instance = instance;
        entry.last_seen = serial;
    }

    std::vector<foundation::StableId> removed;
    removed.reserve(previous_.size());
    for (const auto& [id, entry] : previous_) {
        if (entry.last_seen != serial) {
            removed.push_back(id);
        }
    }
    std::sort(removed.begin(), removed.end());
    for (const foundation::StableId id : removed) {
        const auto iterator = previous_.find(id);
        if (iterator == previous_.end()) {
            continue;
        }
        extraction.changes.push_back({RenderChangeKind::Removed, id,
                                      iterator->second.instance});
        previous_.erase(iterator);
    }
    return foundation::Result<RenderExtraction, foundation::Error>::success(
        std::move(extraction));
}

void RenderExtractor::reset() noexcept {
    previous_.clear();
    serial_ = 0;
}

} // namespace genomes::render
