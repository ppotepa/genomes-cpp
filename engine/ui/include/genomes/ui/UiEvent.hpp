#pragma once

#include <genomes/ui/UiDataModel.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace genomes::ui {

enum class UiEventPhase : std::uint8_t { Input, Change, Click, Submit };

struct UiEvent final {
    std::uint64_t route_id{0};
    std::uint64_t route_revision{0};
    std::string control;
    // Stable data-model key supplied explicitly by data-field.  This is
    // deliberately independent from the command id in `control`.
    std::string field;
    UiEventPhase phase{UiEventPhase::Click};
    UiScalar value{};
    std::vector<std::pair<std::string, UiScalar>> arguments;
};

struct UiCommand final {
    std::string id;
    UiScalar value{};
    std::vector<std::pair<std::string, UiScalar>> arguments;
};

} // namespace genomes::ui
