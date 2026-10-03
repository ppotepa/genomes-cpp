#include <genomes/infantry/InfantryProcedural.hpp>

#include <memory>

namespace genomes::infantry {

foundation::Result<void, foundation::Error> registerInfantryGenerator(
    proc::GeneratorRegistry::Builder& builder) {
    const proc::GeneratorDescriptor descriptor{
        proc::generatorId("infantry.model"),
        "infantry.model",
        {1, 0, 0},
        foundation::stable_id("infantry.model.request"),
        foundation::stable_id("infantry.model.result"),
        true,
        proc::GeneratorExecutionPolicy::Cpu,
        proc::GeneratorCachePolicy::Artifact};
    return builder.addTyped<InfantryModelRequest, InfantryModelCompileResult>(
        descriptor,
        [](const InfantryModelRequest& request,
           proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const InfantryModelCompileResult>,
                                  foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<
                    std::shared_ptr<const InfantryModelCompileResult>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "infantry generation canceled"});
            }
            InfantryModelCompiler compiler;
            auto generated = compiler.compile(request);
            if (!generated) {
                return foundation::Result<
                    std::shared_ptr<const InfantryModelCompileResult>, foundation::Error>::failure(
                    generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<
                    std::shared_ptr<const InfantryModelCompileResult>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "infantry generation canceled"});
            }
            return foundation::Result<
                std::shared_ptr<const InfantryModelCompileResult>, foundation::Error>::success(
                std::make_shared<const InfantryModelCompileResult>(std::move(generated.value())));
        });
}

} // namespace genomes::infantry
