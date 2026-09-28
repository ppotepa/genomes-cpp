#include <genomes/render/ShaderReflection.hpp>

#include <cassert>

int main() {
    using namespace genomes;

    render::ShaderCompileInput input{};
    input.source = "float4 main() : SV_TARGET { return 1; }";
    input.entry_point = "main";
    input.stage = render::ShaderStage::Pixel;
    input.compiler_version = "dxc-1";
    input.backend_target = "spirv1.5";
    input.defines = {{"QUALITY", "2"}, {"USE_FOG", "1"}};
    input.includes = {{"common.hlsl", 11}, {"lighting.hlsl", 22}};

    const render::ShaderAssetKey first = render::ShaderAssetKey::make(input);
    std::swap(input.defines[0], input.defines[1]);
    std::swap(input.includes[0], input.includes[1]);
    assert(first == render::ShaderAssetKey::make(input));
    input.includes.front().content_hash = 23;
    assert(first != render::ShaderAssetKey::make(input));

    render::PipelineAsset pipeline{};
    pipeline.pixel_shader = first;
    pipeline.bindings = {{"CameraConstants", 0, 0, render::PipelineResourceClass::Static},
                         {"Instances", 1, 0, render::PipelineResourceClass::Indexed}};
    const std::uint64_t pipeline_key = pipeline.key();
    pipeline.state_flags = 1;
    assert(pipeline_key != pipeline.key());

    render::ShaderReflection reflection{};
    reflection.bindings = {
        {"CameraConstants", 0, 0, render::PipelineResourceClass::Static},
        {"Instances", 1, 0, render::PipelineResourceClass::Indexed}};
    assert(reflection.validate(pipeline.bindings));
    pipeline.bindings[1].resource_class = render::PipelineResourceClass::Dynamic;
    assert(!reflection.validate(pipeline.bindings));
    return 0;
}
