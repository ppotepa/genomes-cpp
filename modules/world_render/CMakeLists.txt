add_library(genomes_world_render STATIC
    src/WorldMeshCompiler.cpp)

target_include_directories(genomes_world_render PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_world_render PUBLIC
    genomes::api
    genomes::foundation
    genomes::geometry
    genomes::render
    genomes::terrain
    genomes::world_generation
    genomes::buildings)
target_compile_features(genomes_world_render PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_world_render)

add_library(genomes::world_render ALIAS genomes_world_render)
