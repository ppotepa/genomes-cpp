option(GENOMES_BUILD_TESTS
    "Build Genomes native tests"
    ON)

option(GENOMES_BUILD_BENCHMARKS
    "Build Genomes native benchmarks"
    ON)

option(GENOMES_WARNINGS_AS_ERRORS
    "Treat warnings in Genomes-owned native code as errors"
    ON)

option(GENOMES_ENABLE_DILIGENT
    "Configure the pinned Diligent Engine dependency and its renderer adapter"
    OFF)

option(GENOMES_ENABLE_SDL
    "Configure the pinned SDL3 dependency and the desktop platform module"
    OFF)

option(GENOMES_ENABLE_RMLUI
    "Build the RmlUi-backed UI runtime"
    OFF)

option(GENOMES_ENABLE_INFANTRY
    "Build and compose the optional infantry domain module"
    ON)

option(GENOMES_ENABLE_JS_REFERENCE_PARITY
    "Regenerate infantry parity fixtures with the pinned JavaScript reference (developer-only)"
    OFF)

set(GENOMES_ENABLE_CUDA
    "AUTO"
    CACHE STRING
    "CUDA compute backend mode (AUTO, OFF, or ON)")
set_property(CACHE GENOMES_ENABLE_CUDA PROPERTY STRINGS AUTO OFF ON)

option(GENOMES_ENABLE_THREEPP "Configure pinned threepp OpenGL support (SDL host)" OFF)
# Derive a legacy-compatible default only on first configure. Explicit presets
# always select a backend; use a separate binaryDir when changing backend.
set(_genomes_backend_default HEADLESS)
if(GENOMES_ENABLE_THREEPP)
    set(_genomes_backend_default THREEPP_GL)
elseif(GENOMES_ENABLE_SDL AND GENOMES_ENABLE_DILIGENT)
    set(_genomes_backend_default DILIGENT_LEGACY)
endif()
set(GENOMES_RENDER_BACKEND "${_genomes_backend_default}" CACHE STRING
    "Application presentation profile: HEADLESS, THREEPP_GL, DILIGENT_LEGACY")
unset(_genomes_backend_default)
set_property(CACHE GENOMES_RENDER_BACKEND PROPERTY STRINGS HEADLESS THREEPP_GL DILIGENT_LEGACY)
if(NOT GENOMES_RENDER_BACKEND MATCHES "^(HEADLESS|THREEPP_GL|DILIGENT_LEGACY)$")
    message(FATAL_ERROR "Unknown GENOMES_RENDER_BACKEND=${GENOMES_RENDER_BACKEND}")
endif()
if(GENOMES_RENDER_BACKEND STREQUAL "THREEPP_GL" AND
        (NOT GENOMES_ENABLE_THREEPP OR NOT GENOMES_ENABLE_SDL))
    message(FATAL_ERROR "THREEPP_GL requires GENOMES_ENABLE_THREEPP=ON and GENOMES_ENABLE_SDL=ON")
endif()
if(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT_LEGACY" AND
        (NOT GENOMES_ENABLE_DILIGENT OR NOT GENOMES_ENABLE_SDL))
    message(FATAL_ERROR "DILIGENT_LEGACY requires GENOMES_ENABLE_DILIGENT=ON and GENOMES_ENABLE_SDL=ON")
endif()
message(STATUS "Genomes presentation profile: ${GENOMES_RENDER_BACKEND}")
