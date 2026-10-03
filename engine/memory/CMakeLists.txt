add_library(genomes_memory STATIC
    src/LinearArena.cpp)

target_include_directories(genomes_memory PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(genomes_memory PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_memory)

add_library(genomes::memory ALIAS genomes_memory)
