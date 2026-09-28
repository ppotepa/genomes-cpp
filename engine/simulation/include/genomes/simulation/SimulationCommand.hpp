#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/simulation/SystemId.hpp>
#include <genomes/simulation/WorldEcs.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

namespace genomes::simulation {

enum class SimulationCommandType : std::uint8_t {
    CreateEntity,
    DestroyEntity,
    AddComponent,
    RemoveComponent,
};

struct SemanticSequenceKey final {
    std::uint8_t phase{0};
    SystemId system{0};
    std::uint64_t source{0};
    std::uint64_t local_ordinal{0};

    friend constexpr auto operator<=>(const SemanticSequenceKey&,
                                      const SemanticSequenceKey&) noexcept = default;
};

struct CreateToken final {
    SemanticSequenceKey key{};

    [[nodiscard]] bool valid() const noexcept {
        return key.system != 0 || key.source != 0 || key.local_ordinal != 0;
    }

    friend constexpr auto operator<=>(const CreateToken&, const CreateToken&) noexcept = default;
};

struct EntityTarget final {
    EntityId entity{};
    CreateToken token{};
    bool is_token{false};

    [[nodiscard]] static EntityTarget direct(EntityId value) noexcept {
        return {value, {}, false};
    }
    [[nodiscard]] static EntityTarget fromToken(CreateToken value) noexcept {
        return {{}, value, true};
    }
};

struct ComponentPayload final {
    ComponentTypeId id{0};
    std::vector<std::byte> bytes;

    [[nodiscard]] bool valid() const noexcept { return id != 0 && !bytes.empty(); }
};

struct SimulationCommand final {
    foundation::SimulationTick tick{};
    SemanticSequenceKey sequence{};
    SimulationCommandType type{SimulationCommandType::DestroyEntity};
    EntityTarget target{};
    ArchetypeKey create_key{};
    std::vector<ComponentPayload> components;
    ComponentTypeId component{0};
    std::vector<std::byte> value;
};

class CommandWriter;

// Append-only and intentionally non-thread-safe: each worker/system invocation
// owns one buffer. Buffers are gathered by CommandCommitter after a phase.
class CommandBuffer final {
public:
    [[nodiscard]] CommandWriter writer(std::uint8_t phase,
                                       SystemId system,
                                       std::uint64_t source,
                                       foundation::SimulationTick tick) noexcept;

    void clear() noexcept { commands_.clear(); }
    [[nodiscard]] const std::vector<SimulationCommand>& commands() const noexcept {
        return commands_;
    }
    [[nodiscard]] std::size_t size() const noexcept { return commands_.size(); }

private:
    friend class CommandWriter;
    SimulationCommand& append(SimulationCommand command);

    std::vector<SimulationCommand> commands_;
};

// One buffer is assigned to one system invocation. Different workers can
// append concurrently because they never share the same CommandBuffer.
class CommandBufferSet final {
public:
    void reset(std::size_t count) {
        buffers_.clear();
        buffers_.resize(count);
        pointers_.clear();
        pointers_.reserve(buffers_.size());
        for (CommandBuffer& buffer : buffers_) {
            pointers_.push_back(&buffer);
        }
    }

    [[nodiscard]] CommandBuffer& at(std::size_t index) noexcept { return buffers_[index]; }
    [[nodiscard]] std::span<CommandBuffer* const> buffers() noexcept {
        return {pointers_.data(), pointers_.size()};
    }
    [[nodiscard]] std::size_t size() const noexcept { return buffers_.size(); }

private:
    std::vector<CommandBuffer> buffers_;
    std::vector<CommandBuffer*> pointers_;
};

class CommandWriter final {
public:
    CommandWriter() = default;

    [[nodiscard]] CreateToken create(ArchetypeKey key,
                                     std::span<const ComponentPayload> initial = {});
    void destroy(EntityId entity);
    void destroy(CreateToken token);
    void add(EntityId entity, ComponentTypeId component, std::span<const std::byte> value);
    void add(CreateToken token, ComponentTypeId component, std::span<const std::byte> value);
    void remove(EntityId entity, ComponentTypeId component);
    void remove(CreateToken token, ComponentTypeId component);

    template <class T>
    void add(EntityId entity, ComponentTypeId component, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "typed command payloads require trivially copyable components");
        add(entity, component,
            std::as_bytes(std::span<const T>{std::addressof(value), 1}));
    }

    template <class T>
    void add(CreateToken token, ComponentTypeId component, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "typed command payloads require trivially copyable components");
        add(token, component,
            std::as_bytes(std::span<const T>{std::addressof(value), 1}));
    }

private:
    friend class CommandBuffer;
    CommandWriter(CommandBuffer& buffer,
                  std::uint8_t phase,
                  SystemId system,
                  std::uint64_t source,
                  foundation::SimulationTick tick) noexcept
        : buffer_{&buffer}, phase_{phase}, system_{system}, source_{source}, tick_{tick} {}

    [[nodiscard]] SemanticSequenceKey nextKey() noexcept {
        return {phase_, system_, source_, local_ordinal_++};
    }
    SimulationCommand& append(SimulationCommand command);
    static EntityTarget target(EntityId entity) noexcept { return EntityTarget::direct(entity); }
    static EntityTarget target(CreateToken token) noexcept {
        return EntityTarget::fromToken(token);
    }

    CommandBuffer* buffer_{nullptr};
    std::uint8_t phase_{0};
    SystemId system_{0};
    std::uint64_t source_{0};
    std::uint64_t local_ordinal_{0};
    foundation::SimulationTick tick_{};
};

struct CommandCommitResult final {
    std::size_t applied{0};
    std::size_t rejected{0};
    std::size_t created{0};
    std::size_t destroyed{0};
};

class CommandCommitter final {
public:
    [[nodiscard]] foundation::Result<CommandCommitResult, foundation::Error> commit(
        WorldEcs& world,
        std::span<CommandBuffer* const> buffers) const;
};

} // namespace genomes::simulation
