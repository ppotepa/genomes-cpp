#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/PresentationSnapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace genomes::render {

enum class SnapshotSlotState : std::uint8_t {
    Free,
    Writing,
    Published,
    Reading,
};

// A small non-blocking exchange. Simulation receives Busy when all slots are
// owned; it may drop a presentation frame while authoritative ticks continue.
class SnapshotExchange final {
public:
    class WriteLease final {
    public:
        WriteLease(const WriteLease&) = delete;
        WriteLease& operator=(const WriteLease&) = delete;
        WriteLease(WriteLease&& other) noexcept;
        WriteLease& operator=(WriteLease&& other) noexcept;
        ~WriteLease();

        [[nodiscard]] PresentationSnapshot& snapshot() noexcept;
        [[nodiscard]] const PresentationSnapshot& snapshot() const noexcept;
        [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }

    private:
        friend class SnapshotExchange;
        WriteLease(SnapshotExchange* owner, std::size_t index) noexcept
            : owner_{owner}, index_{index} {}

        SnapshotExchange* owner_{nullptr};
        std::size_t index_{0};
    };

    class ReadLease final {
    public:
        ReadLease(const ReadLease&) = delete;
        ReadLease& operator=(const ReadLease&) = delete;
        ReadLease(ReadLease&& other) noexcept;
        ReadLease& operator=(ReadLease&& other) noexcept;
        ~ReadLease();

        [[nodiscard]] const PresentationSnapshot& snapshot() const noexcept;
        [[nodiscard]] bool valid() const noexcept { return owner_ != nullptr; }

    private:
        friend class SnapshotExchange;
        ReadLease(const SnapshotExchange* owner, std::size_t index) noexcept
            : owner_{owner}, index_{index} {}

        const SnapshotExchange* owner_{nullptr};
        std::size_t index_{0};
    };

    explicit SnapshotExchange(std::uint32_t slot_count = 3);
    ~SnapshotExchange() = default;

    SnapshotExchange(const SnapshotExchange&) = delete;
    SnapshotExchange& operator=(const SnapshotExchange&) = delete;

    [[nodiscard]] foundation::Result<WriteLease, foundation::Error> acquireWrite() noexcept;
    [[nodiscard]] foundation::Result<ReadLease, foundation::Error> acquireLatestRead() noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> publish(WriteLease&&) noexcept;

    [[nodiscard]] std::size_t slotCount() const noexcept { return slots_.size(); }
    [[nodiscard]] std::uint64_t publishedSerial() const noexcept;
    [[nodiscard]] SnapshotSlotState state(std::size_t index) const noexcept;

private:
    struct Slot final {
        PresentationSnapshot snapshot{};
        SnapshotSlotState state{SnapshotSlotState::Free};
    };

    static constexpr std::size_t InvalidSlot = static_cast<std::size_t>(-1);

    void abandonWrite(std::size_t index) noexcept;
    void releaseRead(std::size_t index) const noexcept;

    mutable std::mutex mutex_;
    mutable std::vector<Slot> slots_;
    std::size_t write_cursor_{0};
    std::size_t latest_published_{InvalidSlot};
    std::uint64_t published_serial_{0};
};

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
