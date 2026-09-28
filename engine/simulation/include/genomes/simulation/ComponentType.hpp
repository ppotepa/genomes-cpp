#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace genomes::simulation {

using ComponentTypeId = foundation::StableId;

struct ComponentTypeInfo final {
    ComponentTypeId id{0};
    std::size_t size{0};
    std::size_t alignment{1};
    void (*construct)(void*) noexcept{nullptr};
    void (*copy_construct)(void*, const void*) noexcept{nullptr};
    void (*move_construct)(void*, void*) noexcept{nullptr};
    void (*copy_assign)(void*, const void*) noexcept{nullptr};
    void (*move_assign)(void*, void*) noexcept{nullptr};
    void (*destroy)(void*) noexcept{nullptr};

    [[nodiscard]] bool valid() const noexcept {
        return id != 0 && size != 0 && alignment != 0 && construct != nullptr &&
               copy_construct != nullptr && move_construct != nullptr && copy_assign != nullptr &&
               move_assign != nullptr && destroy != nullptr;
    }
};

template <class T>
[[nodiscard]] ComponentTypeInfo makeComponentType(std::string_view stable_name) noexcept {
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>);
    return {
        foundation::stable_id(stable_name),
        sizeof(T),
        alignof(T),
        [](void* destination) noexcept { new (destination) T{}; },
        [](void* destination, const void* source) noexcept {
            new (destination) T(*static_cast<const T*>(source));
        },
        [](void* destination, void* source) noexcept {
            new (destination) T(std::move(*static_cast<T*>(source)));
        },
        [](void* destination, const void* source) noexcept {
            *static_cast<T*>(destination) = *static_cast<const T*>(source);
        },
        [](void* destination, void* source) noexcept {
            *static_cast<T*>(destination) = std::move(*static_cast<T*>(source));
        },
        [](void* value) noexcept { static_cast<T*>(value)->~T(); },
    };
}

} // namespace genomes::simulation
