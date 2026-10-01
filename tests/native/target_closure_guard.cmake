if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(runtime_cmake "${GENOMES_SOURCE_DIR}/engine/runtime/CMakeLists.txt")
set(game_scenes_cmake "${GENOMES_SOURCE_DIR}/engine/game_scenes/CMakeLists.txt")
set(gameplay_cmake "${GENOMES_SOURCE_DIR}/modules/gameplay/CMakeLists.txt")
set(game_cmake "${GENOMES_SOURCE_DIR}/apps/game/CMakeLists.txt")
set(tests_cmake "${GENOMES_SOURCE_DIR}/tests/native/CMakeLists.txt")
set(root_cmake "${GENOMES_SOURCE_DIR}/CMakeLists.txt")

foreach(file IN ITEMS runtime_cmake game_scenes_cmake gameplay_cmake game_cmake tests_cmake root_cmake)
    if(NOT EXISTS "${${file}}")
        message(FATAL_ERROR "Missing target-closure source: ${${file}}")
    endif()
    file(READ "${${file}}" ${file}_text)
endforeach()

# Neutral runtime must not acquire product/gameplay ownership through its
# public link closure.
foreach(forbidden IN ITEMS
        "genomes::game_scenes"
        "genomes::application_scenes"
        "genomes::gameplay"
        "genomes::infantry"
        "genomes::world_generation"
        "genomes::world_render"
        "genomes::render_diligent")
    if(runtime_cmake_text MATCHES "${forbidden}")
        message(FATAL_ERROR "Neutral runtime target links forbidden product dependency: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS "genomes::runtime_core" "genomes::world_generation"
                           "genomes::world_render" "genomes::gameplay")
    if(NOT game_scenes_cmake_text MATCHES "${required}")
        message(FATAL_ERROR "Product scene target is missing required closure edge: ${required}")
    endif()
endforeach()
if(NOT game_scenes_cmake_text MATCHES "GENOMES_ENABLE_INFANTRY")
    message(FATAL_ERROR "Product scene target must make infantry ownership conditional")
endif()
if(NOT game_cmake_text MATCHES "genomes::application_scenes")
    message(FATAL_ERROR "Application target must link the composition-root scene catalog")
endif()
if(game_cmake_text MATCHES "genomes::game_scenes")
    message(FATAL_ERROR "Application target must not bypass application_scenes")
endif()

if(NOT gameplay_cmake_text MATCHES "genomes::world_generation")
    message(FATAL_ERROR "Gameplay target must link the narrow world_generation target")
endif()
if(gameplay_cmake_text MATCHES "genomes::world[^_a-zA-Z]")
    message(FATAL_ERROR "Gameplay target must not link the removed genomes::world aggregate")
endif()

# Optional heavyweight backends/assets are guarded by explicit options, not
# pulled into the default closure.
foreach(file_text IN ITEMS root_cmake_text game_scenes_cmake_text gameplay_cmake_text)
    if(${file_text} MATCHES "target_link_libraries\\([^)]*genomes::(compute_cuda|manifold|assets)" AND
       NOT ${file_text} MATCHES "GENOMES_ENABLE_(CUDA|CSG|ASSETS)")
        message(FATAL_ERROR "Optional backend appears in an unconditional target closure")
    endif()
endforeach()

foreach(required IN ITEMS "public_header_consumer.cpp" "architecture.public_header_consumer"
                           "architecture.refactor_tracker" "config.boundaries")
    if(NOT tests_cmake_text MATCHES "${required}")
        message(FATAL_ERROR "Target boundary consumer/guard is not registered: ${required}")
    endif()
endforeach()

message(STATUS "Target visibility and dependency closure source contract inspected")
