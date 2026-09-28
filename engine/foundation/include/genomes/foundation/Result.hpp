#pragma once

#include <cassert>
#include <optional>
#include <utility>
#include <variant>

namespace genomes::foundation {

template <class T, class E>
class Result final {
public:
    static Result success(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    static Result failure(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return storage_.index() == 0;
    }

    explicit operator bool() const noexcept {
        return hasValue();
    }

    T& value() & {
        assert(hasValue());
        return std::get<0>(storage_);
    }

    const T& value() const& {
        assert(hasValue());
        return std::get<0>(storage_);
    }

    T&& value() && {
        assert(hasValue());
        return std::get<0>(std::move(storage_));
    }

    E& error() & {
        assert(!hasValue());
        return std::get<1>(storage_);
    }

    const E& error() const& {
        assert(!hasValue());
        return std::get<1>(storage_);
    }

private:
    template <std::size_t Index, class Value>
    explicit Result(std::in_place_index_t<Index>, Value&& value)
        : storage_(std::in_place_index<Index>, std::forward<Value>(value)) {}

    std::variant<T, E> storage_;
};

template <class E>
class Result<void, E> final {
public:
    static Result success() {
        return Result(true, std::nullopt);
    }

    static Result failure(E error) {
        return Result(false, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return success_;
    }

    explicit operator bool() const noexcept {
        return hasValue();
    }

    E& error() & {
        assert(!hasValue());
        return *error_;
    }

    const E& error() const& {
        assert(!hasValue());
        return *error_;
    }

private:
    Result(bool success, std::optional<E> error)
        : success_(success), error_(std::move(error)) {}

    bool success_{false};
    std::optional<E> error_;
};

} // namespace genomes::foundation
