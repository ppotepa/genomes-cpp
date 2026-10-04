#include <genomes/runtime/ModuleSystemBridge.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <atomic>
#include <cassert>
#include <thread>
#include <utility>
#include <vector>

namespace {

void registrySpecDrivesExecutionPlan() {
    using namespace genomes;
    api::ModuleHost host;
    const auto core = foundation::stable_id("module.bridge-core");
    const auto domain = foundation::stable_id("module.bridge-domain");
    const auto producer = foundation::stable_id("bridge.producer");
    const auto consumer = foundation::stable_id("bridge.consumer");
    const auto resource = foundation::stable_id("bridge.resource");

    assert(host.registerModule(
        {.id = core, .version = {}, .required_modules = {},
         .required_capabilities = {}, .provided_capabilities = {}},
        [producer, resource](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = producer;
            system.access.writes = {resource};
            system.lane = jobs::ExecutionLane::Worker;
            return registry.declareSystem(context.module(), std::move(system));
        }));
    assert(host.registerModule(
        {.id = domain, .version = {}, .required_modules = {core},
         .required_capabilities = {}, .provided_capabilities = {}},
        [producer, consumer, resource](api::ModuleRegistry& registry,
                                       api::ModuleContext& context) {
            api::ApiSystemDescriptor system{};
            system.id = consumer;
            system.predecessors = {producer};
            system.access.reads = {resource};
            system.lane = jobs::ExecutionLane::Main;
            return registry.declareSystem(context.module(), std::move(system));
        }));
    assert(host.finalize());

    simulation::SystemGraph graph;
    const auto owner = std::this_thread::get_id();
    std::atomic_bool producer_ran{false};
    std::atomic_bool consumer_ran{false};
    const std::vector<runtime::ModuleSystemBinding> bindings{
        {producer, simulation::SystemPhase::Sense, {},
         [&](simulation::SystemContext&) {
             producer_ran.store(true, std::memory_order_release);
         }},
        {consumer, simulation::SystemPhase::Decide, {},
         [&](simulation::SystemContext&) {
             assert(producer_ran.load(std::memory_order_acquire));
             assert(std::this_thread::get_id() == owner);
             consumer_ran.store(true, std::memory_order_release);
         }},
    };

    auto compiled = runtime::ModuleSystemBridge::compile(graph, host.registry(), bindings);
    assert(compiled);
    jobs::JobSystem scheduler(2U);
    const auto result = compiled.value().run({1U}, 1.0 / 60.0, &scheduler);
    assert(result);
    assert(consumer_ran.load(std::memory_order_acquire));
}

void bridgeRejectsIncompleteDependencyClosure() {
    using namespace genomes;
    api::ModuleHost host;
    const auto module = foundation::stable_id("module.bridge-invalid");
    const auto first = foundation::stable_id("bridge.first");
    const auto second = foundation::stable_id("bridge.second");
    assert(host.registerModule(
        {.id = module, .version = {}, .required_modules = {},
         .required_capabilities = {}, .provided_capabilities = {}},
        [first, second](api::ModuleRegistry& registry, api::ModuleContext& context) {
            api::ApiSystemDescriptor producer{};
            producer.id = first;
            auto result = registry.declareSystem(context.module(), std::move(producer));
            if (!result) return result;
            api::ApiSystemDescriptor consumer{};
            consumer.id = second;
            consumer.predecessors = {first};
            return registry.declareSystem(context.module(), std::move(consumer));
        }));
    assert(host.finalize());

    simulation::SystemGraph graph;
    const std::vector<runtime::ModuleSystemBinding> incomplete{
        {second, simulation::SystemPhase::Decide, {},
         [](simulation::SystemContext&) {}},
    };
    const auto result = runtime::ModuleSystemBridge::compile(
        graph, host.registry(), incomplete);
    assert(!result);
    assert(result.error().code == foundation::ErrorCode::InvalidState);
    assert(graph.size() == 0U);
}

} // namespace

int main() {
    registrySpecDrivesExecutionPlan();
    bridgeRejectsIncompleteDependencyClosure();
    return 0;
}
