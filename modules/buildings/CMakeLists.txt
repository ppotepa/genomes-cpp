add_library(genomes_buildings STATIC
    src/BuildingGenerator.cpp
    src/BuildingRuntime.cpp)

target_include_directories(genomes_buildings PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_buildings PUBLIC genomes::foundation genomes::proc genomes::world_core)
target_compile_features(genomes_buildings PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_buildings)

add_library(genomes::buildings ALIAS genomes_buildings)
