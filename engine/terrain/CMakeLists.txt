add_library(genomes_terrain STATIC
    src/HeightField.cpp
    src/TerrainGenerator.cpp
    src/TerrainMesh.cpp
    src/TerrainProcedural.cpp)

target_include_directories(genomes_terrain PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_terrain PUBLIC genomes::foundation genomes::world_core genomes::proc)
target_compile_features(genomes_terrain PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_terrain)

add_library(genomes::terrain ALIAS genomes_terrain)
