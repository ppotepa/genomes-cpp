include(GenomesThirdParty)

set(GENOMES_DILIGENT_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine"
    CACHE PATH
    "Pinned Diligent Engine source directory")

function(genomes_require_file path display_name)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR
            "${display_name} is not initialized at '${path}'. "
            "Run: git submodule update --init --recursive")
    endif()
endfunction()

function(genomes_configure_diligent)
    if(NOT GENOMES_ENABLE_DILIGENT)
        return()
    endif()

    if(NOT WIN32)
        message(FATAL_ERROR "Genomes Diligent production target is Windows/D3D12 only")
    endif()

    genomes_require_file(
        "${GENOMES_DILIGENT_SOURCE_DIR}/CMakeLists.txt"
        "Diligent Engine")
    genomes_require_file(
        "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentTools/CMakeLists.txt"
        "DiligentTools (required for Genomes capture)")
    genomes_require_file(
        "${GENOMES_DILIGENT_SOURCE_DIR}/DiligentFX/CMakeLists.txt"
        "DiligentFX")

    # These cache values are set before add_subdirectory because the upstream
    # meta-project reads them while it configures DiligentCore.
    # Genomes capture uses DiligentTools directly; DiligentFX is not part of
    # the production renderer dependency closure.
    set(DILIGENT_BUILD_TOOLS ON CACHE BOOL "Build DiligentTools" FORCE)
    set(DILIGENT_BUILD_FX OFF CACHE BOOL "Build DiligentFX" FORCE)
    set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "Build DiligentSamples" FORCE)
    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "Build Diligent tests" FORCE)

    # Genomes uses D3D12 as the only production backend. Unused backends are
    # pruned explicitly to keep build times and the dependency surface small.
    set(DILIGENT_NO_DIRECT3D11 ON CACHE BOOL "Disable Diligent D3D11" FORCE)
    set(DILIGENT_NO_DIRECT3D12 OFF CACHE BOOL "Disable Diligent D3D12" FORCE)
    set(DILIGENT_NO_OPENGL ON CACHE BOOL "Disable Diligent OpenGL" FORCE)
    set(DILIGENT_NO_VULKAN ON CACHE BOOL "Disable Diligent Vulkan" FORCE)
    set(DILIGENT_NO_METAL ON CACHE BOOL "Disable Diligent Metal" FORCE)
    set(DILIGENT_NO_WEBGPU ON CACHE BOOL "Disable Diligent WebGPU" FORCE)

    add_subdirectory(
        "${GENOMES_DILIGENT_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/diligent-build"
        EXCLUDE_FROM_ALL)

    foreach(target_name IN ITEMS
            DiligentCore-static
            Diligent-GraphicsEngineD3D12-static)
        genomes_mark_third_party("${target_name}")
    endforeach()

    set(GENOMES_DILIGENT_CONFIGURED TRUE CACHE INTERNAL
        "Diligent was configured by Genomes")
endfunction()

function(genomes_configure_rmlui)
    if(NOT GENOMES_ENABLE_RMLUI)
        return()
    endif()

    genomes_require_file("${CMAKE_CURRENT_SOURCE_DIR}/external/RmlUi/CMakeLists.txt" "RmlUi")
    genomes_require_file("${CMAKE_CURRENT_SOURCE_DIR}/external/freetype/CMakeLists.txt" "FreeType")

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static third-party libraries" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable optional FreeType HarfBuzz support" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/freetype"
        "${CMAKE_BINARY_DIR}/_deps/freetype-build" EXCLUDE_FROM_ALL)
    if(TARGET freetype AND NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    set(RMLUI_SAMPLES OFF CACHE BOOL "Build RmlUi samples" FORCE)
    set(RMLUI_TESTS OFF CACHE BOOL "Build RmlUi tests" FORCE)
    set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "Build RmlUi Lua bindings" FORCE)
    set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "RmlUi font engine" FORCE)
    set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "RmlUi precompiled headers" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/RmlUi"
        "${CMAKE_BINARY_DIR}/_deps/rmlui-build" EXCLUDE_FROM_ALL)
    set(GENOMES_RMLUI_CONFIGURED TRUE CACHE INTERNAL "RmlUi was configured by Genomes")
endfunction()
