if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(game_application_source "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.cpp")
set(game_application_header "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.hpp")
set(menu_application_source "${GENOMES_SOURCE_DIR}/apps/menu/main.cpp")
set(application_catalog_header
    "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/genomes/game_scenes/BuiltinScenes.hpp")
set(application_catalog_source
    "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BuiltinScenes.cpp")
foreach(required_application_file IN ITEMS
        "${game_application_source}" "${game_application_header}" "${menu_application_source}"
        "${application_catalog_header}" "${application_catalog_source}")
    if(NOT EXISTS "${required_application_file}")
        message(FATAL_ERROR "R035 composition boundary file is missing: ${required_application_file}")
    endif()
endforeach()

file(READ "${game_application_source}" game_application_text)
file(READ "${game_application_header}" game_application_header_text)
file(READ "${menu_application_source}" menu_application_text)
file(READ "${application_catalog_header}" application_catalog_header_text)
file(READ "${application_catalog_source}" application_catalog_source_text)
if(NOT application_catalog_header_text MATCHES "namespace genomes::application")
    message(FATAL_ERROR "Product catalog must be owned by genomes::application")
endif()
if(NOT application_catalog_header_text MATCHES "using BuiltinSceneFactory" OR
   NOT application_catalog_header_text MATCHES "runtime::Scene")
    message(FATAL_ERROR "Product catalog factory must create neutral runtime scenes")
endif()
if(NOT application_catalog_source_text MATCHES "set_application_action_router")
    message(FATAL_ERROR "Application catalog must install the application action router")
endif()
if(NOT application_catalog_source_text MATCHES "set_scene_command_handler")
    message(FATAL_ERROR "Application catalog must install the scene command handoff")
endif()
if(NOT game_application_header_text MATCHES
   "std::unique_ptr<application::BuiltinSceneCatalog>[ \\t\\r\\n]+scene_catalog_")
    message(FATAL_ERROR "Game composition root must own the built-in scene catalog")
endif()
if(NOT game_application_text MATCHES
   "scene_catalog_[ \\t\\r\\n]*=[ \\t\\r\\n]*std::make_unique<application::BuiltinSceneCatalog>")
    message(FATAL_ERROR "Game composition root must construct its owned scene catalog")
endif()
if(NOT menu_application_text MATCHES "application::BuiltinSceneCatalog")
    message(FATAL_ERROR "Menu composition root must use the application catalog")
endif()
foreach(application_text IN ITEMS "${game_application_text}" "${menu_application_text}")
    if(NOT application_text MATCHES "BuiltinSceneCatalog")
        message(FATAL_ERROR "Production composition must consume BuiltinSceneCatalog")
    endif()
    if(application_text MATCHES "genomes/runtime/BuiltinScenes\\.hpp")
        message(FATAL_ERROR "Production composition must not include the runtime catalog compatibility header")
    endif()
    if(application_text MATCHES "registerBuiltinScenes")
        message(FATAL_ERROR "Production composition must consume BuiltinSceneCatalog directly")
    endif()
    if(application_text MATCHES "ServiceLocator|service_locator|service locator")
        message(FATAL_ERROR "Production composition must not introduce a ServiceLocator")
    endif()
endforeach()
foreach(required_catalog_text IN ITEMS
        "class BuiltinSceneCatalog"
        "using BuiltinSceneFactory"
        "std::span<const BuiltinSceneEntry>"
        "BuiltinSceneCatalog::BuiltinSceneCatalog"
        "void BuiltinSceneCatalog::install")
    if(NOT application_catalog_header_text MATCHES "${required_catalog_text}" AND
       NOT application_catalog_source_text MATCHES "${required_catalog_text}")
        message(FATAL_ERROR "R035 explicit factory/catalog boundary lost ${required_catalog_text}")
    endif()
endforeach()
if(application_catalog_source_text MATCHES
   "director\\.fixed_update|director\\.frame_update|director\\.present")
    message(FATAL_ERROR "Application catalog must not own SceneDirector lifecycle")
endif()
