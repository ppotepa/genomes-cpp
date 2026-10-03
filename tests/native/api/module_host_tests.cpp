#include <genomes/api/Api.hpp>

#include <array>
#include <cassert>
#include <span>
#include <utility>
#include <vector>

namespace {

void deterministicModuleOrderAndFreeze() {
    genomes::api::ModuleHost host;
    std::vector<genomes::api::ModuleId> registered;
    const auto module_a = genomes::foundation::stable_id("module.a");
    const auto module_b = genomes::foundation::stable_id("module.b");
    const auto module_core = genomes::foundation::stable_id("module.core");

    auto register_module = [&](genomes::api::ModuleId id,
                                std::vector<genomes::api::ModuleId> dependencies) {
        return host.registerModule(
            {.id = id, .version = {}, .required_modules = std::move(dependencies),
             .required_capabilities = {}, .provided_capabilities = {}},
            [&, id](genomes::api::ModuleRegistry& registry,
                    genomes::api::ModuleContext&) {
                registered.push_back(id);
                return registry.declareCommand(id, {.id = id,
                                                    .schema_version = {},
                                                    .arguments = {},
                                                    .permission = 0,
                                                    .lane = genomes::jobs::ExecutionLane::Worker,
                                                    .deterministic = true});
            });
    };

    assert(register_module(module_b, {module_core}));
    assert(register_module(module_a, {module_core}));
    assert(register_module(module_core, {}));
    assert(host.finalize());
    assert((host.loadOrder() == std::vector<genomes::api::ModuleId>{module_core, module_a,
                                                                      module_b}));
    assert(registered == host.loadOrder());
    assert(host.frozen());
    assert(!host.registerModule({.id = 99, .version = {}, .required_modules = {},
                                 .required_capabilities = {}, .provided_capabilities = {}}, {}));
}

void missingAndCyclicDependenciesAreRejected() {
    genomes::api::ModuleHost missing;
    assert(missing.registerModule({.id = 1, .version = {}, .required_modules = {2},
                                   .required_capabilities = {}, .provided_capabilities = {}},
                                  [](auto&, auto&) { return genomes::foundation::Result<void, genomes::foundation::Error>::success(); }));
    assert(!missing.finalize());

    genomes::api::ModuleHost cyclic;
    assert(cyclic.registerModule({.id = 1, .version = {}, .required_modules = {2},
                                  .required_capabilities = {}, .provided_capabilities = {}},
                                 [](auto&, auto&) { return genomes::foundation::Result<void, genomes::foundation::Error>::success(); }));
    assert(cyclic.registerModule({.id = 2, .version = {}, .required_modules = {1},
                                  .required_capabilities = {}, .provided_capabilities = {}},
                                 [](auto&, auto&) { return genomes::foundation::Result<void, genomes::foundation::Error>::success(); }));
    assert(!cyclic.finalize());
}

void commandOrderingAndEncodingAreValueOnly() {
    const auto value = genomes::api::EncodedValue::string("world");
    assert(value.valid());
    assert(value.type == genomes::api::ValueType::String);
    assert(value.bytes.size() == 5U);

    genomes::api::CommandEnvelope low{1, 2, {}, {4}, 10, 2, 0, value};
    genomes::api::CommandEnvelope high = low;
    high.sequence = 3;
    assert(low < high);

    genomes::api::CommandQueue queue;
    const auto first = queue.enqueue({1, 2, {}, {5}, 7, 0, 4, value}, {0});
    const auto second = queue.enqueue({1, 2, {}, {5}, 3, 0, 1, value}, {0});
    assert(first.accepted && second.accepted);
    const auto commands = queue.take({5});
    assert(commands.size() == 2U);
    assert(commands[0].source == 3U);
    assert(commands[1].source == 7U);
    assert(queue.take({6}).empty());

    const auto wire = genomes::api::encodeSnapshot(
        {7U}, 3U, 11U,
        std::span<const genomes::api::SnapshotEntity>{
            std::array<genomes::api::SnapshotEntity, 1>{{{42U, 1.0F, 2.0F, 3.0F,
                                                            4.0F, 5.0F, 6.0F, 0.5F, 7U}}}});
    assert(wire.size() == 76U);
    assert(wire[0] == 'G' && wire[1] == 'S' && wire[2] == 'N' && wire[3] == 'P');
}

void textAdapterUsesRegisteredSchema() {
    genomes::api::ModuleHost host;
    const auto core = genomes::foundation::stable_id("core");
    assert(host.registerModule(
        {.id = core, .version = {}, .required_modules = {},
         .required_capabilities = {}, .provided_capabilities = {}},
        [](auto& registry, auto&) {
            genomes::api::ApiOperationDescriptor quit{};
            quit.id = genomes::foundation::stable_id("core.quit");
            return registry.declareCommand(genomes::foundation::stable_id("core"),
                                           std::move(quit));
        }));
    assert(host.finalize());
    genomes::api::TextCommandAdapter adapter(host.registry());
    const auto parsed = adapter.parse("core.quit()", {4U},
                                     genomes::foundation::stable_id("console"));
    assert(parsed && parsed.value().module == core && parsed.value().payload.valid());
}

} // namespace

int main() {
    deterministicModuleOrderAndFreeze();
    missingAndCyclicDependenciesAreRejected();
    commandOrderingAndEncodingAreValueOnly();
    textAdapterUsesRegisteredSchema();
    return 0;
}
