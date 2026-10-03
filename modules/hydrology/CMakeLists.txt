add_library(genomes_hydrology STATIC
    src/HydrologyGenerator.cpp
    src/HydrologyProcedural.cpp)

target_include_directories(genomes_hydrology PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_hydrology PUBLIC genomes::api genomes::foundation genomes::proc)
target_compile_features(genomes_hydrology PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_hydrology)

add_library(genomes::hydrology ALIAS genomes_hydrology)
