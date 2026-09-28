add_library(genomes_spatial STATIC
    src/SpatialGrid.cpp)

target_include_directories(genomes_spatial PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_spatial PUBLIC
    genomes::foundation
    genomes::simulation)
target_compile_features(genomes_spatial PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_spatial)

add_library(genomes::spatial ALIAS genomes_spatial)
