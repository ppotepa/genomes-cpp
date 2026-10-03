add_library(genomes_thread_scaling_gate_lib STATIC
    benchmark/thread_scaling_gate.cpp)

target_include_directories(genomes_thread_scaling_gate_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(genomes_thread_scaling_gate_lib PUBLIC cxx_std_20)
target_link_libraries(genomes_thread_scaling_gate_lib PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_thread_scaling_gate_lib)

add_library(genomes::thread_scaling_gate ALIAS genomes_thread_scaling_gate_lib)

if(GENOMES_BUILD_BENCHMARKS)
    add_executable(genomes_thread_scaling_gate
        benchmark/thread_scaling_gate_cli.cpp)
    target_link_libraries(genomes_thread_scaling_gate PRIVATE
        genomes::thread_scaling_gate genomes::jobs)
    genomes_set_project_warnings(genomes_thread_scaling_gate)
endif()
