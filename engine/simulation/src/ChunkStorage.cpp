#include <genomes/simulation/ChunkStorage.hpp>

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace genomes::simulation {

namespace {

[[nodiscard]] constexpr bool isPowerOfTwo(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

[[nodiscard]] constexpr std::size_t alignUp(std::size_t value,
                                            std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] std::size_t layoutBytes(const std::vector<ComponentTypeInfo>& types,
                                      std::size_t capacity) noexcept {
    std::size_t offset = 0;
    for (const ComponentTypeInfo& type : types) {
        offset = alignUp(offset, type.alignment);
        offset += type.size * capacity;
    }
    return offset;
}

[[nodiscard]] std::size_t maxAlignment(const ChunkLayout& layout) noexcept {
    std::size_t result = alignof(std::max_align_t);
    for (const ChunkColumn& column : layout.columns) {
        result = std::max(result, column.type.alignment);
    }
    return result;
}

[[nodiscard]] const ChunkColumn* findColumn(const ChunkLayout& layout,
                                            ComponentTypeId id) noexcept {
    const auto iterator = std::lower_bound(
        layout.columns.begin(), layout.columns.end(), id,
        [](const ChunkColumn& column, ComponentTypeId value) { return column.type.id < value; });
    if (iterator == layout.columns.end() || iterator->type.id != id) {
        return nullptr;
    }
    return &*iterator;
}

[[nodiscard]] foundation::Error storageError(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

} // namespace

const ChunkColumn* ChunkLayout::find(ComponentTypeId id) const noexcept {
    return findColumn(*this, id);
}

foundation::Result<ChunkLayout, foundation::Error> ChunkLayout::build(
    std::span<const ComponentTypeInfo> types,
    std::size_t target_payload_bytes) {
    if (target_payload_bytes < 256 || types.size() > 4096) {
        return foundation::Result<ChunkLayout, foundation::Error>::failure(
            storageError("invalid chunk payload target"));
    }

    std::vector<ComponentTypeInfo> sorted(types.begin(), types.end());
    std::sort(sorted.begin(), sorted.end(), [](const ComponentTypeInfo& left,
                                               const ComponentTypeInfo& right) {
        return left.id < right.id;
    });
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        if (!sorted[index].valid() || !isPowerOfTwo(sorted[index].alignment) ||
            (index > 0 && sorted[index - 1].id == sorted[index].id)) {
            return foundation::Result<ChunkLayout, foundation::Error>::failure(
                storageError("invalid or duplicate component type in chunk layout"));
        }
    }

    std::size_t low = 0;
    std::size_t high = target_payload_bytes;
    while (low < high) {
        const std::size_t middle = low + (high - low + 1) / 2;
        if (layoutBytes(sorted, middle) <= target_payload_bytes) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    if (low == 0 && !sorted.empty()) {
        return foundation::Result<ChunkLayout, foundation::Error>::failure(
            storageError("component columns do not fit in chunk payload"));
    }

    ChunkLayout layout{};
    layout.capacity = sorted.empty()
                          ? std::max<std::size_t>(1, target_payload_bytes / sizeof(EntityId))
                          : low;
    layout.columns.reserve(sorted.size());
    std::size_t offset = 0;
    for (const ComponentTypeInfo& type : sorted) {
        offset = alignUp(offset, type.alignment);
        layout.columns.push_back({type, offset});
        offset += type.size * layout.capacity;
    }
    layout.payload_bytes = offset;
    return foundation::Result<ChunkLayout, foundation::Error>::success(std::move(layout));
}

void ChunkStorage::AlignedDeleter::operator()(std::byte* value) const noexcept {
    if (value != nullptr) {
        ::operator delete(value, std::align_val_t{alignment});
    }
}

ChunkStorage::ChunkStorage(ChunkLayout layout)
    : layout_{std::move(layout)}, entities_(layout_.capacity),
      payload_{nullptr, AlignedDeleter{maxAlignment(layout_)}} {
    if (layout_.payload_bytes != 0) {
        payload_.reset(static_cast<std::byte*>(
            ::operator new(layout_.payload_bytes,
                           std::align_val_t{maxAlignment(layout_)})));
    }
}

ChunkStorage::~ChunkStorage() {
    for (std::uint32_t row = 0; row < size_; ++row) {
        for (const ChunkColumn& column : layout_.columns) {
            destroy(column.type.id, row);
        }
    }
}

ChunkStorage::ChunkStorage(ChunkStorage&& other) noexcept
    : layout_{std::move(other.layout_)},
      entities_{std::move(other.entities_)},
      payload_{other.payload_.release(), other.payload_.get_deleter()},
      size_{other.size_} {
    other.size_ = 0;
}

ChunkStorage& ChunkStorage::operator=(ChunkStorage&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    for (std::uint32_t row = 0; row < size_; ++row) {
        for (const ChunkColumn& column : layout_.columns) {
            destroy(column.type.id, row);
        }
    }
    payload_.reset();
    layout_ = std::move(other.layout_);
    entities_ = std::move(other.entities_);
    payload_ = std::unique_ptr<std::byte[], AlignedDeleter>{other.payload_.release(),
                                                            other.payload_.get_deleter()};
    size_ = other.size_;
    other.size_ = 0;
    return *this;
}

std::uint32_t ChunkStorage::append(EntityId entity) noexcept {
    if (full()) {
        return foundation::Handle<EntityTag>::InvalidIndex;
    }
    const auto row = static_cast<std::uint32_t>(size_++);
    entities_[row] = entity;
    for (const ChunkColumn& column : layout_.columns) {
        column.type.construct(columnAddress(column, row));
    }
    return row;
}

EntityId ChunkStorage::removeSwap(std::uint32_t row) noexcept {
    if (row >= size_) {
        return {};
    }
    const auto last = static_cast<std::uint32_t>(size_ - 1);
    const EntityId moved = row == last ? EntityId{} : entities_[last];
    if (row != last) {
        for (const ChunkColumn& column : layout_.columns) {
            destroy(column.type.id, row);
            moveConstructFrom(*this, column.type.id, row, last);
            destroy(column.type.id, last);
        }
        entities_[row] = entities_[last];
    } else {
        for (const ChunkColumn& column : layout_.columns) {
            destroy(column.type.id, row);
        }
    }
    --size_;
    return moved;
}

void* ChunkStorage::component(ComponentTypeId id, std::uint32_t row) noexcept {
    if (row >= size_) {
        return nullptr;
    }
    const ChunkColumn* column = layout_.find(id);
    return column == nullptr ? nullptr : columnAddress(*column, row);
}

const void* ChunkStorage::component(ComponentTypeId id, std::uint32_t row) const noexcept {
    if (row >= size_) {
        return nullptr;
    }
    const ChunkColumn* column = layout_.find(id);
    return column == nullptr ? nullptr : columnAddress(*column, row);
}

void* ChunkStorage::columnData(ComponentTypeId id) noexcept {
    const ChunkColumn* column = layout_.find(id);
    return column == nullptr ? nullptr : payload_.get() + column->offset;
}

const void* ChunkStorage::columnData(ComponentTypeId id) const noexcept {
    const ChunkColumn* column = layout_.find(id);
    return column == nullptr ? nullptr : payload_.get() + column->offset;
}

std::size_t ChunkStorage::componentStride(ComponentTypeId id) const noexcept {
    const ChunkColumn* column = layout_.find(id);
    return column == nullptr ? 0 : column->type.size;
}

void ChunkStorage::defaultConstruct(ComponentTypeId id, std::uint32_t row) noexcept {
    const ChunkColumn* column = layout_.find(id);
    if (column != nullptr && row < size_) {
        column->type.construct(columnAddress(*column, row));
    }
}

void ChunkStorage::copyConstructFrom(const ChunkStorage& source,
                                     ComponentTypeId id,
                                     std::uint32_t destination_row,
                                     std::uint32_t source_row) noexcept {
    const ChunkColumn* destination_column = layout_.find(id);
    const ChunkColumn* source_column = source.layout_.find(id);
    if (destination_column == nullptr || source_column == nullptr ||
        destination_row >= size_ || source_row >= source.size_) {
        return;
    }
    destination_column->type.copy_construct(
        columnAddress(*destination_column, destination_row),
        source.columnAddress(*source_column, source_row));
}

void ChunkStorage::moveConstructFrom(ChunkStorage& source,
                                     ComponentTypeId id,
                                     std::uint32_t destination_row,
                                     std::uint32_t source_row) noexcept {
    const ChunkColumn* destination_column = layout_.find(id);
    const ChunkColumn* source_column = source.layout_.find(id);
    if (destination_column == nullptr || source_column == nullptr ||
        destination_row >= size_ || source_row >= source.size_) {
        return;
    }
    destination_column->type.move_construct(
        columnAddress(*destination_column, destination_row),
        source.columnAddress(*source_column, source_row));
}

void ChunkStorage::destroy(ComponentTypeId id, std::uint32_t row) noexcept {
    const ChunkColumn* column = layout_.find(id);
    if (column != nullptr && row < layout_.capacity) {
        column->type.destroy(columnAddress(*column, row));
    }
}

void ChunkStorage::copyAssignFrom(const ChunkStorage& source,
                                  ComponentTypeId id,
                                  std::uint32_t destination_row,
                                  std::uint32_t source_row) noexcept {
    const ChunkColumn* destination_column = layout_.find(id);
    const ChunkColumn* source_column = source.layout_.find(id);
    if (destination_column == nullptr || source_column == nullptr ||
        destination_row >= size_ || source_row >= source.size_) {
        return;
    }
    destination_column->type.copy_assign(
        columnAddress(*destination_column, destination_row),
        source.columnAddress(*source_column, source_row));
}

void ChunkStorage::moveAssignFrom(ChunkStorage& source,
                                  ComponentTypeId id,
                                  std::uint32_t destination_row,
                                  std::uint32_t source_row) noexcept {
    const ChunkColumn* destination_column = layout_.find(id);
    const ChunkColumn* source_column = source.layout_.find(id);
    if (destination_column == nullptr || source_column == nullptr ||
        destination_row >= size_ || source_row >= source.size_) {
        return;
    }
    destination_column->type.move_assign(
        columnAddress(*destination_column, destination_row),
        source.columnAddress(*source_column, source_row));
}

void ChunkStorage::copyAssignRaw(ComponentTypeId id,
                                 std::uint32_t destination_row,
                                 const void* source) noexcept {
    const ChunkColumn* column = layout_.find(id);
    if (column != nullptr && source != nullptr && destination_row < size_) {
        column->type.copy_assign(columnAddress(*column, destination_row), source);
    }
}

std::byte* ChunkStorage::columnAddress(const ChunkColumn& column,
                                       std::uint32_t row) noexcept {
    return payload_.get() + column.offset + column.type.size * row;
}

const std::byte* ChunkStorage::columnAddress(const ChunkColumn& column,
                                             std::uint32_t row) const noexcept {
    return payload_.get() + column.offset + column.type.size * row;
}

} // namespace genomes::simulation
