if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE MODULE_FILES
    "${GENOMES_SOURCE_DIR}/modules/*/include/*.hpp"
    "${GENOMES_SOURCE_DIR}/modules/*/src/*.cpp")

set(FORBIDDEN_MODULE_TOKENS
    "std::thread"
    "std::jthread"
    "pthread_create"
    "CreateThread")
foreach(FILE_PATH IN LISTS MODULE_FILES)
    file(READ "${FILE_PATH}" CONTENT)
    foreach(TOKEN IN LISTS FORBIDDEN_MODULE_TOKENS)
        string(FIND "${CONTENT}" "${TOKEN}" POSITION)
        if(NOT POSITION EQUAL -1)
            message(FATAL_ERROR "module execution boundary violation: ${TOKEN} in ${FILE_PATH}")
        endif()
    endforeach()

    string(REGEX MATCH
        "(make_unique|make_shared)[ \t\r\n]*<[ \t\r\n]*(genomes::)?jobs::JobSystem|"
        "(genomes::)?jobs::JobSystem[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*[({]"
        PRIVATE_SCHEDULER
        "${CONTENT}")
    if(PRIVATE_SCHEDULER)
        message(FATAL_ERROR "module execution boundary violation: private JobSystem in ${FILE_PATH}")
    endif()
endforeach()


# System declarations have one neutral execution contract. API registration
# and simulation descriptors may extend it with domain-specific metadata, but
# must not redeclare lane/dependency/access semantics independently.
set(EXECUTION_SPEC
    "${GENOMES_SOURCE_DIR}/engine/execution/include/genomes/execution/SystemSpec.hpp")
set(API_HEADER
    "${GENOMES_SOURCE_DIR}/engine/api/include/genomes/api/Api.hpp")
set(SYSTEM_GRAPH_HEADER
    "${GENOMES_SOURCE_DIR}/engine/simulation/include/genomes/simulation/SystemGraph.hpp")
set(SYSTEM_EXECUTION_SOURCE
    "${GENOMES_SOURCE_DIR}/engine/simulation/src/SystemExecutionPlan.cpp")
foreach(REQUIRED_FILE IN ITEMS EXECUTION_SPEC API_HEADER SYSTEM_GRAPH_HEADER SYSTEM_EXECUTION_SOURCE)
    if(NOT EXISTS "${${REQUIRED_FILE}}")
        message(FATAL_ERROR "missing canonical system execution contract: ${${REQUIRED_FILE}}")
    endif()
endforeach()
file(READ "${EXECUTION_SPEC}" EXECUTION_SPEC_TEXT)
file(READ "${API_HEADER}" API_HEADER_TEXT)
file(READ "${SYSTEM_GRAPH_HEADER}" SYSTEM_GRAPH_TEXT)
file(READ "${SYSTEM_EXECUTION_SOURCE}" SYSTEM_EXECUTION_TEXT)
foreach(REQUIRED_TEXT IN ITEMS
        "struct SystemSpec"
        "SystemAccess access"
        "std::vector<SystemId> predecessors"
        "jobs::ExecutionLane lane")
    string(FIND "${EXECUTION_SPEC_TEXT}" "${REQUIRED_TEXT}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "execution::SystemSpec lost canonical field: ${REQUIRED_TEXT}")
    endif()
endforeach()
string(FIND "${API_HEADER_TEXT}"
       "struct ApiSystemDescriptor final : execution::SystemSpec"
       API_SPEC_POSITION)
if(API_SPEC_POSITION EQUAL -1)
    message(FATAL_ERROR "API system descriptor must extend execution::SystemSpec")
endif()
string(FIND "${SYSTEM_GRAPH_TEXT}"
       "struct SystemDescriptor final : execution::SystemSpec"
       SIMULATION_SPEC_POSITION)
if(SIMULATION_SPEC_POSITION EQUAL -1)
    message(FATAL_ERROR "simulation system descriptor must extend execution::SystemSpec")
endif()
string(FIND "${SYSTEM_EXECUTION_TEXT}" "descriptor.main_thread_only ?"
       LEGACY_LANE_EXECUTION)
if(NOT LEGACY_LANE_EXECUTION EQUAL -1)
    message(FATAL_ERROR
        "SystemExecutionPlan must consume canonical SystemSpec::lane, not legacy affinity flags")
endif()
