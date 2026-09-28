#include <genomes/navigation/FlowField.hpp>

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using genomes::navigation::FlowFieldBuilder;
    using genomes::navigation::FlowFieldCache;
    using genomes::navigation::FlowFieldDescriptor;
    using genomes::navigation::FlowFieldKey;

    const FlowFieldDescriptor descriptor{
        FlowFieldKey{7U, 11U, 13U, 5U, 5U}, {24U}};
    std::vector<std::uint8_t> blocked(25U, 0U);
    blocked[12U] = 1U;
    const auto built = FlowFieldBuilder::build(descriptor, blocked);
    assert(built);
    const auto& field = built.value();
    assert(field.validFor(7U));
    assert(!field.validFor(8U));
    const auto origin = field.sample(0U, 0U);
    assert(origin.reachable && origin.integration_cost == 8U);
    assert(origin.direction.x == 1 && origin.direction.z == 0);
    assert(field.sample(2U, 2U).reachable == false);
    assert(field.sample(4U, 4U).goal);

    FlowFieldCache cache;
    assert(cache.insert(field));
    assert(cache.size() == 1U);
    assert(cache.find(descriptor.key) != nullptr);
    assert(cache.invalidateNavigationRevision(8U) == 1U);
    assert(cache.find(descriptor.key) == nullptr);

    std::vector<std::uint8_t> isolated(9U, 0U);
    isolated[5U] = 1U;
    isolated[7U] = 1U;
    const FlowFieldDescriptor unreachable{
        FlowFieldKey{9U, 11U, 17U, 3U, 3U}, {8U}};
    const auto no_path = FlowFieldBuilder::build(unreachable, isolated);
    assert(no_path);
    assert(!no_path.value().sample(0U, 0U).reachable);
    assert(no_path.value().sample(2U, 2U).goal);

    assert(!FlowFieldBuilder::build(
        FlowFieldDescriptor{FlowFieldKey{1U, 2U, 3U, 2U, 2U}, {4U, 4U}}, {}, {}));
    return 0;
}
