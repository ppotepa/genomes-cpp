#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/simulation/Archetype.hpp>
#include <genomes/simulation/Query.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <unordered_map>
#include <vector>

namespace genomes::simulation {

struct ComponentInit final {
    ComponentTypeId id{0};
    const void* value{nullptr};
};

class WorldEcs final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> registerComponent(
        ComponentTypeInfo type);
    [[nodiscard]] foundation::Result<void, foundation::Error> freeze();

    void clear() noexcept;

    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] std::size_t entityCount() const noexcept { return live_entities_; }
    [[nodiscard]] std::size_t archetypeCount() const noexcept { return archetypes_.size(); }
    [[nodiscard]] std::uint64_t archetypeRevision() const noexcept { return revision_; }

    [[nodiscard]] foundation::Result<EntityId, foundation::Error> create(
        ArchetypeKey key,
        std::span<const ComponentInit> initial = {});
    [[nodiscard]] foundation::Result<void, foundation::Error> destroy(EntityId);
    [[nodiscard]] foundation::Result<void, foundation::Error> addComponent(
        EntityId,
        ComponentTypeId,
        const void* initial = nullptr);
    [[nodiscard]] foundation::Result<void, foundation::Error> removeComponent(EntityId,
                                                                                ComponentTypeId);

    [[nodiscard]] bool contains(EntityId) const noexcept;
    [[nodiscard]] const EntityLocation* location(EntityId) const noexcept;
    [[nodiscard]] const ComponentTypeInfo* componentType(ComponentTypeId) const noexcept;
    [[nodiscard]] void* component(EntityId, ComponentTypeId) noexcept;
    [[nodiscard]] const void* component(EntityId, ComponentTypeId) const noexcept;

    template <class Function>
    void forEachEntity(Function&& function) const {
        for (std::size_t index = 0; index < locations_.size(); ++index) {
            const EntityLocation& entity_location = locations_[index];
            if (entity_location.alive) {
                function(EntityId{static_cast<std::uint32_t>(index),
                                  entity_location.generation});
            }
        }
    }

    template <class T>
    [[nodiscard]] T* component(EntityId entity, ComponentTypeId id) noexcept {
        return static_cast<T*>(component(entity, id));
    }

    template <class T>
    [[nodiscard]] const T* component(EntityId entity, ComponentTypeId id) const noexcept {
        return static_cast<const T*>(component(entity, id));
    }

    [[nodiscard]] foundation::Result<QueryPlan, foundation::Error> compileQuery(
        const QueryDescription&) const;

    template <class Function>
    bool forEachChunk(const QueryPlan& plan, Function&& function) const {
        if (plan.archetype_revision != revision_) {
            return false;
        }
        for (const std::size_t archetype_index : plan.archetypes) {
            if (archetype_index >= archetypes_.size()) {
                return false;
            }
            const Archetype& archetype = archetypes_[archetype_index];
            for (std::size_t chunk_index = 0; chunk_index < archetype.chunkCount(); ++chunk_index) {
                function(QueryChunkView{&archetype.chunk(chunk_index)});
            }
        }
        return true;
    }

private:
    [[nodiscard]] foundation::Result<std::size_t, foundation::Error> getOrCreateArchetype(
        const ArchetypeKey&);
    [[nodiscard]] bool validKey(const ArchetypeKey&) const noexcept;
    [[nodiscard]] std::size_t allocateEntitySlot();
    [[nodiscard]] foundation::Error error(const char* message) const;
    void updateLocation(EntityId, std::size_t archetype, ChunkRow);

    std::unordered_map<ComponentTypeId, ComponentTypeInfo> component_types_;
    std::map<ArchetypeKey, std::size_t> archetype_lookup_;
    std::vector<Archetype> archetypes_;
    std::vector<EntityLocation> locations_;
    std::vector<std::uint32_t> free_indices_;
    std::size_t live_entities_{0};
    std::uint64_t revision_{0};
    bool frozen_{false};
};

} // namespace genomes::simulation
