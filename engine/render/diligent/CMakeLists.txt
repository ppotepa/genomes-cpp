set(GENOMES_DILIGENT_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/CharacterCommon.hlsli"
     GENOMES_CHARACTER_COMMON)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/SkinnedCharacter.vsh"
     GENOMES_SKINNED_VERTEX)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/SkinnedCharacter.psh"
     GENOMES_SKINNED_PIXEL)
configure_file(ShaderSources.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/ShaderSources.hpp" @ONLY)

add_library(genomes_render_diligent STATIC
    DiligentBackend.cpp
    DiligentSceneRenderer.cpp)

target_link_libraries(genomes_render_diligent
    PUBLIC
        genomes::render
    PRIVATE
        $<$<PLATFORM_ID:Windows>:Diligent-GraphicsEngineD3D12-static>
        $<$<NOT:$<PLATFORM_ID:Windows>>:Diligent-GraphicsEngineVk-static>)

target_include_directories(genomes_render_diligent
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(genomes_render_diligent
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(genomes_render_diligent
    SYSTEM PRIVATE
        "${GENOMES_DILIGENT_SOURCE_DIR}"
        "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngineVulkan/interface"
        "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
        "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngine/interface"
        "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentCore/Primitives/interface")

target_compile_features(genomes_render_diligent PRIVATE cxx_std_20)
genomes_set_project_warnings(genomes_render_diligent)

add_library(genomes::render_diligent ALIAS genomes_render_diligent)
