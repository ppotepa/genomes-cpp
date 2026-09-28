set(GENOMES_SDL_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL"
    CACHE PATH
    "Pinned SDL3 source directory")

function(genomes_configure_sdl)
    if(NOT GENOMES_ENABLE_SDL)
        return()
    endif()

    genomes_require_file(
        "${GENOMES_SDL_SOURCE_DIR}/CMakeLists.txt"
        "SDL3")

    set(SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE)
    set(SDL_INSTALL_CPACK OFF CACHE BOOL "Package SDL3" FORCE)

    add_subdirectory(
        "${GENOMES_SDL_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/sdl-build"
        EXCLUDE_FROM_ALL)

    if(TARGET SDL3-static)
        set_property(TARGET SDL3-static PROPERTY GENOMES_THIRD_PARTY TRUE)
    endif()
    set(GENOMES_SDL_CONFIGURED TRUE CACHE INTERNAL
        "SDL3 was configured by Genomes")
endfunction()
