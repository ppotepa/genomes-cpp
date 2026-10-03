add_library(genomes_buildings STATIC
    src/BuildingProfile.cpp
    src/BuildingGenerator.cpp
    src/BuildingProcedural.cpp
    src/BuildingRuntime.cpp)

target_include_directories(genomes_buildings PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(genomes_buildings PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_link_libraries(genomes_buildings PUBLIC
    genomes::api genomes::content genomes::foundation genomes::proc genomes::world_core)
target_compile_features(genomes_buildings PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_buildings)

add_library(genomes::buildings ALIAS genomes_buildings)
