#include <genomes/render/RenderCapabilities.hpp>
#include <genomes/render/RenderHandles.hpp>
#include <genomes/render/ResourceDesc.hpp>

#include <cassert>

int main() {
    using namespace genomes;

    render::RenderResourcePool<render::BufferTag, int> pool;
    const auto first = pool.create(7);
    assert(first);
    assert(pool.contains(first.value()));
    assert(*pool.get(first.value()) == 7);

    const auto released = pool.destroy(first.value());
    assert(released && released.value() == 7);
    assert(!pool.contains(first.value()));
    assert(!pool.destroy(first.value()));

    const auto reused = pool.create(9);
    assert(reused && reused.value().index == first.value().index);
    assert(reused.value().generation != first.value().generation);

    render::DeferredReleaseQueue<int> deferred;
    deferred.enqueue(4, 4);
    deferred.enqueue(8, 8);
    int retired_sum = 0;
    assert(deferred.retire(5, [&retired_sum](int&& value) { retired_sum += value; }) == 1);
    assert(retired_sum == 4);
    assert(deferred.pending() == 1);
    assert(deferred.retire(8, [&retired_sum](int&& value) { retired_sum += value; }) == 1);
    assert(retired_sum == 12);

    assert((render::BufferDesc{1024, render::ResourceUsage::Storage, true}.valid()));
    assert((render::TextureDesc{64, 64, 1, 1, render::ResourceUsage::RenderTarget}.valid()));
    render::RenderCapabilities capabilities{};
    capabilities.resource_indexing = true;
    assert(capabilities.resource_indexing);
    return 0;
}
