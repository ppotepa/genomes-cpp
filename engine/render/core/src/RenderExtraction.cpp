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

SnapshotExchange::WriteLease::WriteLease(WriteLease&& other) noexcept
    : owner_{other.owner_}, index_{other.index_} {
    other.owner_ = nullptr;
}

SnapshotExchange::WriteLease& SnapshotExchange::WriteLease::operator=(
    WriteLease&& other) noexcept {
    if (this != &other) {
        if (owner_ != nullptr) {
            owner_->abandonWrite(index_);
        }
        owner_ = other.owner_;
        index_ = other.index_;
        other.owner_ = nullptr;
    }
    return *this;
}

SnapshotExchange::WriteLease::~WriteLease() {
    if (owner_ != nullptr) {
        owner_->abandonWrite(index_);
    }
}

PresentationSnapshot& SnapshotExchange::WriteLease::snapshot() noexcept {
    return owner_->slots_[index_].snapshot;
}

const PresentationSnapshot& SnapshotExchange::WriteLease::snapshot() const noexcept {
    return owner_->slots_[index_].snapshot;
}

SnapshotExchange::ReadLease::ReadLease(ReadLease&& other) noexcept
    : owner_{other.owner_}, index_{other.index_} {
    other.owner_ = nullptr;
}

SnapshotExchange::ReadLease& SnapshotExchange::ReadLease::operator=(ReadLease&& other) noexcept {
    if (this != &other) {
        if (owner_ != nullptr) {
            owner_->releaseRead(index_);
        }
        owner_ = other.owner_;
        index_ = other.index_;
        other.owner_ = nullptr;
    }
    return *this;
}

SnapshotExchange::ReadLease::~ReadLease() {
    if (owner_ != nullptr) {
        owner_->releaseRead(index_);
    }
}

const PresentationSnapshot& SnapshotExchange::ReadLease::snapshot() const noexcept {
    return owner_->slots_[index_].snapshot;
}

SnapshotExchange::SnapshotExchange(std::uint32_t slot_count) {
    slot_count = std::clamp<std::uint32_t>(slot_count, 2, 3);
    slots_.resize(slot_count);
}

foundation::Result<SnapshotExchange::WriteLease, foundation::Error>
SnapshotExchange::acquireWrite() noexcept {
    std::lock_guard lock{mutex_};
    for (std::size_t offset = 0; offset < slots_.size(); ++offset) {
        const std::size_t index = (write_cursor_ + offset) % slots_.size();
        if (slots_[index].state != SnapshotSlotState::Free) {
            continue;
        }
        slots_[index].state = SnapshotSlotState::Writing;
        write_cursor_ = (index + 1) % slots_.size();
        slots_[index].snapshot.clear();
        return foundation::Result<WriteLease, foundation::Error>::success(
            WriteLease{this, index});
    }
    return foundation::Result<WriteLease, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidState, "presentation snapshot exchange has no free slot"});
}

foundation::Result<SnapshotExchange::ReadLease, foundation::Error>
SnapshotExchange::acquireLatestRead() noexcept {
    std::lock_guard lock{mutex_};
    if (latest_published_ == InvalidSlot ||
        slots_[latest_published_].state != SnapshotSlotState::Published) {
        return foundation::Result<ReadLease, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "presentation snapshot has no readable version"});
    }
    slots_[latest_published_].state = SnapshotSlotState::Reading;
    return foundation::Result<ReadLease, foundation::Error>::success(
        ReadLease{this, latest_published_});
}

foundation::Result<void, foundation::Error> SnapshotExchange::publish(WriteLease&& lease) noexcept {
    std::lock_guard lock{mutex_};
    if (lease.owner_ != this || lease.index_ >= slots_.size() ||
        slots_[lease.index_].state != SnapshotSlotState::Writing) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid presentation write lease"});
    }
    if (latest_published_ != InvalidSlot && latest_published_ != lease.index_ &&
        slots_[latest_published_].state == SnapshotSlotState::Published) {
        slots_[latest_published_].state = SnapshotSlotState::Free;
    }
    slots_[lease.index_].state = SnapshotSlotState::Published;
    latest_published_ = lease.index_;
    ++published_serial_;
    lease.owner_ = nullptr;
    return foundation::Result<void, foundation::Error>::success();
}

std::uint64_t SnapshotExchange::publishedSerial() const noexcept {
    std::lock_guard lock{mutex_};
    return published_serial_;
}

SnapshotSlotState SnapshotExchange::state(std::size_t index) const noexcept {
    std::lock_guard lock{mutex_};
    return index < slots_.size() ? slots_[index].state : SnapshotSlotState::Free;
}

void SnapshotExchange::abandonWrite(std::size_t index) noexcept {
    std::lock_guard lock{mutex_};
    if (index < slots_.size() && slots_[index].state == SnapshotSlotState::Writing) {
        slots_[index].state = SnapshotSlotState::Free;
    }
}

void SnapshotExchange::releaseRead(std::size_t index) const noexcept {
    std::lock_guard lock{mutex_};
    if (index >= slots_.size() || slots_[index].state != SnapshotSlotState::Reading) {
        return;
    }
    slots_[index].state = latest_published_ == index ? SnapshotSlotState::Published
                                                      : SnapshotSlotState::Free;
}

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

    for (auto iterator = previous_.begin(); iterator != previous_.end();) {
        if (iterator->second.last_seen == serial) {
            ++iterator;
            continue;
        }
        extraction.changes.push_back({RenderChangeKind::Removed, iterator->first,
                                      iterator->second.instance});
        iterator = previous_.erase(iterator);
    }
    return foundation::Result<RenderExtraction, foundation::Error>::success(
        std::move(extraction));
}

void RenderExtractor::reset() noexcept {
    previous_.clear();
    serial_ = 0;
}

} // namespace genomes::render
