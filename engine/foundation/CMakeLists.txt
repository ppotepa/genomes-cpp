add_library(genomes_foundation STATIC
    src/BuildInfo.cpp
    src/MemoryTelemetry.cpp
    src/StableHash.cpp
    src/WallClock.cpp)

target_include_directories(genomes_foundation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(genomes_foundation PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_foundation)

add_library(genomes::foundation ALIAS genomes_foundation)
