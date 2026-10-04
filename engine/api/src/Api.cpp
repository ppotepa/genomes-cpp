#include <genomes/api/Api.hpp>

#include <array>
#include <bit>
#include <limits>

namespace genomes::api {

namespace {

[[nodiscard]] foundation::Result<void, foundation::Error> duplicateError() {
    return foundation::Result<void, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument, "duplicate API operation"});
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (std::size_t index = 0U; index < sizeof(value); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

void appendU64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (std::size_t index = 0U; index < sizeof(value); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

void appendFloat(std::vector<std::uint8_t>& bytes, float value) {
    appendU32(bytes, std::bit_cast<std::uint32_t>(value));
}

} // namespace

std::vector<std::uint8_t> encodeSnapshot(
    foundation::SimulationTick tick, std::uint64_t scene_epoch,
    std::uint64_t semantic_hash, std::span<const SnapshotEntity> entities) {
    // GSNP, format 1, followed by fixed-width little-endian fields. No C++
    // object layout or padding crosses the API boundary.
    std::vector<std::uint8_t> bytes;
    bytes.reserve(40U + entities.size() * 36U);
    bytes.insert(bytes.end(), {'G', 'S', 'N', 'P'});
    bytes.push_back(1U);
    bytes.push_back(0U);
    bytes.push_back(0U);
    bytes.push_back(0U);
    appendU64(bytes, tick.value);
    appendU64(bytes, scene_epoch);
    appendU64(bytes, semantic_hash);
    appendU32(bytes, static_cast<std::uint32_t>(entities.size()));
    for (const SnapshotEntity& entity : entities) {
        appendU64(bytes, entity.entity);
        appendFloat(bytes, entity.position_x);
        appendFloat(bytes, entity.position_y);
        appendFloat(bytes, entity.position_z);
        appendFloat(bytes, entity.velocity_x);
        appendFloat(bytes, entity.velocity_y);
        appendFloat(bytes, entity.velocity_z);
        appendFloat(bytes, entity.heading);
        appendU32(bytes, entity.flags);
    }
    return bytes;
}

foundation::Result<void, foundation::Error> ModuleRegistry::declareCommand(
    ModuleId module, ApiOperationDescriptor descriptor) {
    if (module == 0 || descriptor.id == 0 ||
        std::any_of(commands_.begin(), commands_.end(), [&](const auto& item) {
            return item.first == module && item.second.id == descriptor.id;
        })) {
        return duplicateError();
    }
    commands_.emplace_back(module, std::move(descriptor));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> ModuleRegistry::declareQuery(
    ModuleId module, ApiOperationDescriptor descriptor) {
    if (module == 0 || descriptor.id == 0 ||
        std::any_of(queries_.begin(), queries_.end(), [&](const auto& item) {
            return item.first == module && item.second.id == descriptor.id;
        })) {
        return duplicateError();
    }
    queries_.emplace_back(module, std::move(descriptor));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> ModuleRegistry::declareSystem(
    ModuleId module, ApiSystemDescriptor descriptor) {
    if (module == 0 || !descriptor.valid() ||
        std::any_of(systems_.begin(), systems_.end(), [&](const auto& item) {
            return item.second.id == descriptor.id;
        })) {
        return duplicateError();
    }
    systems_.emplace_back(module, std::move(descriptor));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> ModuleRegistry::declareResourceRead(
    ModuleId module, CapabilityId resource) {
    if (module == 0 || resource == 0 ||
        std::any_of(resource_reads_.begin(), resource_reads_.end(),
                    [&](const auto& item) { return item == std::pair{module, resource}; })) {
        return duplicateError();
    }
    resource_reads_.emplace_back(module, resource);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> ModuleRegistry::declareResourceWrite(
    ModuleId module, CapabilityId resource) {
    if (module == 0 || resource == 0 ||
        std::any_of(resource_writes_.begin(), resource_writes_.end(),
                    [&](const auto& item) { return item == std::pair{module, resource}; })) {
        return duplicateError();
    }
    resource_writes_.emplace_back(module, resource);
    return foundation::Result<void, foundation::Error>::success();
}

const ApiOperationDescriptor* ModuleRegistry::findCommand(
    ModuleId module, ApiId id) const noexcept {
    const auto found = std::find_if(commands_.begin(), commands_.end(),
                                    [module, id](const auto& item) {
                                        return item.first == module && item.second.id == id;
                                    });
    return found == commands_.end() ? nullptr : &found->second;
}

const ApiOperationDescriptor* ModuleRegistry::findQuery(
    ModuleId module, ApiId id) const noexcept {
    const auto found = std::find_if(queries_.begin(), queries_.end(),
                                    [module, id](const auto& item) {
                                        return item.first == module && item.second.id == id;
                                    });
    return found == queries_.end() ? nullptr : &found->second;
}

const ApiSystemDescriptor* ModuleRegistry::findSystem(ApiId id) const noexcept {
    const auto found = std::find_if(systems_.begin(), systems_.end(),
                                    [id](const auto& item) {
                                        return item.second.id == id;
                                    });
    return found == systems_.end() ? nullptr : &found->second;
}

foundation::Result<void, foundation::Error> ModuleHost::registerModule(
    ModuleDescriptor descriptor, Registration registration) {
    if (frozen_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "module registry is frozen"});
    }
    if (descriptor.id == 0 || !registration ||
        std::any_of(entries_.begin(), entries_.end(), [&](const Entry& entry) {
            return entry.descriptor.id == descriptor.id;
        })) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid or duplicate module"});
    }
    entries_.push_back({std::move(descriptor), std::move(registration)});
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> ModuleHost::finalize() {
    if (frozen_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "module registry is already frozen"});
    }
    const auto find = [&](ModuleId id) {
        return std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
            return entry.descriptor.id == id;
        });
    };
    for (const Entry& entry : entries_) {
        for (const ModuleId dependency : entry.descriptor.required_modules) {
            if (find(dependency) == entries_.end()) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::NotFound, "required module is not registered"});
            }
        }
        for (const CapabilityId capability : entry.descriptor.required_capabilities) {
            const bool provided = std::any_of(entries_.begin(), entries_.end(), [&](const Entry& candidate) {
                return std::find(candidate.descriptor.provided_capabilities.begin(),
                                 candidate.descriptor.provided_capabilities.end(), capability) !=
                       candidate.descriptor.provided_capabilities.end();
            });
            if (!provided) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::NotFound, "required capability is not registered"});
            }
        }
    }

    std::vector<std::pair<ModuleId, std::size_t>> indegrees;
    indegrees.reserve(entries_.size());
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        indegrees.emplace_back(entries_[index].descriptor.id,
                               entries_[index].descriptor.required_modules.size());
    }
    while (load_order_.size() < entries_.size()) {
        std::vector<ModuleId> ready;
        for (const auto& [id, indegree] : indegrees) {
            if (indegree == 0 && std::find(load_order_.begin(), load_order_.end(), id) ==
                                      load_order_.end()) {
                ready.push_back(id);
            }
        }
        if (ready.empty()) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "cyclic module dependency"});
        }
        std::sort(ready.begin(), ready.end());
        const ModuleId selected = ready.front();
        load_order_.push_back(selected);
        for (auto& [id, indegree] : indegrees) {
            if (indegree == 0) {
                continue;
            }
            const Entry& entry = *find(id);
            if (std::find(entry.descriptor.required_modules.begin(),
                          entry.descriptor.required_modules.end(), selected) !=
                entry.descriptor.required_modules.end()) {
                --indegree;
            }
        }
    }

    std::vector<CapabilityId> available_capabilities;
    for (const Entry& entry : entries_) {
        available_capabilities.insert(available_capabilities.end(),
                                      entry.descriptor.provided_capabilities.begin(),
                                      entry.descriptor.provided_capabilities.end());
    }
    std::sort(available_capabilities.begin(), available_capabilities.end());
    available_capabilities.erase(
        std::unique(available_capabilities.begin(), available_capabilities.end()),
        available_capabilities.end());

    for (const ModuleId module : load_order_) {
        Entry& entry = *find(module);
        ModuleContext context(module, registry_, available_capabilities);
        const auto result = entry.registration(registry_, context);
        if (!result) {
            return result;
        }
    }

    // System declarations are a second deterministic graph. Reject dangling
    // edges and cycles before the host becomes immutable; execution layers
    // can therefore consume this registry without inventing a second module
    // ordering policy.
    std::vector<std::pair<ApiId, std::size_t>> system_indegrees;
    system_indegrees.reserve(registry_.systems().size());
    registry_.system_order_.clear();
    for (const auto& [module, system] : registry_.systems()) {
        (void)module;
        system_indegrees.emplace_back(system.id, system.predecessors.size());
        for (const ApiId predecessor : system.predecessors) {
            if (std::none_of(registry_.systems().begin(), registry_.systems().end(),
                             [predecessor](const auto& candidate) {
                                 return candidate.second.id == predecessor;
                             })) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::NotFound, "system predecessor is not registered"});
            }
        }
    }
    std::size_t resolved_systems{0U};
    while (resolved_systems < system_indegrees.size()) {
        std::vector<ApiId> ready;
        for (const auto& [id, indegree] : system_indegrees) {
            if (indegree == 0U) ready.push_back(id);
        }
        if (ready.empty()) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "cyclic system dependency"});
        }
        std::sort(ready.begin(), ready.end());
        const ApiId selected = ready.front();
        registry_.system_order_.push_back(selected);
        for (auto& [id, indegree] : system_indegrees) {
            if (id == selected) {
                indegree = std::numeric_limits<std::size_t>::max();
                ++resolved_systems;
                continue;
            }
            if (indegree == 0U) continue;
            const auto found = std::find_if(registry_.systems().begin(), registry_.systems().end(),
                                            [id](const auto& candidate) {
                                                return candidate.second.id == id;
                                            });
            if (std::find(found->second.predecessors.begin(), found->second.predecessors.end(),
                          selected) != found->second.predecessors.end()) {
                --indegree;
            }
        }
    }
    frozen_ = true;
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::api
