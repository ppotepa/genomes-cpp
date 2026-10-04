add_library(genomes_execution INTERFACE)

target_include_directories(genomes_execution INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_execution INTERFACE
    genomes::foundation
    genomes::jobs)
target_compile_features(genomes_execution INTERFACE cxx_std_20)

add_library(genomes::execution ALIAS genomes_execution)
