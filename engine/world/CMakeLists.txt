add_library(genomes_world_core STATIC
    src/WorldPosition.cpp
    src/WorldQuery.cpp
    src/WorldSave.cpp)

target_include_directories(genomes_world_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_world_core PUBLIC
    genomes::api
    genomes::foundation
    genomes::proc
    genomes::io)
target_compile_features(genomes_world_core PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_world_core)

add_library(genomes_world_generation STATIC
    src/CityGenerator.cpp
    src/WorldGenerationProfile.cpp
    src/WorldGenerationTask.cpp
    src/WorldRegionStreamer.cpp
    src/WorldGenerator.cpp
    src/WorldStreamer.cpp)

target_include_directories(genomes_world_generation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(genomes_world_generation PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_link_libraries(genomes_world_generation PUBLIC
    genomes::content
    genomes_world_core
    genomes::foundation
    genomes::proc
    genomes_hydrology
    genomes_roads)
target_compile_features(genomes_world_generation PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_world_generation)

add_library(genomes::world_core ALIAS genomes_world_core)
add_library(genomes::world_generation ALIAS genomes_world_generation)
