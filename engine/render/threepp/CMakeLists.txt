add_library(genomes_render_threepp STATIC
    ThreeppGlBootstrap.cpp
    ThreeppInputAdapter.cpp
    ThreeppSceneRenderer.cpp
    ThreeppUiPass.cpp)
target_link_libraries(genomes_render_threepp
    PUBLIC genomes::render
    PRIVATE genomes::platform threepp::threepp)
target_include_directories(genomes_render_threepp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# GLAD is compiled exactly once by the pinned threepp target. These private
# include paths expose only its loader declarations/functions to the adapter.
target_include_directories(genomes_render_threepp SYSTEM PRIVATE
    "${GENOMES_THREEPP_SOURCE_DIR}/src"
    "${GENOMES_THREEPP_SOURCE_DIR}/src/external/glad")
target_compile_features(genomes_render_threepp PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_render_threepp)
add_library(genomes::render_threepp ALIAS genomes_render_threepp)
