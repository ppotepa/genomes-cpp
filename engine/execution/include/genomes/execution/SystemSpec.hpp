#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/jobs/SchedulerTypes.hpp>

#include <algorithm>
#include <vector>

namespace genomes::execution {

using SystemId = foundation::StableId;
using AccessKey = foundation::StableId;

// Canonical semantic access declaration shared by module registration and
// simulation execution. It describes data hazards only; ownership remains in
// the domain that provides the actual system callback.
struct SystemAccess final {
    std::vector<AccessKey> reads;
    std::vector<AccessKey> writes;
    std::vector<AccessKey> resource_reads;
    std::vector<AccessKey> resource_writes;

    [[nodiscard]] bool valid() const noexcept {
        const auto valid_keys = [](const std::vector<AccessKey>& values) noexcept {
            return std::all_of(values.begin(), values.end(),
                               [](AccessKey key) { return key != 0U; });
        };
        return valid_keys(reads) && valid_keys(writes) &&
               valid_keys(resource_reads) && valid_keys(resource_writes);
    }
};

// One canonical execution-facing system declaration. Higher-level domains may
// extend it with cadence, phases and callbacks, but scheduling dependencies,
// access hazards and lane ownership must compile down to this type.
struct SystemSpec {
    SystemId id{0};
    std::vector<SystemId> predecessors;
    SystemAccess access{};
    jobs::ExecutionLane lane{jobs::ExecutionLane::Worker};
    bool deterministic{true};

    [[nodiscard]] bool valid() const noexcept {
        return id != 0U && access.valid() &&
               std::all_of(predecessors.begin(), predecessors.end(),
                           [this](SystemId predecessor) {
                               return predecessor != 0U && predecessor != id;
                           });
    }
};

} // namespace genomes::execution
