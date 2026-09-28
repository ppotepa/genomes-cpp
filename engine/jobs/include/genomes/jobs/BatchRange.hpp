#pragma once

#include <cstddef>

namespace genomes::jobs {

struct BatchRange final {
    std::size_t begin{0};
    std::size_t end{0};
    std::size_t batch_index{0};

    [[nodiscard]] std::size_t size() const noexcept { return end - begin; }
    [[nodiscard]] bool empty() const noexcept { return begin >= end; }
};

} // namespace genomes::jobs
