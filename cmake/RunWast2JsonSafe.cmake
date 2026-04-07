if(NOT DEFINED WL_WASM_TOOLS OR NOT DEFINED WL_INPUT OR NOT DEFINED WL_OUTPUT_JSON OR NOT DEFINED WL_STATUS)
    message(FATAL_ERROR "RunWast2JsonSafe.cmake requires WL_WASM_TOOLS, WL_INPUT, WL_OUTPUT_JSON, and WL_STATUS")
endif()

if(NOT DEFINED WL_LOG)
    set(WL_LOG "${WL_OUTPUT_JSON}.log")
endif()

get_filename_component(WL_OUTPUT_DIR "${WL_OUTPUT_JSON}" DIRECTORY)
file(MAKE_DIRECTORY "${WL_OUTPUT_DIR}")

execute_process(
    COMMAND "${WL_WASM_TOOLS}" json-from-wast "${WL_INPUT}" -o "${WL_OUTPUT_JSON}" --wasm-dir "${WL_OUTPUT_DIR}"
    RESULT_VARIABLE WL_RESULT
    OUTPUT_VARIABLE WL_STDOUT
    ERROR_VARIABLE WL_STDERR
)

set(WL_COMBINED_LOG "${WL_STDOUT}${WL_STDERR}")
file(WRITE "${WL_LOG}" "${WL_COMBINED_LOG}")

if("${WL_RESULT}" STREQUAL "0")
    file(WRITE "${WL_STATUS}" "ok\n")
    if(NOT EXISTS "${WL_OUTPUT_JSON}")
        message(FATAL_ERROR "json-from-wast succeeded but did not create ${WL_OUTPUT_JSON}")
    endif()
else()
    file(REMOVE "${WL_OUTPUT_JSON}")
    file(WRITE "${WL_OUTPUT_JSON}" "{\n  \"source_filename\": \"${WL_INPUT}\",\n  \"commands\": []\n}\n")
    file(WRITE "${WL_STATUS}" "skip\njson-from-wast failed for ${WL_INPUT}\n${WL_COMBINED_LOG}")
endif()