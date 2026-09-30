#include <genomes/geometry/GeometryRecipe.hpp>

namespace genomes::geometry {
std::uint64_t GeometryRecipe::identity() const noexcept {
    auto hash=foundation::stableHashU64(static_cast<std::uint64_t>(operation));
    hash=foundation::stableHashCombine(hash,foundation::stableHashString(provider_version));
    hash=foundation::stableHashCombine(hash,policy_fingerprint);
    for(const float parameter:parameters) hash=foundation::stableHashCombine(hash,foundation::stableHashFloat(parameter));
    return hash;
}
}
