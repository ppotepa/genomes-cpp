add_library(genomes_engine_api STATIC
    src/Api.cpp
    src/CommandAdapter.cpp)

target_include_directories(genomes_engine_api PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_engine_api PUBLIC
    genomes::foundation
    genomes::jobs)
target_compile_features(genomes_engine_api PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_engine_api)

add_library(genomes::api ALIAS genomes_engine_api)
