add_library(genomes_input INTERFACE)
target_link_libraries(genomes_input INTERFACE genomes::foundation)
target_include_directories(genomes_input INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(genomes::input ALIAS genomes_input)
