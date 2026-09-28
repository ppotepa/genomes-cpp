if(GENOMES_CUDA_ENABLED)
    set(GENOMES_CUDA_BACKEND_SOURCE CudaComputeBackend.cpp)
else()
    set(GENOMES_CUDA_BACKEND_SOURCE CudaComputeBackendStub.cpp)
endif()

add_library(genomes_compute_cuda STATIC
    ${GENOMES_CUDA_BACKEND_SOURCE}
    CudaInterop.cpp)

target_include_directories(genomes_compute_cuda PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_compute_cuda PUBLIC
    genomes::compute)
target_compile_features(genomes_compute_cuda PUBLIC cxx_std_20)
if(GENOMES_CUDA_ENABLED)
    target_compile_definitions(genomes_compute_cuda PRIVATE GENOMES_CUDA_ENABLED=1)
    target_link_libraries(genomes_compute_cuda PRIVATE CUDA::cudart)
else()
    target_compile_definitions(genomes_compute_cuda PRIVATE GENOMES_CUDA_ENABLED=0)
endif()
genomes_set_project_warnings(genomes_compute_cuda)

add_library(genomes::compute_cuda ALIAS genomes_compute_cuda)
