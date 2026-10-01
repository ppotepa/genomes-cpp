#pragma once

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <compare>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

// Domains provide already-resolved, canonical field values. This function
// makes field ordering non-semantic and keeps the three configuration hash
// spaces deliberately distinct.
struct CanonicalConfigField final {
    std::string_view name;
    std::uint64_t canonical_value{0U};
};

[[nodiscard]] inline std::uint64_t canonicalConfigFingerprint(
    std::string_view domain,
    std::span<const CanonicalConfigField> fields) {
    std::vector<CanonicalConfigField> ordered{fields.begin(), fields.end()};
    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.name < right.name;
    });
    std::uint64_t hash = stableHashString(domain);
    for (const auto& field : ordered) {
        hash = stableHashCombine(hash, stableHashString(field.name));
        hash = stableHashCombine(hash, field.canonical_value);
    }
    return hash;
}

[[nodiscard]] inline SimConfigHash makeSimConfigHash(
    std::string_view domain,
    std::span<const CanonicalConfigField> fields) {
    return {canonicalConfigFingerprint("simulation:" + std::string{domain}, fields)};
}

[[nodiscard]] inline PresentationConfigHash makePresentationConfigHash(
    std::string_view domain,
    std::span<const CanonicalConfigField> fields) {
    return {canonicalConfigFingerprint("presentation:" + std::string{domain}, fields)};
}

[[nodiscard]] inline ExecutionProfileHash makeExecutionProfileHash(
    std::string_view domain,
    std::span<const CanonicalConfigField> fields) {
    return {canonicalConfigFingerprint("execution:" + std::string{domain}, fields)};
}

} // namespace genomes::foundation
