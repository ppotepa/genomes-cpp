function(genomes_configure_cuda)
    string(TOUPPER "${GENOMES_ENABLE_CUDA}" _genomes_cuda_mode)
    if(NOT _genomes_cuda_mode STREQUAL "AUTO" AND
       NOT _genomes_cuda_mode STREQUAL "OFF" AND
       NOT _genomes_cuda_mode STREQUAL "ON")
        message(FATAL_ERROR
            "GENOMES_ENABLE_CUDA must be AUTO, OFF, or ON; got '${GENOMES_ENABLE_CUDA}'")
    endif()

    set(GENOMES_ENABLE_CUDA "${_genomes_cuda_mode}" CACHE STRING
        "CUDA compute backend mode (AUTO, OFF, or ON)" FORCE)
    set_property(CACHE GENOMES_ENABLE_CUDA PROPERTY STRINGS AUTO OFF ON)
    set(GENOMES_CUDA_ENABLED OFF CACHE INTERNAL
        "Whether the optional Genomes CUDA backend is compiled" FORCE)

    if(_genomes_cuda_mode STREQUAL "OFF")
        message(STATUS "Genomes CUDA backend: disabled (GENOMES_ENABLE_CUDA=OFF)")
        return()
    endif()

    # FindCUDAToolkit does not enable the CUDA language.  The spike only needs
    # the runtime API, so keeping the project CXX-only avoids imposing nvcc on
    # every native build while still giving ON a strict configure-time check.
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND AND TARGET CUDA::cudart)
        set(GENOMES_CUDA_ENABLED ON CACHE INTERNAL
            "Whether the optional Genomes CUDA backend is compiled" FORCE)
        message(STATUS "Genomes CUDA backend: enabled (CUDA toolkit found)")
        return()
    endif()

    if(_genomes_cuda_mode STREQUAL "ON")
        message(FATAL_ERROR
            "GENOMES_ENABLE_CUDA=ON requires a usable CUDA toolkit (including CUDA::cudart). "
            "Install the CUDA toolkit or configure with GENOMES_ENABLE_CUDA=AUTO/OFF.")
    endif()

    message(STATUS
        "Genomes CUDA backend: disabled (GENOMES_ENABLE_CUDA=AUTO and no usable CUDA toolkit found)")
endfunction()
