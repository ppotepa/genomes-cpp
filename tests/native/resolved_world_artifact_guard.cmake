if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(runtime_header
    "${GENOMES_SOURCE_DIR}/modules/gameplay/include/genomes/gameplay/BattlefieldRuntime.hpp")
set(runtime_source "${GENOMES_SOURCE_DIR}/modules/gameplay/src/BattlefieldRuntime.cpp")
set(scene_source "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BattlefieldScene.cpp")
set(artifact_header
    "${GENOMES_SOURCE_DIR}/modules/gameplay/include/genomes/gameplay/WorldScenario.hpp")
set(mesh_source "${GENOMES_SOURCE_DIR}/modules/world_render/src/WorldMeshCompiler.cpp")
set(world_scenario_source "${GENOMES_SOURCE_DIR}/modules/gameplay/src/WorldScenario.cpp")

foreach(required_file IN ITEMS runtime_header runtime_source scene_source artifact_header mesh_source
                               world_scenario_source)
    if(NOT EXISTS "${${required_file}}")
        message(FATAL_ERROR "Missing R039 source: ${${required_file}}")
    endif()
    file(READ "${${required_file}}" ${required_file}_text)
endforeach()

# A resolved artifact validates its own revision and binds destruction
# invalidations to that exact identity.
foreach(required_text IN ITEMS
        "revision == world::artifactRevision(plan)"
        "destruction_invalidations->worldRevision() == revision")
    string(FIND "${artifact_header_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "ResolvedWorldArtifacts lost revision invariant: ${required_text}")
    endif()
endforeach()

# Rendering must consume the artifact revision and reject a plan/revision mix.
foreach(required_text IN ITEMS
        "source_revision != world::artifactRevision(plan)"
        "mesh->revision = source_revision")
    string(FIND "${mesh_source_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "World mesh compiler lost revision guard: ${required_text}")
    endif()
endforeach()
string(FIND "${scene_source_text}"
       "world_render::WorldMeshCompiler::compile("
       mesh_compile_position)
string(FIND "${scene_source_text}"
       "battlefield_runtime_->bindWorldArtifact(world_artifacts_)"
       runtime_bind_position)
if(mesh_compile_position EQUAL -1 OR runtime_bind_position EQUAL -1)
    message(FATAL_ERROR
            "BattlefieldScene must bind runtime and compile render products from one artifact revision")
endif()

# Collision and navigation are owned by the same authoritative runtime. A
# nonzero revision may be bound once; switching revisions is forbidden.
foreach(required_text IN ITEMS
        "bindWorldArtifactRevision"
        "revision == 0U || navigation_ == nullptr"
        "world_artifact_revision_ != 0U && world_artifact_revision_ != revision"
        "physics_.bindWorldRevision(revision)"
        "navigation_->bindWorldRevision(revision)")
    string(FIND "${runtime_source_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Battlefield runtime lost R039 consumer binding: ${required_text}")
    endif()
endforeach()

foreach(required_text IN ITEMS
        "physics_.setGroundHeightQuery"
        "world_artifact_->sampleLandscape"
        "navigation_->setBlocked")
    string(FIND "${runtime_source_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Battlefield runtime does not consume resolved terrain: ${required_text}")
    endif()
endforeach()

# Production world resolution is already running inside a procedural worker.
# It must compose registered children through the shared JobGraph/inline
# generation path instead of recursively enqueueing a ticket and waiting on it.
foreach(required_text IN ITEMS
        "jobs::JobGraphBuilder"
        "procedural_runtime->generateInline"
        "builder.precedes(terrain_node, hydrology_node)"
        "parallel building generation")
    string(FIND "${world_scenario_source_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "Resolved world procedural DAG lost execution contract: ${required_text}")
    endif()
endforeach()
string(FIND "${world_scenario_source_text}"
       "auto ticket = procedural_runtime->request"
       nested_ticket_position)
if(NOT nested_ticket_position EQUAL -1)
    message(FATAL_ERROR
        "Resolved world composition must not use nested procedural request()+wait()")
endif()

message(STATUS "Resolved world artifact revision source contract inspected")
