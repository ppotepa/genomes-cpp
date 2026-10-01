add_executable(genomes_bench_jobs_system
    jobs/job_system_benchmark.cpp)
target_link_libraries(genomes_bench_jobs_system PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_bench_jobs_system)

add_executable(genomes_bench_destruction_impact
    destruction/impact_solver_benchmark.cpp)
target_link_libraries(genomes_bench_destruction_impact PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_bench_destruction_impact)

add_executable(genomes_bench_destruction_rubble_field
    destruction/rubble_field_benchmark.cpp)
target_link_libraries(genomes_bench_destruction_rubble_field PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_bench_destruction_rubble_field)

add_executable(genomes_bench_ballistics_fragmentation
    ballistics_fragmentation_benchmark.cpp)
target_link_libraries(genomes_bench_ballistics_fragmentation PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_bench_ballistics_fragmentation)

add_executable(genomes_bench_ballistics_batch
    ballistics/ballistics_batch_benchmark.cpp)
target_link_libraries(genomes_bench_ballistics_batch PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_bench_ballistics_batch)

add_executable(genomes_bench_perception_broadphase
    combat/perception_broadphase_benchmark.cpp)
target_link_libraries(genomes_bench_perception_broadphase PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_bench_perception_broadphase)

add_executable(genomes_bench_tactical_ai
    combat/tactical_ai_benchmark.cpp)
target_link_libraries(genomes_bench_tactical_ai PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_bench_tactical_ai)

add_executable(genomes_bench_squad_signal
    combat/squad_signal_benchmark.cpp)
target_link_libraries(genomes_bench_squad_signal PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_bench_squad_signal)

add_executable(genomes_bench_ai_pipeline
    combat/ai_pipeline_benchmark.cpp)
target_link_libraries(genomes_bench_ai_pipeline PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_bench_ai_pipeline)

add_executable(genomes_bench_world_streaming
    world/streaming_benchmark.cpp)
target_link_libraries(genomes_bench_world_streaming PRIVATE genomes::world_generation)
target_compile_definitions(genomes_bench_world_streaming PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_bench_world_streaming)

add_executable(genomes_bench_full_scene
    render/full_scene_benchmark.cpp)
target_link_libraries(genomes_bench_full_scene PRIVATE genomes::render_core)
genomes_set_project_warnings(genomes_bench_full_scene)

add_executable(genomes_bench_toolkit toolkit_benchmark.cpp)
target_link_libraries(genomes_bench_toolkit PRIVATE genomes::geometry)
if(GENOMES_ENABLE_CSG)
    target_compile_definitions(genomes_bench_toolkit PRIVATE GENOMES_BENCH_HAS_CSG=1)
else()
    target_compile_definitions(genomes_bench_toolkit PRIVATE GENOMES_BENCH_HAS_CSG=0)
endif()
if(GENOMES_ENABLE_ASSETS)
    target_link_libraries(genomes_bench_toolkit PRIVATE genomes::assets)
    target_compile_definitions(genomes_bench_toolkit PRIVATE GENOMES_BENCH_HAS_ASSETS=1)
else()
    target_compile_definitions(genomes_bench_toolkit PRIVATE GENOMES_BENCH_HAS_ASSETS=0)
endif()
genomes_set_project_warnings(genomes_bench_toolkit)

add_executable(genomes_bench_field_atlas
    render/field_atlas_benchmark.cpp)
target_link_libraries(genomes_bench_field_atlas PRIVATE genomes::render_compute)
genomes_set_project_warnings(genomes_bench_field_atlas)

add_executable(genomes_bench_flow_field
    compute/flow_field_benchmark.cpp)
target_link_libraries(genomes_bench_flow_field PRIVATE genomes::navigation)
genomes_set_project_warnings(genomes_bench_flow_field)

add_executable(genomes_bench_cuda_backend
    compute/cuda_backend_benchmark.cpp)
target_link_libraries(genomes_bench_cuda_backend PRIVATE genomes::compute_cuda)
genomes_set_project_warnings(genomes_bench_cuda_backend)

if(GENOMES_ENABLE_INFANTRY)
    add_executable(genomes_bench_infantry_appearance
        infantry/appearance_generation_benchmark.cpp)
    target_link_libraries(genomes_bench_infantry_appearance PRIVATE genomes::infantry)
    target_include_directories(genomes_bench_infantry_appearance PRIVATE
        "${CMAKE_SOURCE_DIR}/external/json/single_include")
    target_compile_definitions(genomes_bench_infantry_appearance PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_bench_infantry_appearance)

    add_executable(genomes_bench_infantry_animation
        infantry/animation_benchmark.cpp)
    target_link_libraries(genomes_bench_infantry_animation PRIVATE genomes::infantry)
    target_include_directories(genomes_bench_infantry_animation PRIVATE
        "${CMAKE_SOURCE_DIR}/external/json/single_include")
    target_compile_definitions(genomes_bench_infantry_animation PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_bench_infantry_animation)
endif()
