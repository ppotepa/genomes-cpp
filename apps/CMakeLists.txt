add_subdirectory(headless)
add_subdirectory(menu)
add_subdirectory(proc_viewer)

if(GENOMES_ENABLE_DILIGENT)
    add_subdirectory(render_smoke)
endif()

if(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT_LEGACY")
    add_subdirectory(game)
elseif(GENOMES_RENDER_BACKEND STREQUAL "THREEPP_GL")
    # G02 only. Do not silently run Diligent or advertise a partial game as ready.
    # The existing genomes_game composition switches after renderer/UI gates.
    add_subdirectory(threepp_bootstrap)
    message(STATUS "THREEPP_GL migration: build genomes_threepp_bootstrap; genomes_game is not switched yet")
endif()
