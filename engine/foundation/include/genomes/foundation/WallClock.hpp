#pragma once

#include <genomes/foundation/Time.hpp>

namespace genomes::foundation {

class WallClock final {
public:
    using TimePoint = SteadyClock::time_point;

    [[nodiscard]] static TimePoint now() noexcept;
};

} // namespace genomes::foundation
