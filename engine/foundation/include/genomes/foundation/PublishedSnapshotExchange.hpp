#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace genomes::foundation {

enum class SnapshotSlotState : std::uint8_t {
    Free,
    Writing,
    Published,
    Reading,
};

struct SnapshotMetadata final {
    std::uint64_t tick{0};
    std::uint64_t generation{0};
    std::uint64_t scene_epoch{0};
    std::uint64_t revision{0};
};

// Bounded non-blocking hand-off for immutable-after-publish values. A stalled
// reader owns only its slot; producers may publish through the remaining
// slots, or skip a publication when every slot is leased.
template <class T>
class PublishedSnapshotExchange final {
public:
    class WriteLease final {
    public:
        WriteLease(const WriteLease&) = delete;
        WriteLease& operator=(const WriteLease&) = delete;
        WriteLease(WriteLease&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)), index_(other.index_) {}
        WriteLease& operator=(WriteLease&& other) noexcept {
            if (this != &other) {
                if (owner_ != nullptr) {
                    owner_->abandonWrite(index_);
                }
                owner_ = std::exchange(other.owner_, nullptr);
                index_ = other.index_;
            }
            return *this;
        }
        ~WriteLease() {
            if (owner_ != nullptr) {
                owner_->abandonWrite(index_);
            }
        }

        [[nodiscard]] T& snapshot() noexcept { return owner_->slots_[index_].snapshot; }
        [[nodiscard]] const T& snapshot() const noexcept {
            return owner_->slots_[index_].snapshot;
        }
        [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }

    private:
        friend class PublishedSnapshotExchange;
        WriteLease(PublishedSnapshotExchange* owner, std::size_t index) noexcept
            : owner_(owner), index_(index) {}

        PublishedSnapshotExchange* owner_{nullptr};
        std::size_t index_{0};
    };

    class ReadLease final {
    public:
        ReadLease(const ReadLease&) = delete;
        ReadLease& operator=(const ReadLease&) = delete;
        ReadLease(ReadLease&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)), index_(other.index_) {}
        ReadLease& operator=(ReadLease&& other) noexcept {
            if (this != &other) {
                if (owner_ != nullptr) {
                    owner_->releaseRead(index_);
                }
                owner_ = std::exchange(other.owner_, nullptr);
                index_ = other.index_;
            }
            return *this;
        }
        ~ReadLease() {
            if (owner_ != nullptr) {
                owner_->releaseRead(index_);
            }
        }

        [[nodiscard]] const T& snapshot() const noexcept {
            return owner_->slots_[index_].snapshot;
        }
        [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }

    private:
        friend class PublishedSnapshotExchange;
        ReadLease(const PublishedSnapshotExchange* owner, std::size_t index) noexcept
            : owner_(owner), index_(index) {}

        const PublishedSnapshotExchange* owner_{nullptr};
        std::size_t index_{0};
    };

    explicit PublishedSnapshotExchange(std::uint32_t slot_count = 3) {
        slots_.resize(std::clamp<std::uint32_t>(slot_count, 2, 3));
    }

    PublishedSnapshotExchange(const PublishedSnapshotExchange&) = delete;
    PublishedSnapshotExchange& operator=(const PublishedSnapshotExchange&) = delete;

    [[nodiscard]] Result<WriteLease, Error> acquireWrite() noexcept {
        std::lock_guard lock(mutex_);
        for (std::size_t offset = 0; offset < slots_.size(); ++offset) {
            const std::size_t index = (write_cursor_ + offset) % slots_.size();
            if (slots_[index].state != SnapshotSlotState::Free) {
                continue;
            }
            slots_[index].state = SnapshotSlotState::Writing;
            write_cursor_ = (index + 1U) % slots_.size();
            reset(slots_[index].snapshot);
            return Result<WriteLease, Error>::success(WriteLease(this, index));
        }
        return Result<WriteLease, Error>::failure(
            {ErrorCode::InvalidState, "snapshot exchange has no free slot"});
    }

    [[nodiscard]] Result<ReadLease, Error> acquireLatestRead() noexcept {
        std::lock_guard lock(mutex_);
        if (latest_published_ == InvalidSlot ||
            slots_[latest_published_].state != SnapshotSlotState::Published) {
            return Result<ReadLease, Error>::failure(
                {ErrorCode::NotFound, "snapshot exchange has no readable version"});
        }
        slots_[latest_published_].state = SnapshotSlotState::Reading;
        return Result<ReadLease, Error>::success(ReadLease(this, latest_published_));
    }

    [[nodiscard]] Result<void, Error> publish(WriteLease&& lease) noexcept {
        std::lock_guard lock(mutex_);
        if (lease.owner_ != this || lease.index_ >= slots_.size() ||
            slots_[lease.index_].state != SnapshotSlotState::Writing) {
            return Result<void, Error>::failure(
                {ErrorCode::InvalidState, "invalid snapshot write lease"});
        }
        T& snapshot = slots_[lease.index_].snapshot;
        if constexpr (requires(const T& value) { value.scene_epoch; }) {
            if (snapshot.scene_epoch < minimum_scene_epoch_) {
                slots_[lease.index_].state = SnapshotSlotState::Free;
                lease.owner_ = nullptr;
                return Result<void, Error>::failure(
                    {ErrorCode::InvalidState, "stale snapshot scene epoch"});
            }
        } else if constexpr (requires(const T& value) { value.metadata.scene_epoch; }) {
            if (snapshot.metadata.scene_epoch < minimum_scene_epoch_) {
                slots_[lease.index_].state = SnapshotSlotState::Free;
                lease.owner_ = nullptr;
                return Result<void, Error>::failure(
                    {ErrorCode::InvalidState, "stale snapshot scene epoch"});
            }
        }
        // A worker may finish out of order within the same scene. Do not let
        // an older revision replace the currently published consumer state.
        if (latest_published_ != InvalidSlot) {
            bool stale_revision = false;
            if constexpr (requires(const T& value) { value.revision; }) {
                if constexpr (requires(const T& value) { value.scene_epoch; }) {
                    stale_revision = snapshot.scene_epoch ==
                                         slots_[latest_published_].snapshot.scene_epoch &&
                                     snapshot.revision <
                                         slots_[latest_published_].snapshot.revision;
                }
            } else if constexpr (requires(const T& value) { value.metadata.revision; }) {
                if constexpr (requires(const T& value) { value.metadata.scene_epoch; }) {
                    stale_revision = snapshot.metadata.scene_epoch ==
                                         slots_[latest_published_].snapshot.metadata.scene_epoch &&
                                     snapshot.metadata.revision <
                                         slots_[latest_published_].snapshot.metadata.revision;
                }
            }
            if (stale_revision) {
                slots_[lease.index_].state = SnapshotSlotState::Free;
                lease.owner_ = nullptr;
                return Result<void, Error>::failure(
                    {ErrorCode::InvalidState, "stale snapshot revision"});
            }
        }
        const std::uint64_t generation = published_serial_ + 1U;
        if constexpr (requires(T& value) { value.snapshot_generation; }) {
            snapshot.snapshot_generation = generation;
        } else if constexpr (requires(T& value) { value.metadata.generation; }) {
            snapshot.metadata.generation = generation;
        }
        if (latest_published_ != InvalidSlot && latest_published_ != lease.index_ &&
            slots_[latest_published_].state == SnapshotSlotState::Published) {
            slots_[latest_published_].state = SnapshotSlotState::Free;
        }
        slots_[lease.index_].state = SnapshotSlotState::Published;
        latest_published_ = lease.index_;
        published_serial_ = generation;
        lease.owner_ = nullptr;
        return Result<void, Error>::success();
    }

    void rejectBeforeSceneEpoch(std::uint64_t scene_epoch) noexcept {
        std::lock_guard lock(mutex_);
        minimum_scene_epoch_ = std::max(minimum_scene_epoch_, scene_epoch);
        if constexpr (requires(const T& value) { value.scene_epoch; }) {
            if (latest_published_ != InvalidSlot &&
                slots_[latest_published_].snapshot.scene_epoch < minimum_scene_epoch_) {
                if (slots_[latest_published_].state == SnapshotSlotState::Published) {
                    slots_[latest_published_].state = SnapshotSlotState::Free;
                }
                // A reader may still own the latest slot.  Do not leave the
                // index live: releaseRead() must not resurrect this stale
                // scene as the current publication.
                latest_published_ = InvalidSlot;
            }
        } else if constexpr (requires(const T& value) { value.metadata.scene_epoch; }) {
            if (latest_published_ != InvalidSlot &&
                slots_[latest_published_].snapshot.metadata.scene_epoch < minimum_scene_epoch_) {
                if (slots_[latest_published_].state == SnapshotSlotState::Published) {
                    slots_[latest_published_].state = SnapshotSlotState::Free;
                }
                latest_published_ = InvalidSlot;
            }
        }
    }

    [[nodiscard]] std::size_t slotCount() const noexcept { return slots_.size(); }
    [[nodiscard]] std::uint64_t publishedSerial() const noexcept {
        std::lock_guard lock(mutex_);
        return published_serial_;
    }
    [[nodiscard]] SnapshotSlotState state(std::size_t index) const noexcept {
        std::lock_guard lock(mutex_);
        return index < slots_.size() ? slots_[index].state : SnapshotSlotState::Free;
    }

private:
    struct Slot final {
        T snapshot{};
        SnapshotSlotState state{SnapshotSlotState::Free};
    };

    static void reset(T& value) {
        if constexpr (requires(T& snapshot) { snapshot.clear(); }) {
            value.clear();
        } else {
            value = T{};
        }
    }

    void abandonWrite(std::size_t index) noexcept {
        std::lock_guard lock(mutex_);
        if (index < slots_.size() && slots_[index].state == SnapshotSlotState::Writing) {
            slots_[index].state = SnapshotSlotState::Free;
        }
    }

    void releaseRead(std::size_t index) const noexcept {
        std::lock_guard lock(mutex_);
        if (index >= slots_.size() || slots_[index].state != SnapshotSlotState::Reading) {
            return;
        }
        bool stale = false;
        if constexpr (requires(const T& value) { value.scene_epoch; }) {
            stale = slots_[index].snapshot.scene_epoch < minimum_scene_epoch_;
        } else if constexpr (requires(const T& value) { value.metadata.scene_epoch; }) {
            stale = slots_[index].snapshot.metadata.scene_epoch < minimum_scene_epoch_;
        }
        slots_[index].state = !stale && latest_published_ == index
                                  ? SnapshotSlotState::Published
                                  : SnapshotSlotState::Free;
    }

    static constexpr std::size_t InvalidSlot = static_cast<std::size_t>(-1);

    mutable std::mutex mutex_;
    mutable std::vector<Slot> slots_;
    std::size_t write_cursor_{0};
    std::size_t latest_published_{InvalidSlot};
    std::uint64_t published_serial_{0};
    std::uint64_t minimum_scene_epoch_{0};
};

} // namespace genomes::foundation
