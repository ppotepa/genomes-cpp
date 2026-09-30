cmake_minimum_required(VERSION 3.21)
if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

# CPU-only configuration must remain headless and must not select a graphics
# backend merely because geometry/toolkit code is enabled.
set(GENOMES_ENABLE_SDL OFF)
set(GENOMES_ENABLE_DILIGENT OFF)
include("${GENOMES_SOURCE_DIR}/cmake/GenomesOptions.cmake")
if(NOT GENOMES_RENDER_BACKEND STREQUAL "HEADLESS")
message(FATAL_ERROR "CPU-only configuration selected a renderer")
endif()

unset(GENOMES_RENDER_BACKEND CACHE)
unset(GENOMES_RENDER_BACKEND)
set(GENOMES_ENABLE_SDL ON)
set(GENOMES_ENABLE_DILIGENT ON)
include("${GENOMES_SOURCE_DIR}/cmake/GenomesOptions.cmake")
if(NOT GENOMES_RENDER_BACKEND STREQUAL "DILIGENT")
message(FATAL_ERROR "Diligent plus geometry dependency did not select Diligent")
endif()
