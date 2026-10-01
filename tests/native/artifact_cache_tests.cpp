#include <genomes/proc/ArtifactCache.hpp>

#include <cassert>
#include <memory>

int main() {
    using namespace genomes;
    proc::ArtifactCache cache{{.byte_budget = 8U}};
    const proc::ArtifactKey first_key{.namespace_id = foundation::stable_id("cache.first")};
    const proc::ArtifactKey second_key{.namespace_id = foundation::stable_id("cache.second")};
    const proc::ArtifactKey oversize_key{.namespace_id = foundation::stable_id("cache.oversize")};

    auto first = std::make_shared<const int>(11);
    cache.store(first_key, first, {.retained_bytes = 8U});
    assert(cache.find<int>(first_key) == first);
    assert(cache.stats().shared_bytes == 8U);

    const auto second = std::make_shared<const int>(22);
    cache.store(second_key, second, {.retained_bytes = 8U});
    assert(cache.find<int>(first_key) == nullptr);
    assert(cache.find<int>(second_key) == second);
    assert(*first == 11);
    assert(cache.stats().retained_bytes == 8U);
    assert(cache.stats().externally_pinned_bytes == 8U);

    first.reset();
    assert(cache.stats().externally_pinned_bytes == 0U);
    const auto oversize = std::make_shared<const int>(33);
    cache.store(oversize_key, oversize, {.retained_bytes = 9U});
    assert(cache.find<int>(oversize_key) == nullptr);
    assert(*oversize == 33);

    cache.clear();
    assert(cache.stats().retained_bytes == 0U);
    assert(cache.stats().externally_pinned_bytes == 8U);
    return 0;
}
