#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GenerationContext.hpp>
#include <genomes/proc/GeneratorDescriptor.hpp>

#include <algorithm>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace genomes::proc {

using GeneratorFunction =
    std::function<foundation::Result<void, foundation::Error>(GenerationContext&)>;

struct GeneratorEntry final {
    GeneratorDescriptor descriptor;
    GeneratorFunction generate;
};

class GeneratorRegistry final {
public:
    class Builder final {
    public:
        foundation::Result<void, foundation::Error> add(GeneratorDescriptor descriptor,
                                                         GeneratorFunction generate);
        foundation::Result<GeneratorRegistry, foundation::Error> freeze() &&;

    private:
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
