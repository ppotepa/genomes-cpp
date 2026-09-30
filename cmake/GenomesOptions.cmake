option(GENOMES_BUILD_TESTS "Build Genomes native tests" ON)
option(GENOMES_BUILD_BENCHMARKS "Build Genomes native benchmarks" ON)
option(GENOMES_WARNINGS_AS_ERRORS "Treat warnings in Genomes-owned native code as errors" ON)
option(GENOMES_ENABLE_DILIGENT "Configure pinned Diligent and its renderer adapter" OFF)
option(GENOMES_ENABLE_SDL "Configure pinned SDL3 and the desktop platform module" OFF)
option(GENOMES_ENABLE_RMLUI "Build the RmlUi-backed UI runtime" OFF)
option(GENOMES_ENABLE_INFANTRY "Build and compose the optional infantry domain module" ON)
option(GENOMES_ENABLE_JS_REFERENCE_PARITY
    "Regenerate infantry parity fixtures with pinned JavaScript (developer-only)" OFF)
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

set(GENOMES_ENABLE_CUDA "AUTO" CACHE STRING "CUDA compute backend mode (AUTO, OFF, or ON)")
set_property(CACHE GENOMES_ENABLE_CUDA PROPERTY STRINGS AUTO OFF ON)

# Dependency usage and application renderer selection are separate decisions.
# This enables the existing CPU primitive provider, NOT an OpenGL game.
# The pinned upstream build is still broad; CPU usage does not mean GL/GLFW
# have already been removed from the upstream compilation/link dependency set.
option(GENOMES_ENABLE_THREEPP "Configure pinned threepp for geometry/tools" OFF)
set(_genomes_threepp_renderer_default OFF)
if(DEFINED GENOMES_RENDER_BACKEND AND GENOMES_RENDER_BACKEND STREQUAL "THREEPP_GL")
    set(_genomes_threepp_renderer_default ON)
endif()
option(GENOMES_ENABLE_THREEPP_RENDERER
    "Build experimental threepp/GLRenderer adapter (not the production architecture)"
    ${_genomes_threepp_renderer_default})
unset(_genomes_threepp_renderer_default)

set(_genomes_backend_default HEADLESS)
if(GENOMES_ENABLE_SDL AND GENOMES_ENABLE_DILIGENT)
    set(_genomes_backend_default DILIGENT_LEGACY)
elseif(GENOMES_ENABLE_THREEPP_RENDERER)
    set(_genomes_backend_default THREEPP_GL)
endif()
# DILIGENT_LEGACY is retained as a compatibility spelling for existing callers.
# The accepted production renderer is Diligent; reactivation is a separate gate.
set(GENOMES_RENDER_BACKEND "${_genomes_backend_default}" CACHE STRING
    "Application presentation profile: HEADLESS, DILIGENT_LEGACY, THREEPP_GL (experimental)")
unset(_genomes_backend_default)
set_property(CACHE GENOMES_RENDER_BACKEND PROPERTY STRINGS HEADLESS DILIGENT_LEGACY THREEPP_GL)
if(NOT GENOMES_RENDER_BACKEND MATCHES "^(HEADLESS|THREEPP_GL|DILIGENT_LEGACY)$")
    message(FATAL_ERROR "Unknown GENOMES_RENDER_BACKEND=${GENOMES_RENDER_BACKEND}")
endif()
if(GENOMES_ENABLE_THREEPP_RENDERER AND
        (NOT GENOMES_ENABLE_THREEPP OR NOT GENOMES_ENABLE_SDL))
    message(FATAL_ERROR "The experimental threepp renderer requires threepp and SDL")
endif()
if(GENOMES_RENDER_BACKEND STREQUAL "THREEPP_GL" AND NOT GENOMES_ENABLE_THREEPP_RENDERER)
    message(FATAL_ERROR "THREEPP_GL requires GENOMES_ENABLE_THREEPP_RENDERER=ON")
endif()
if(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT_LEGACY" AND
        (NOT GENOMES_ENABLE_DILIGENT OR NOT GENOMES_ENABLE_SDL))
    message(FATAL_ERROR "DILIGENT_LEGACY requires GENOMES_ENABLE_DILIGENT=ON and GENOMES_ENABLE_SDL=ON")
endif()
message(STATUS "Genomes presentation profile: ${GENOMES_RENDER_BACKEND}")
