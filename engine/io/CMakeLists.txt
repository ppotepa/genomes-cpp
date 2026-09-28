add_library(genomes_io STATIC
    src/AtomicFile.cpp)

target_include_directories(genomes_io PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_io PUBLIC genomes::foundation)
target_compile_features(genomes_io PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_io)

add_library(genomes::io ALIAS genomes_io)
