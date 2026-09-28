#include <genomes/simulation/SimulationCommand.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_set>
#include <utility>

namespace genomes::simulation {

namespace {

[[nodiscard]] foundation::Error commandError(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] bool sameTickAndSequence(const SimulationCommand* left,
                                       const SimulationCommand* right) noexcept {
    if (left->tick.value != right->tick.value) {
        return left->tick.value < right->tick.value;
    }
    if (left->sequence != right->sequence) {
        return left->sequence < right->sequence;
    }
    return static_cast<std::uint8_t>(left->type) < static_cast<std::uint8_t>(right->type);
}

} // namespace

CommandWriter CommandBuffer::writer(std::uint8_t phase,
                                     SystemId system,
                                     std::uint64_t source,
                                     foundation::SimulationTick tick) noexcept {
    return CommandWriter{*this, phase, system, source, tick};
}

SimulationCommand& CommandBuffer::append(SimulationCommand command) {
    commands_.push_back(std::move(command));
    return commands_.back();
}

SimulationCommand& CommandWriter::append(SimulationCommand command) {
    return buffer_->append(std::move(command));
}

CreateToken CommandWriter::create(ArchetypeKey key,
                                   std::span<const ComponentPayload> initial) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::CreateEntity;
    command.create_key = std::move(key);
    command.components.assign(initial.begin(), initial.end());
    const CreateToken token{command.sequence};
    append(std::move(command));
    return token;
}

void CommandWriter::destroy(EntityId entity) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::DestroyEntity;
    command.target = target(entity);
    append(std::move(command));
}

void CommandWriter::destroy(CreateToken token) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::DestroyEntity;
    command.target = target(token);
    append(std::move(command));
}

void CommandWriter::add(EntityId entity,
                        ComponentTypeId component,
                        std::span<const std::byte> value) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::AddComponent;
    command.target = target(entity);
    command.component = component;
    command.value.assign(value.begin(), value.end());
    append(std::move(command));
}

void CommandWriter::add(CreateToken token,
                        ComponentTypeId component,
                        std::span<const std::byte> value) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::AddComponent;
    command.target = target(token);
    command.component = component;
    command.value.assign(value.begin(), value.end());
    append(std::move(command));
}

void CommandWriter::remove(EntityId entity, ComponentTypeId component) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::RemoveComponent;
    command.target = target(entity);
    command.component = component;
    append(std::move(command));
}

void CommandWriter::remove(CreateToken token, ComponentTypeId component) {
    SimulationCommand command{};
    command.tick = tick_;
    command.sequence = nextKey();
    command.type = SimulationCommandType::RemoveComponent;
    command.target = target(token);
    command.component = component;
    append(std::move(command));
}

foundation::Result<CommandCommitResult, foundation::Error> CommandCommitter::commit(
    WorldEcs& world,
    std::span<CommandBuffer* const> buffers) const {
    std::vector<const SimulationCommand*> ordered;
    for (const CommandBuffer* buffer : buffers) {
        if (buffer == nullptr) {
            return foundation::Result<CommandCommitResult, foundation::Error>::failure(
                commandError("null command buffer"));
        }
        for (const SimulationCommand& command : buffer->commands()) {
            ordered.push_back(&command);
        }
    }
    std::stable_sort(ordered.begin(), ordered.end(), sameTickAndSequence);

    std::map<CreateToken, EntityId> created;
    std::unordered_set<std::uint64_t> destroyed;
    CommandCommitResult result{};

    const auto resolve = [&created](const EntityTarget& target) -> std::optional<EntityId> {
        if (!target.is_token) {
            return target.entity;
        }
        const auto iterator = created.find(target.token);
        return iterator == created.end() ? std::nullopt
                                         : std::optional<EntityId>{iterator->second};
    };

    for (const SimulationCommand* command : ordered) {
        if (command == nullptr) {
            ++result.rejected;
            continue;
        }
        switch (command->type) {
        case SimulationCommandType::CreateEntity: {
            if (!command->create_key.valid()) {
                ++result.rejected;
                break;
            }
            std::vector<ComponentInit> initial;
            initial.reserve(command->components.size());
            bool valid_payload = true;
            for (const ComponentPayload& payload : command->components) {
                const ComponentTypeInfo* type = world.componentType(payload.id);
                if (type == nullptr || payload.bytes.size() != type->size) {
                    valid_payload = false;
                    break;
                }
                initial.push_back({payload.id, payload.bytes.data()});
            }
            if (!valid_payload) {
                ++result.rejected;
                break;
            }
            const auto entity = world.create(command->create_key, initial);
            if (!entity) {
                ++result.rejected;
                break;
            }
            const CreateToken token{command->sequence};
            if (!token.valid() || !created.emplace(token, entity.value()).second) {
                (void)world.destroy(entity.value());
                ++result.rejected;
                break;
            }
            ++result.created;
            ++result.applied;
            break;
        }
        case SimulationCommandType::DestroyEntity: {
            const std::optional<EntityId> entity = resolve(command->target);
            if (!entity || !world.contains(*entity) || destroyed.contains(entity->packed())) {
                ++result.rejected;
                break;
            }
            const auto applied = world.destroy(*entity);
            if (!applied) {
                ++result.rejected;
                break;
            }
            destroyed.insert(entity->packed());
            ++result.destroyed;
            ++result.applied;
            break;
        }
        case SimulationCommandType::AddComponent: {
            const std::optional<EntityId> entity = resolve(command->target);
            const ComponentTypeInfo* type = world.componentType(command->component);
            if (!entity || type == nullptr || command->value.size() != type->size ||
                destroyed.contains(entity->packed())) {
                ++result.rejected;
                break;
            }
            const auto applied = world.addComponent(*entity, command->component,
                                                    command->value.data());
            if (!applied) {
                ++result.rejected;
                break;
            }
            ++result.applied;
            break;
        }
        case SimulationCommandType::RemoveComponent: {
            const std::optional<EntityId> entity = resolve(command->target);
            if (!entity || destroyed.contains(entity->packed())) {
                ++result.rejected;
                break;
            }
            const auto applied = world.removeComponent(*entity, command->component);
            if (!applied) {
                ++result.rejected;
                break;
            }
            ++result.applied;
            break;
        }
        }
    }

    for (CommandBuffer* buffer : buffers) {
        buffer->clear();
    }
    return foundation::Result<CommandCommitResult, foundation::Error>::success(result);
}

} // namespace genomes::simulation
