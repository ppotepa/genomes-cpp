if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(_benchmark_dir "${GENOMES_SOURCE_DIR}/benchmarks")
set(_cmake "${_benchmark_dir}/CMakeLists.txt")
file(READ "${_cmake}" _cmake_text)
foreach(_target IN ITEMS genomes_bench_infantry_appearance genomes_bench_infantry_animation)
    string(FIND "${_cmake_text}" "${_target}" _target_offset)
    if(_target_offset LESS 0)
        message(FATAL_ERROR "PR16 benchmark target is missing: ${_target}")
    endif()
endforeach()
foreach(_source IN ITEMS
        "${_benchmark_dir}/infantry/appearance_generation_benchmark.cpp"
        "${_benchmark_dir}/infantry/animation_benchmark.cpp")
    file(READ "${_source}" _source_text)
    foreach(_token IN ITEMS
            "../Pr16Measurement.hpp"
            "loadPr16MeasurementPlan"
            "GENOMES_SOURCE_DIR"
            "BASELINE_REQUIRED"
            "required_metrics"
            "samples_us")
        string(FIND "${_source_text}" "${_token}" _offset)
        if(_offset LESS 0)
            message(FATAL_ERROR "PR16 benchmark source misses fixture contract token '${_token}': ${_source}")
        endif()
    endforeach()
endforeach()
foreach(_token IN ITEMS
        "target_include_directories(genomes_bench_infantry_appearance"
        "target_include_directories(genomes_bench_infantry_animation"
        "GENOMES_SOURCE_DIR=")
    string(FIND "${_cmake_text}" "${_token}" _offset)
    if(_offset LESS 0)
        message(FATAL_ERROR "PR16 benchmark CMake contract misses '${_token}'")
    endif()
endforeach()
message(STATUS "PR16 measurement benchmark source contract: PASS (baseline remains user-required)")
