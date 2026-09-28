#include <genomes/foundation/WallClock.hpp>

namespace genomes::foundation {

WallClock::TimePoint WallClock::now() noexcept {
    return SteadyClock::now();
}

} // namespace genomes::foundation
