#pragma once

#include <genomes/proc/ProceduralRuntime.hpp>

#include <utility>

namespace genomes::proc {

// Non-owning procedural capability. Consumers can request deterministic
// generation and inspect the frozen registry/telemetry, but do not own the
// runtime, its scheduler or cache lifetime.
class GenerationClient final {
public:
    GenerationClient() = default;
    explicit GenerationClient(ProceduralRuntime& runtime) noexcept : runtime_(&runtime) {}
    explicit GenerationClient(ProceduralRuntime* runtime) noexcept : runtime_(runtime) {}

    [[nodiscard]] bool valid() const noexcept { return runtime_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] bool hasGenerator(GeneratorId id) const noexcept {
        return runtime_ != nullptr && runtime_->registry().find(id) != nullptr;
    }

    [[nodiscard]] const GeneratorRegistry& registry() const noexcept {
        static const GeneratorRegistry empty{};
        return runtime_ != nullptr ? runtime_->registry() : empty;
    }

    [[nodiscard]] ProceduralRuntimeTelemetry telemetry() const noexcept {
        return runtime_ != nullptr ? runtime_->telemetry() : ProceduralRuntimeTelemetry{};
    }

    template <class Input, class Output>
    [[nodiscard]] GenerationTicket<Output> request(
        GenerationRequest<Input, Output> request,
        GenerationChannel* channel = nullptr) const {
        return runtime_ != nullptr
                   ? runtime_->request(std::move(request), channel)
                   : GenerationTicket<Output>{};
    }

    template <class Output>
    [[nodiscard]] GenerationTicket<Output> requestStage(
        GeneratorId stage,
        SeedPath seed_path,
        GenerationOptions options,
        std::function<foundation::Result<std::shared_ptr<const Output>, foundation::Error>(
            GenerationContext&)> execute,
        GenerationChannel* channel = nullptr) const {
        return runtime_ != nullptr
                   ? runtime_->requestStage<Output>(stage, std::move(seed_path), options,
                                                    std::move(execute), channel)
                   : GenerationTicket<Output>{};
    }

    template <class Input, class Output>
    [[nodiscard]] foundation::Result<std::shared_ptr<const Output>, foundation::Error>
    generateInline(const GenerationRequest<Input, Output>& request) const {
        if (runtime_ == nullptr) {
            return foundation::Result<std::shared_ptr<const Output>,
                                      foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "procedural generation capability is unavailable"});
        }
        return runtime_->generateInline(request);
    }

    template <class Input, class Output>
    [[nodiscard]] foundation::Result<std::shared_ptr<const Output>, foundation::Error>
    generateInline(GenerationRequest<Input, Output> request,
                   GenerationContext& parent_context) const {
        if (runtime_ == nullptr) {
            return foundation::Result<std::shared_ptr<const Output>,
                                      foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "procedural generation capability is unavailable"});
        }
        return runtime_->generateInline(std::move(request), parent_context);
    }

private:
    ProceduralRuntime* runtime_{nullptr};
};

} // namespace genomes::proc
