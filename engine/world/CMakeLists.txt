add_library(genomes_world STATIC
    src/CityGenerator.cpp
    src/WorldGenerationTask.cpp
    src/WorldRegionStreamer.cpp
    src/WorldPosition.cpp
    src/WorldGenerator.cpp
    src/WorldQuery.cpp
    src/WorldStreamer.cpp
    src/WorldSave.cpp)

target_include_directories(genomes_world PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_world PUBLIC genomes::foundation genomes::proc genomes_hydrology
                                           genomes_roads genomes::io)
target_compile_features(genomes_world PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_world)

add_library(genomes::world ALIAS genomes_world)
