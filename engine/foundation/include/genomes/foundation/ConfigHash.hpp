#pragma once

#include <compare>
#include <cstdint>

namespace genomes::foundation {

// These distinct wrappers prevent simulation, presentation and execution
// fingerprints from being accidentally compared or substituted for one another.
// Values are produced only after a domain has resolved and canonicalized input.
struct SimConfigHash final {
    std::uint64_t value{0U};
    constexpr auto operator<=>(const SimConfigHash&) const = default;
};

struct PresentationConfigHash final {
    std::uint64_t value{0U};
    constexpr auto operator<=>(const PresentationConfigHash&) const = default;
};

struct ExecutionProfileHash final {
    std::uint64_t value{0U};
    constexpr auto operator<=>(const ExecutionProfileHash&) const = default;
};

} // namespace genomes::foundation
