add_library(genomes_ui INTERFACE)
target_link_libraries(genomes_ui INTERFACE genomes::foundation)
target_include_directories(genomes_ui INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(genomes::ui ALIAS genomes_ui)
