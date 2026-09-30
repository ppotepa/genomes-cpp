set(GENOMES_DILIGENT_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(_shader_files SceneCommon.hlsli CharacterCommon.hlsli SkinnedCharacter.vsh
    RegularSurface.vsh SkinnedCharacter.psh ShadowSurface.psh)
foreach(_file IN LISTS _shader_files)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/${_file}")
endforeach()
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/SceneCommon.hlsli" GENOMES_SCENE_COMMON)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/CharacterCommon.hlsli" GENOMES_CHARACTER_COMMON)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/SkinnedCharacter.vsh" GENOMES_SKINNED_VERTEX)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/RegularSurface.vsh" GENOMES_REGULAR_VERTEX)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/SkinnedCharacter.psh" GENOMES_SKINNED_PIXEL)
file(READ "${GENOMES_DILIGENT_SHADER_SOURCE_DIR}/ShadowSurface.psh" GENOMES_SHADOW_PIXEL)
configure_file(ShaderSources.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/ShaderSources.hpp" @ONLY)

if(NOT TARGET Diligent-TextureLoader)
    message(FATAL_ERROR "Diligent restoration requires pinned DiligentTools TextureLoader for PNG capture")
endif()
add_library(genomes_render_diligent STATIC
    DiligentBackend.cpp DiligentResources.cpp DiligentMeshUpload.cpp
    DiligentScenePasses.cpp DiligentDraw.cpp DiligentUiPass.cpp
    DiligentCapture.cpp DiligentSceneRenderer.cpp)
target_link_libraries(genomes_render_diligent PUBLIC genomes::render genomes::camera PRIVATE
    Diligent-TextureLoader
    Diligent-GraphicsEngineD3D12-static)
target_include_directories(genomes_render_diligent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(genomes_render_diligent PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(genomes_render_diligent SYSTEM PRIVATE
    "${GENOMES_DILIGENT_SOURCE_DIR}"
    "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngine/interface"
    "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentCore/Primitives/interface")
target_compile_features(genomes_render_diligent PRIVATE cxx_std_20)
genomes_set_project_warnings(genomes_render_diligent)
find_package(Git QUIET)
set(_capture_sha "unknown")
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _capture_sha OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _capture_git_result ERROR_QUIET)
    if(NOT _capture_git_result EQUAL 0)
        set(_capture_sha "unknown")
    endif()
endif()
target_compile_definitions(genomes_render_diligent PRIVATE GENOMES_DILIGENT_BUILD_SHA="${_capture_sha}")
add_library(genomes::render_diligent ALIAS genomes_render_diligent)
unset(_shader_files)
unset(_file)
unset(_capture_sha)
unset(_capture_git_result)
