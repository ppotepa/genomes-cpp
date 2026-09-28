cmake_minimum_required(VERSION 3.20)

project(Genomes VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenomesOptions)
include(GenomesWarnings)
include(GenomesDependencies)
include(GenomesPlatform)
include(GenomesCuda)

set(CMAKE_CXX_EXTENSIONS OFF)

genomes_configure_cuda()
genomes_configure_diligent()
genomes_configure_rmlui()
genomes_configure_sdl()

add_subdirectory(engine)
add_subdirectory(modules)
add_subdirectory(apps)
add_subdirectory(tools)

if(GENOMES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/native)
endif()

if(GENOMES_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
