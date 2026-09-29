# Bootstrap support first. IRenderer/scene adapters arrive after G02 is verified.
add_library(genomes_render_threepp STATIC ThreeppGlBootstrap.cpp)
target_link_libraries(genomes_render_threepp
    PUBLIC genomes::render
    PRIVATE genomes::platform threepp::threepp)
target_include_directories(genomes_render_threepp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# The private loader header and its existing GLAD implementation come from the
# exact pin. Never compile glad.c here and never export these include paths.
target_include_directories(genomes_render_threepp SYSTEM PRIVATE
    "${GENOMES_THREEPP_SOURCE_DIR}/src"
    "${GENOMES_THREEPP_SOURCE_DIR}/src/external/glad")
target_compile_features(genomes_render_threepp PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_render_threepp)
add_library(genomes::render_threepp ALIAS genomes_render_threepp)
