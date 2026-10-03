option(GENOMES_BUILD_TESTS "Build Genomes native tests" ON)
option(GENOMES_BUILD_BENCHMARKS "Build Genomes native benchmarks" ON)
option(GENOMES_WARNINGS_AS_ERRORS "Treat warnings in Genomes-owned native code as errors" ON)
option(GENOMES_ENABLE_DILIGENT "Configure pinned Diligent and its renderer adapter" OFF)
option(GENOMES_ENABLE_SDL "Configure pinned SDL3 and the desktop platform module" OFF)
option(GENOMES_ENABLE_RMLUI "Build the RmlUi-backed UI runtime" OFF)
option(GENOMES_ENABLE_INFANTRY "Build and compose the optional infantry domain module" ON)
option(GENOMES_ENABLE_JS_REFERENCE_PARITY
    "Regenerate versioned infantry reference artifacts with pinned JavaScript (developer-only)" OFF)
option(GENOMES_BUILD_INFANTRY_REFERENCE_COMPARISON
    "Build the opt-in, non-gating C++ vs JavaScript comparison diagnostic" OFF)
option(GENOMES_ENABLE_MESHOPTIMIZER
    "Prepare render index order with pinned CPU-only meshoptimizer" OFF)
option(GENOMES_ENABLE_ASSETS
    "Build the optional backend-neutral static glTF importer" OFF)
option(GENOMES_ENABLE_CSG
    "Build the optional Manifold-backed solid operations" OFF)
option(GENOMES_ENABLE_GPU_TESTS
    "Enable opt-in Diligent/GPU acceptance tests" OFF)
option(GENOMES_BUILD_TOOLKIT_TOOLS
    "Build optional headless lightweight-toolkit tools" OFF)
option(GENOMES_ENABLE_EARCUT
    "Configure pinned earcut.hpp for polygon triangulation" ON)
option(GENOMES_ENABLE_MIKKTSPACE
    "Configure pinned MikkTSpace for tangent generation" ON)

set(GENOMES_ENABLE_CUDA "AUTO" CACHE STRING "CUDA compute backend mode (AUTO, OFF, or ON)")
set_property(CACHE GENOMES_ENABLE_CUDA PROPERTY STRINGS AUTO OFF ON)

set(_genomes_backend_default HEADLESS)
if(GENOMES_ENABLE_SDL AND GENOMES_ENABLE_DILIGENT)
    set(_genomes_backend_default DILIGENT)
endif()
set(GENOMES_RENDER_BACKEND "${_genomes_backend_default}" CACHE STRING
    "Application presentation profile: HEADLESS or DILIGENT (Diligent/D3D12)")
unset(_genomes_backend_default)
set_property(CACHE GENOMES_RENDER_BACKEND PROPERTY STRINGS HEADLESS DILIGENT)
if(NOT GENOMES_RENDER_BACKEND MATCHES "^(HEADLESS|DILIGENT)$")
    message(FATAL_ERROR "Unknown GENOMES_RENDER_BACKEND=${GENOMES_RENDER_BACKEND}")
endif()
if(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT" AND
        (NOT GENOMES_ENABLE_DILIGENT OR NOT GENOMES_ENABLE_SDL))
    message(FATAL_ERROR "DILIGENT requires GENOMES_ENABLE_DILIGENT=ON and GENOMES_ENABLE_SDL=ON")
endif()
message(STATUS "Genomes presentation profile: ${GENOMES_RENDER_BACKEND}")
