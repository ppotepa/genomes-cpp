add_library(genomes_render_core STATIC
    core/src/RenderExtraction.cpp
    core/src/ProceduralMeshes.cpp
    core/src/PresentationPipeline.cpp
    core/src/SkinnedDeformer.cpp)
target_include_directories(genomes_render_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include")
target_link_libraries(genomes_render_core PUBLIC genomes::foundation genomes::ui)
target_compile_features(genomes_render_core PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_render_core)

add_library(genomes::render_core ALIAS genomes_render_core)

add_library(genomes_render_graph STATIC
    graph/src/RenderGraph.cpp)
target_include_directories(genomes_render_graph PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/graph/include")
target_link_libraries(genomes_render_graph PUBLIC genomes::foundation)
target_compile_features(genomes_render_graph PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_render_graph)

add_library(genomes::render_graph ALIAS genomes_render_graph)

add_subdirectory(compute)

add_library(genomes_render_gpu_scene STATIC
    gpu_scene/src/GpuScene.cpp)
target_include_directories(genomes_render_gpu_scene PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/gpu_scene/include")
target_link_libraries(genomes_render_gpu_scene PUBLIC genomes::foundation genomes::ui)
target_compile_features(genomes_render_gpu_scene PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_render_gpu_scene)

add_library(genomes::render_gpu_scene ALIAS genomes_render_gpu_scene)

add_library(genomes_render INTERFACE)
target_link_libraries(genomes_render INTERFACE genomes::foundation genomes::ui
    genomes::render_core genomes::render_graph genomes::render_gpu_scene
    genomes::render_compute)
target_include_directories(genomes_render INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include")

add_library(genomes::render ALIAS genomes_render)
