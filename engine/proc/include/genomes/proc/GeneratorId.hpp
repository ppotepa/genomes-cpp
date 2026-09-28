#pragma once

#include <genomes/foundation/StrongId.hpp>
#include <genomes/foundation/Types.hpp>

#include <string_view>

namespace genomes::proc {

struct GeneratorIdTag;
using GeneratorId = foundation::StrongId<GeneratorIdTag>;

[[nodiscard]] constexpr GeneratorId generatorId(std::string_view name) noexcept {
    return GeneratorId(foundation::stable_id(name));
}

} // namespace genomes::proc
