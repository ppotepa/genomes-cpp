#include <genomes/proc/GeneratorRegistry.hpp>

#include <utility>

namespace genomes::proc {

namespace {

using foundation::Error;
using foundation::ErrorCode;

Error invalidDescriptor() noexcept {
    return {ErrorCode::InvalidArgument, "invalid generator descriptor"};
}

} // namespace

foundation::Result<void, Error> GeneratorRegistry::Builder::add(
    GeneratorDescriptor descriptor, GeneratorFunction generate) {
    if (!descriptor.valid() || !generate) {
        return foundation::Result<void, Error>::failure(invalidDescriptor());
    }
    for (const GeneratorEntry& entry : entries_) {
        if (entry.descriptor.id == descriptor.id) {
            return foundation::Result<void, Error>::failure(
                {ErrorCode::InvalidState, "duplicate generator id"});
        }
        if (entry.descriptor.name == descriptor.name) {
            return foundation::Result<void, Error>::failure(
                {ErrorCode::InvalidState, "duplicate generator name"});
        }
    }
    entries_.push_back({descriptor, std::move(generate), typeid(void), typeid(void), {}});
    return foundation::Result<void, Error>::success();
}

foundation::Result<void, Error> GeneratorRegistry::Builder::addTypedErased(
    GeneratorDescriptor descriptor,
    std::type_index input_type,
    std::type_index output_type,
    ErasedGeneratorFunction generate) {
    if (!descriptor.valid() || !generate) {
        return foundation::Result<void, Error>::failure(invalidDescriptor());
    }
    for (const GeneratorEntry& entry : entries_) {
        if (entry.descriptor.id == descriptor.id) {
            return foundation::Result<void, Error>::failure(
                {ErrorCode::InvalidState, "duplicate generator id"});
        }
        if (entry.descriptor.name == descriptor.name) {
            return foundation::Result<void, Error>::failure(
                {ErrorCode::InvalidState, "duplicate generator name"});
        }
    }
    entries_.push_back({descriptor, {}, input_type, output_type, std::move(generate)});
    return foundation::Result<void, Error>::success();
}

foundation::Result<GeneratorRegistry, Error> GeneratorRegistry::Builder::freeze() && {
    std::sort(entries_.begin(), entries_.end(), [](const GeneratorEntry& left,
                                                   const GeneratorEntry& right) {
        return left.descriptor.id < right.descriptor.id;
    });
    return foundation::Result<GeneratorRegistry, Error>::success(
        GeneratorRegistry(std::move(entries_)));
}

foundation::Result<void, Error> GeneratorRegistry::run(GeneratorId id,
                                                       GenerationContext& context) const {
    const GeneratorEntry* entry = find(id);
    if (entry == nullptr) {
        return foundation::Result<void, Error>::failure(
            {ErrorCode::NotFound, "generator not registered"});
    }
    if (!entry->generate) {
        return foundation::Result<void, Error>::failure(
            {ErrorCode::InvalidArgument, "typed generator requires a typed request"});
    }
    return entry->generate(context);
}

} // namespace genomes::proc
