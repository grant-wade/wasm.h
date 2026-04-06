if(NOT DEFINED WL_WABT_BUILD_DIR)
    message(FATAL_ERROR "WL_WABT_BUILD_DIR is required.")
endif()

if(NOT DEFINED WL_WABT_OUTPUT_DIR)
    message(FATAL_ERROR "WL_WABT_OUTPUT_DIR is required.")
endif()

set(WL_WABT_TOOL_NAMES
    wat2wasm
    wast2json
    wasm2wat
    wasm2c
    wasm-stats
    wasm-objdump
    wasm-interp
    spectest-interp
    wat-desugar
    wasm-validate
    wasm-strip
    wasm-decompile
)

set(WL_WABT_SEARCH_DIRS "${WL_WABT_BUILD_DIR}")
if(DEFINED WL_WABT_CONFIG AND NOT WL_WABT_CONFIG STREQUAL "")
    list(INSERT WL_WABT_SEARCH_DIRS 0 "${WL_WABT_BUILD_DIR}/${WL_WABT_CONFIG}")
endif()

if(WIN32)
    set(WL_WABT_EXE_SUFFIX ".exe")
else()
    set(WL_WABT_EXE_SUFFIX "")
endif()

file(MAKE_DIRECTORY "${WL_WABT_OUTPUT_DIR}")

foreach(WL_WABT_TOOL_NAME IN LISTS WL_WABT_TOOL_NAMES)
    set(WL_WABT_TOOL_PATH "")
    foreach(WL_WABT_SEARCH_DIR IN LISTS WL_WABT_SEARCH_DIRS)
        set(WL_WABT_CANDIDATE "${WL_WABT_SEARCH_DIR}/${WL_WABT_TOOL_NAME}${WL_WABT_EXE_SUFFIX}")
        if(EXISTS "${WL_WABT_CANDIDATE}")
            set(WL_WABT_TOOL_PATH "${WL_WABT_CANDIDATE}")
            break()
        endif()
    endforeach()

    if(WL_WABT_TOOL_PATH STREQUAL "")
        message(FATAL_ERROR
            "Expected WABT tool '${WL_WABT_TOOL_NAME}${WL_WABT_EXE_SUFFIX}' was not found in ${WL_WABT_SEARCH_DIRS}."
        )
    endif()

    file(COPY_FILE
        "${WL_WABT_TOOL_PATH}"
        "${WL_WABT_OUTPUT_DIR}/${WL_WABT_TOOL_NAME}${WL_WABT_EXE_SUFFIX}"
        ONLY_IF_DIFFERENT
    )
endforeach()