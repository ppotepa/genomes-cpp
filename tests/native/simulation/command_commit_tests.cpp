#include <genomes/simulation/SimulationCommand.hpp>

#include <cassert>
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

int main() {
    using namespace genomes;
    simulation::WorldEcs world;
    const auto component = simulation::makeComponentType<std::int32_t>("test.command.value");
    assert(world.registerComponent(component));
    assert(world.freeze());

    simulation::CommandBufferSet buffers;
    buffers.reset(2U);
    const std::int32_t initial_value = 42;
    simulation::ComponentPayload payload{
        component.id,
        std::vector<std::byte>(reinterpret_cast<const std::byte*>(&initial_value),
                               reinterpret_cast<const std::byte*>(&initial_value) +
                                   sizeof(initial_value))};
    const std::array<simulation::ComponentPayload, 1> payloads{payload};
    auto writer = buffers.at(1U).writer(
        1U, foundation::stable_id("test.command.create"), 7U,
        foundation::SimulationTick{1U});
    (void)writer.create(simulation::ArchetypeKey{{component.id}}, payloads);

    simulation::CommandCommitter committer;
    const auto committed = committer.commit(world, buffers.buffers());
    assert(committed);
    assert(committed.value().created == 1U);
    assert(committed.value().applied == 1U);
    assert(buffers.at(1U).size() == 0U);
    assert(world.entityCount() == 1U);

    simulation::EntityId entity{};
    world.forEachEntity([&](simulation::EntityId value) { entity = value; });
    const auto* stored = world.component<std::int32_t>(entity, component.id);
    assert(stored != nullptr && *stored == initial_value);

    // Identical semantic keys must not make commit order depend on buffer
    // collection order. The payload tie-break chooses the same result either
    // way.
    simulation::CommandBuffer left;
    simulation::CommandBuffer right;
    simulation::CommandBuffer left_reversed;
    simulation::CommandBuffer right_reversed;
    const auto emit = [&](simulation::CommandBuffer& buffer, std::int32_t value) {
        const simulation::ComponentPayload value_payload{
            component.id,
            std::vector<std::byte>(reinterpret_cast<const std::byte*>(&value),
                                   reinterpret_cast<const std::byte*>(&value) + sizeof(value))};
        const std::array<simulation::ComponentPayload, 1> values{value_payload};
        auto duplicate_key_writer = buffer.writer(
            1U, foundation::stable_id("test.command.duplicate"), 9U,
            foundation::SimulationTick{2U});
        (void)duplicate_key_writer.create(simulation::ArchetypeKey{{component.id}}, values);
    };
    emit(left, 7);
    emit(right, 99);
    emit(left_reversed, 7);
    emit(right_reversed, 99);
    simulation::WorldEcs first_order;
    simulation::WorldEcs second_order;
    assert(first_order.registerComponent(component));
    assert(second_order.registerComponent(component));
    assert(first_order.freeze());
    assert(second_order.freeze());
    const std::array<simulation::CommandBuffer*, 2> first_buffers{&left, &right};
    const std::array<simulation::CommandBuffer*, 2> second_buffers{&right_reversed,
                                                                     &left_reversed};
    // The duplicate token is rejected deterministically after the payload
    // tie-break, so exactly one create is applied in either collection order.
    assert(committer.commit(first_order, first_buffers).value().created == 1U);
    assert(committer.commit(second_order, second_buffers).value().created == 1U);
    std::vector<std::int32_t> first_values;
    std::vector<std::int32_t> second_values;
    first_order.forEachEntity([&](simulation::EntityId value) {
        first_values.push_back(*first_order.component<std::int32_t>(value, component.id));
    });
    second_order.forEachEntity([&](simulation::EntityId value) {
        second_values.push_back(*second_order.component<std::int32_t>(value, component.id));
    });
    assert(first_values == second_values);
    return 0;
}
