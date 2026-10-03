add_library(genomes_roads STATIC
    src/RoadGraph.cpp)

target_include_directories(genomes_roads PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_roads PUBLIC genomes::api genomes::foundation genomes::proc)
target_compile_features(genomes_roads PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_roads)

add_library(genomes::roads ALIAS genomes_roads)
