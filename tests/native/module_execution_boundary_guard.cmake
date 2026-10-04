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
