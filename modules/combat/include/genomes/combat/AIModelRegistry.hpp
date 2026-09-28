#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace genomes::combat {

struct Observation;
struct AIState;
struct AIIntent;

using AIModelId = std::uint32_t;
using AIModelFunction = foundation::Result<AIIntent, foundation::Error> (*)(
    const Observation&, const AIState&) noexcept;

struct AIModelDefinition final {
    AIModelId id{0};
    std::string_view name{};
    AIModelFunction evaluate{nullptr};
};

class AIModelRegistry final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> registerModel(
        AIModelDefinition definition);
    [[nodiscard]] const AIModelDefinition* find(AIModelId id) const noexcept;
    [[nodiscard]] const AIModelDefinition* find(std::string_view name) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return models_.size(); }

private:
    std::vector<AIModelDefinition> models_;
};

} // namespace genomes::combat
