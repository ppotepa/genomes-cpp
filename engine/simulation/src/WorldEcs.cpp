#include <genomes/simulation/WorldEcs.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace genomes::simulation {

namespace {

[[nodiscard]] foundation::Error ecsError(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] ArchetypeKey without(const ArchetypeKey& key, ComponentTypeId id) {
    ArchetypeKey result = key;
    result.components.erase(std::remove(result.components.begin(), result.components.end(), id),
                            result.components.end());
    return result;
}

} // namespace

foundation::Result<void, foundation::Error> WorldEcs::registerComponent(
    ComponentTypeInfo type) {
    if (frozen_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "component registry is already frozen"});
    }
    if (!type.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            ecsError("invalid component type metadata"));
    }
    const auto [iterator, inserted] = component_types_.emplace(type.id, type);
    if (!inserted) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "duplicate component type id"});
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WorldEcs::freeze() {
    if (component_types_.empty()) {
        // An empty registry is valid for tagless entities, but the transition
        // is still explicit so later registration cannot invalidate queries.
        frozen_ = true;
        return foundation::Result<void, foundation::Error>::success();
    }
    frozen_ = true;
    return foundation::Result<void, foundation::Error>::success();
}

void WorldEcs::clear() noexcept {
    archetype_lookup_.clear();
    archetypes_.clear();
    locations_.clear();
    free_indices_.clear();
    live_entities_ = 0;
    ++revision_;
}

foundation::Error WorldEcs::error(const char* message) const {
    return {foundation::ErrorCode::InvalidState, message};
}

bool WorldEcs::validKey(const ArchetypeKey& key) const noexcept {
    if (!key.valid()) {
        return false;
    }
    return std::all_of(key.components.begin(), key.components.end(), [this](ComponentTypeId id) {
        return component_types_.find(id) != component_types_.end();
    });
}

foundation::Result<std::size_t, foundation::Error> WorldEcs::getOrCreateArchetype(
    const ArchetypeKey& key) {
    const auto existing = archetype_lookup_.find(key);
    if (existing != archetype_lookup_.end()) {
        return foundation::Result<std::size_t, foundation::Error>::success(existing->second);
    }
    std::vector<ComponentTypeInfo> types;
    types.reserve(key.components.size());
    for (const ComponentTypeId id : key.components) {
        const auto type = component_types_.find(id);
        if (type == component_types_.end()) {
            return foundation::Result<std::size_t, foundation::Error>::failure(
                {foundation::ErrorCode::NotFound, "component type is not registered"});
        }
        types.push_back(type->second);
    }
    auto archetype = Archetype::create(key, types);
    if (!archetype) {
        return foundation::Result<std::size_t, foundation::Error>::failure(archetype.error());
    }
    const std::size_t index = archetypes_.size();
    archetypes_.push_back(std::move(archetype.value()));
    archetype_lookup_.emplace(archetypes_[index].key(), index);
    ++revision_;
    return foundation::Result<std::size_t, foundation::Error>::success(index);
}

std::size_t WorldEcs::allocateEntitySlot() {
    if (!free_indices_.empty()) {
        const std::uint32_t index = free_indices_.back();
        free_indices_.pop_back();
        return index;
    }
    if (locations_.size() >= foundation::Handle<EntityTag>::InvalidIndex) {
        return std::numeric_limits<std::size_t>::max();
    }
    locations_.push_back({1, {}, {}, {}, false});
    return locations_.size() - 1;
}

void WorldEcs::updateLocation(EntityId entity,
                              std::size_t archetype,
                              ChunkRow row) {
    if (!entity.isValid() || entity.index >= locations_.size()) {
        return;
    }
    EntityLocation& location = locations_[entity.index];
    location.generation = entity.generation;
    location.archetype = static_cast<std::uint32_t>(archetype);
    location.chunk = row.chunk;
    location.row = row.row;
    location.alive = true;
}

foundation::Result<EntityId, foundation::Error> WorldEcs::create(
    ArchetypeKey key,
    std::span<const ComponentInit> initial) {
    if (!frozen_) {
        const auto freeze_result = freeze();
        if (!freeze_result) {
            return foundation::Result<EntityId, foundation::Error>::failure(freeze_result.error());
        }
    }
    if (!validKey(key)) {
        return foundation::Result<EntityId, foundation::Error>::failure(
            ecsError("invalid or unregistered archetype key"));
    }
    for (const ComponentInit& value : initial) {
        if (value.id == 0 || value.value == nullptr || !key.contains(value.id)) {
            return foundation::Result<EntityId, foundation::Error>::failure(
                ecsError("component initialization does not match archetype"));
        }
    }
    auto archetype = getOrCreateArchetype(key);
    if (!archetype) {
        return foundation::Result<EntityId, foundation::Error>::failure(archetype.error());
    }
    const std::size_t slot = allocateEntitySlot();
    if (slot == std::numeric_limits<std::size_t>::max()) {
        return foundation::Result<EntityId, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "entity capacity exhausted"});
    }
    const EntityId entity{static_cast<std::uint32_t>(slot), locations_[slot].generation};
    Archetype& destination = archetypes_[archetype.value()];
    const ChunkRow row = destination.append(entity);
    if (!row.valid()) {
        free_indices_.push_back(static_cast<std::uint32_t>(slot));
        return foundation::Result<EntityId, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "could not allocate archetype row"});
    }
    for (const ComponentInit& value : initial) {
        destination.chunk(row.chunk).copyAssignRaw(value.id, row.row, value.value);
    }
    updateLocation(entity, archetype.value(), row);
    ++live_entities_;
    return foundation::Result<EntityId, foundation::Error>::success(entity);
}

bool WorldEcs::contains(EntityId entity) const noexcept {
    return entity.isValid() && entity.index < locations_.size() &&
           locations_[entity.index].alive &&
           locations_[entity.index].generation == entity.generation;
}

const EntityLocation* WorldEcs::location(EntityId entity) const noexcept {
    return contains(entity) ? &locations_[entity.index] : nullptr;
}

const ComponentTypeInfo* WorldEcs::componentType(ComponentTypeId id) const noexcept {
    const auto iterator = component_types_.find(id);
    return iterator == component_types_.end() ? nullptr : &iterator->second;
}

void* WorldEcs::component(EntityId entity, ComponentTypeId id) noexcept {
    const EntityLocation* entity_location = location(entity);
    if (entity_location == nullptr || entity_location->archetype >= archetypes_.size()) {
        return nullptr;
    }
    return archetypes_[entity_location->archetype]
        .chunk(entity_location->chunk)
        .component(id, entity_location->row);
}

const void* WorldEcs::component(EntityId entity, ComponentTypeId id) const noexcept {
    const EntityLocation* entity_location = location(entity);
    if (entity_location == nullptr || entity_location->archetype >= archetypes_.size()) {
        return nullptr;
    }
    return archetypes_[entity_location->archetype]
        .chunk(entity_location->chunk)
        .component(id, entity_location->row);
}

foundation::Result<void, foundation::Error> WorldEcs::destroy(EntityId entity) {
    if (!contains(entity)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "stale or unknown entity"});
    }
    const EntityLocation old = locations_[entity.index];
    Archetype& source = archetypes_[old.archetype];
    const EntityId moved = source.removeSwap({old.chunk, old.row});
    locations_[entity.index].alive = false;
    ++locations_[entity.index].generation;
    if (locations_[entity.index].generation == 0) {
        locations_[entity.index].generation = 1;
    }
    locations_[entity.index].archetype = foundation::Handle<EntityTag>::InvalidIndex;
    locations_[entity.index].chunk = foundation::Handle<EntityTag>::InvalidIndex;
    locations_[entity.index].row = foundation::Handle<EntityTag>::InvalidIndex;
    free_indices_.push_back(entity.index);
    if (moved.isValid()) {
        updateLocation(moved, old.archetype, {old.chunk, old.row});
    }
    --live_entities_;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WorldEcs::addComponent(EntityId entity,
                                                                   ComponentTypeId id,
                                                                   const void* initial) {
    if (!contains(entity)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "stale or unknown entity"});
    }
    const auto type = component_types_.find(id);
    if (type == component_types_.end()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "component type is not registered"});
    }
    const EntityLocation old = locations_[entity.index];
    const ArchetypeKey source_key = archetypes_[old.archetype].key();
    if (source_key.contains(id)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "entity already has component"});
    }
    ArchetypeKey target_key = source_key;
    target_key.components.push_back(id);
    std::sort(target_key.components.begin(), target_key.components.end());
    auto target_index = getOrCreateArchetype(target_key);
    if (!target_index) {
        return foundation::Result<void, foundation::Error>::failure(target_index.error());
    }
    Archetype& destination = archetypes_[target_index.value()];
    const ChunkRow destination_row = destination.append(entity);
    if (!destination_row.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "could not allocate target archetype row"});
    }
    Archetype& source = archetypes_[old.archetype];
    for (const ComponentTypeId common : source_key.components) {
        destination.chunk(destination_row.chunk).copyAssignFrom(
            source.chunk(old.chunk), common, destination_row.row, old.row);
    }
    if (initial != nullptr) {
        destination.chunk(destination_row.chunk).copyAssignRaw(id, destination_row.row, initial);
    }
    const EntityId moved = source.removeSwap({old.chunk, old.row});
    if (moved.isValid()) {
        updateLocation(moved, old.archetype, {old.chunk, old.row});
    }
    updateLocation(entity, target_index.value(), destination_row);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WorldEcs::removeComponent(EntityId entity,
                                                                      ComponentTypeId id) {
    if (!contains(entity)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "stale or unknown entity"});
    }
    const EntityLocation old = locations_[entity.index];
    const ArchetypeKey source_key = archetypes_[old.archetype].key();
    if (!source_key.contains(id)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "entity does not have component"});
    }
    const ArchetypeKey target_key = without(source_key, id);
    auto target_index = getOrCreateArchetype(target_key);
    if (!target_index) {
        return foundation::Result<void, foundation::Error>::failure(target_index.error());
    }
    Archetype& destination = archetypes_[target_index.value()];
    const ChunkRow destination_row = destination.append(entity);
    if (!destination_row.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "could not allocate target archetype row"});
    }
    Archetype& source = archetypes_[old.archetype];
    for (const ComponentTypeId common : target_key.components) {
        destination.chunk(destination_row.chunk).copyAssignFrom(
            source.chunk(old.chunk), common, destination_row.row, old.row);
    }
    const EntityId moved = source.removeSwap({old.chunk, old.row});
    if (moved.isValid()) {
        updateLocation(moved, old.archetype, {old.chunk, old.row});
    }
    updateLocation(entity, target_index.value(), destination_row);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<QueryPlan, foundation::Error> WorldEcs::compileQuery(
    const QueryDescription& description) const {
    const auto valid_query_keys = [this](const std::vector<ComponentTypeId>& keys) {
        return std::all_of(keys.begin(), keys.end(), [this](ComponentTypeId id) {
            return id != 0 && component_types_.find(id) != component_types_.end();
        });
    };
    if (!valid_query_keys(description.required) || !valid_query_keys(description.optional) ||
        !valid_query_keys(description.excluded)) {
        return foundation::Result<QueryPlan, foundation::Error>::failure(
            ecsError("query references an unregistered component"));
    }
    QueryPlan plan{};
    plan.archetype_revision = revision_;
    for (std::size_t index = 0; index < archetypes_.size(); ++index) {
        const ArchetypeKey& key = archetypes_[index].key();
        if (!std::all_of(description.required.begin(), description.required.end(),
                         [&key](ComponentTypeId id) { return key.contains(id); }) ||
            std::any_of(description.excluded.begin(), description.excluded.end(),
                        [&key](ComponentTypeId id) { return key.contains(id); })) {
            continue;
        }
        plan.archetypes.push_back(index);
    }
    return foundation::Result<QueryPlan, foundation::Error>::success(std::move(plan));
}

} // namespace genomes::simulation
