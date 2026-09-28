#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/PipelineAsset.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace genomes::render {

struct ReflectedBinding final {
    std::string name;
    std::uint32_t set{0};
    std::uint32_t binding{0};
    PipelineResourceClass resource_class{PipelineResourceClass::Static};
};

struct ShaderReflection final {
    std::vector<ReflectedBinding> bindings;

    [[nodiscard]] foundation::Result<void, foundation::Error> validate(
        std::span<const PipelineBinding> expected) const {
        for (const PipelineBinding& requirement : expected) {
            const auto iterator = std::find_if(
                bindings.begin(), bindings.end(), [&](const ReflectedBinding& binding) {
                    return binding.name == requirement.name && binding.set == requirement.set &&
                           binding.binding == requirement.binding;
                });
            if (iterator == bindings.end() || iterator->resource_class != requirement.resource_class) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidArgument,
                     "shader reflection does not match pipeline binding contract"});
            }
        }
        return foundation::Result<void, foundation::Error>::success();
    }
};

} // namespace genomes::render
