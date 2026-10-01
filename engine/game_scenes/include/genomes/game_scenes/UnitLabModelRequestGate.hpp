#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <optional>

namespace genomes::game_scenes {

struct UnitLabModelRequestToken final {
    std::uint64_t revision{0U};
    foundation::StableId request_key{0U};

    friend bool operator==(const UnitLabModelRequestToken&, const UnitLabModelRequestToken&) =
        default;
    [[nodiscard]] explicit operator bool() const noexcept { return revision != 0U; }
};

struct UnitLabModelRequestSubmission final {
    UnitLabModelRequestToken token{};
    bool queued{false};
};

enum class UnitLabModelRequestAction : std::uint8_t {
    Ignore,
    Publish,
    StartPending,
};

struct UnitLabModelRequestCompletion final {
    UnitLabModelRequestAction action{UnitLabModelRequestAction::Ignore};
    UnitLabModelRequestToken next{};
};

// Serializes Unit Lab's latest-wins policy independently from worker timing.
// There is at most one active token and one replaceable pending token. A
// completion either publishes that exact request, promotes the newest pending
// request, or is ignored as stale/canceled.
class UnitLabModelRequestGate final {
public:
    [[nodiscard]] UnitLabModelRequestSubmission submit(
        foundation::StableId request_key) noexcept {
        const UnitLabModelRequestToken token{++next_revision_, request_key};
        if (active_) {
            pending_ = token;
            return {token, true};
        }
        active_ = token;
        return {token, false};
    }

    [[nodiscard]] UnitLabModelRequestCompletion complete(
        UnitLabModelRequestToken token) noexcept {
        if (!active_ || *active_ != token) {
            return {};
        }
        if (pending_) {
            active_ = pending_;
            pending_.reset();
            return {UnitLabModelRequestAction::StartPending, *active_};
        }
        active_.reset();
        return {UnitLabModelRequestAction::Publish, {}};
    }

    void cancel() noexcept {
        active_.reset();
        pending_.reset();
    }

    [[nodiscard]] bool hasActive() const noexcept { return active_.has_value(); }
    [[nodiscard]] bool hasPending() const noexcept { return pending_.has_value(); }
    [[nodiscard]] std::optional<UnitLabModelRequestToken> active() const noexcept {
        return active_;
    }

private:
    std::uint64_t next_revision_{0U};
    std::optional<UnitLabModelRequestToken> active_;
    std::optional<UnitLabModelRequestToken> pending_;
};

} // namespace genomes::game_scenes
