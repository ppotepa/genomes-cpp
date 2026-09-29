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
