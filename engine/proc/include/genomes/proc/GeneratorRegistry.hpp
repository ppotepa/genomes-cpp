#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GenerationContext.hpp>
#include <genomes/proc/GeneratorDescriptor.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

namespace genomes::proc {

using GeneratorFunction =
    std::function<foundation::Result<void, foundation::Error>(GenerationContext&)>;
using ErasedGeneratorFunction = std::function<
    foundation::Result<std::shared_ptr<const void>, foundation::Error>(
        const void*, GenerationContext&)>;

struct GeneratorEntry final {
    GeneratorDescriptor descriptor;
    GeneratorFunction generate;
    std::type_index input_cpp_type{typeid(void)};
    std::type_index output_cpp_type{typeid(void)};
    ErasedGeneratorFunction generate_typed;
};

class GeneratorRegistry final {
public:
    class Builder final {
    public:
        foundation::Result<void, foundation::Error> add(GeneratorDescriptor descriptor,
                                                         GeneratorFunction generate);
        template <class Input, class Output>
        foundation::Result<void, foundation::Error> addTyped(
            GeneratorDescriptor descriptor,
            std::function<foundation::Result<std::shared_ptr<const Output>, foundation::Error>(
                const Input&, GenerationContext&)> generate) {
            if (!generate) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidArgument, "invalid typed generator"});
            }
            ErasedGeneratorFunction erased =
                [generate = std::move(generate)](const void* input, GenerationContext& context)
                -> foundation::Result<std::shared_ptr<const void>, foundation::Error> {
                    if (input == nullptr) {
                        return foundation::Result<std::shared_ptr<const void>,
                                                  foundation::Error>::failure(
                            {foundation::ErrorCode::InvalidArgument,
                             "typed generator input is null"});
                    }
                    auto result = generate(*static_cast<const Input*>(input), context);
                    if (!result) {
                        return foundation::Result<std::shared_ptr<const void>,
                                                  foundation::Error>::failure(result.error());
                    }
                    return foundation::Result<std::shared_ptr<const void>,
                                              foundation::Error>::success(
                        std::static_pointer_cast<const void>(result.value()));
                };
            return addTypedErased(std::move(descriptor), std::type_index(typeid(Input)),
                                  std::type_index(typeid(Output)), std::move(erased));
        }
        foundation::Result<GeneratorRegistry, foundation::Error> freeze() &&;

    private:
        foundation::Result<void, foundation::Error> addTypedErased(
            GeneratorDescriptor,
            std::type_index,
            std::type_index,
            ErasedGeneratorFunction);
        std::vector<GeneratorEntry> entries_;
    };

    GeneratorRegistry() = default;

    [[nodiscard]] const GeneratorEntry* find(GeneratorId id) const noexcept {
        const auto iterator = std::lower_bound(
            entries_.begin(), entries_.end(), id,
            [](const GeneratorEntry& entry, GeneratorId value) { return entry.descriptor.id < value; });
        return iterator != entries_.end() && iterator->descriptor.id == id ? &*iterator : nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return entries_.size();
    }

    foundation::Result<void, foundation::Error> run(GeneratorId id,
                                                     GenerationContext& context) const;

private:
    explicit GeneratorRegistry(std::vector<GeneratorEntry> entries)
        : entries_(std::move(entries)) {}

    std::vector<GeneratorEntry> entries_;
};

} // namespace genomes::proc
