add_subdirectory(headless)
add_subdirectory(menu)
add_subdirectory(proc_viewer)

if(GENOMES_ENABLE_ASSETS AND GENOMES_BUILD_TOOLKIT_TOOLS)
    add_subdirectory(asset_probe)
endif()

if(GENOMES_ENABLE_DILIGENT)
    add_subdirectory(render_smoke)
endif()

if(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT_LEGACY")
    add_subdirectory(game)
elseif(GENOMES_RENDER_BACKEND STREQUAL "THREEPP_GL")
    # Keep the low-level bootstrap as a focused context/lifecycle diagnostic,
    # while the real game now consumes the same renderer-neutral snapshots.
    add_subdirectory(threepp_bootstrap)
    add_subdirectory(game)
endif()
