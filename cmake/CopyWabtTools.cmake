function(wl_wabt_get_tool_names out_var)
    set(${out_var}
        wat2wasm
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
        PARENT_SCOPE
    )
endfunction()

function(wl_copy_wabt_tools)
    set(options)
    set(oneValueArgs BUILD_DIR OUTPUT_DIR CONFIG)
    set(multiValueArgs)
    cmake_parse_arguments(WL_WABT_COPY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT DEFINED WL_WABT_COPY_BUILD_DIR OR WL_WABT_COPY_BUILD_DIR STREQUAL "")
        message(FATAL_ERROR "BUILD_DIR is required.")
    endif()

    if(NOT DEFINED WL_WABT_COPY_OUTPUT_DIR OR WL_WABT_COPY_OUTPUT_DIR STREQUAL "")
        message(FATAL_ERROR "OUTPUT_DIR is required.")
    endif()

    wl_wabt_get_tool_names(WL_WABT_TOOL_NAMES)

    set(WL_WABT_SEARCH_DIRS "${WL_WABT_COPY_BUILD_DIR}")
    if(DEFINED WL_WABT_COPY_CONFIG AND NOT WL_WABT_COPY_CONFIG STREQUAL "")
        list(INSERT WL_WABT_SEARCH_DIRS 0 "${WL_WABT_COPY_BUILD_DIR}/${WL_WABT_COPY_CONFIG}")
    endif()

    if(WIN32)
        set(WL_WABT_EXE_SUFFIX ".exe")
    else()
        set(WL_WABT_EXE_SUFFIX "")
    endif()

    file(MAKE_DIRECTORY "${WL_WABT_COPY_OUTPUT_DIR}")

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
            "${WL_WABT_COPY_OUTPUT_DIR}/${WL_WABT_TOOL_NAME}${WL_WABT_EXE_SUFFIX}"
            ONLY_IF_DIFFERENT
        )
    endforeach()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
    if(NOT DEFINED WL_WABT_BUILD_DIR)
        message(FATAL_ERROR "WL_WABT_BUILD_DIR is required.")
    endif()

    if(NOT DEFINED WL_WABT_OUTPUT_DIR)
        message(FATAL_ERROR "WL_WABT_OUTPUT_DIR is required.")
    endif()

    wl_copy_wabt_tools(
        BUILD_DIR "${WL_WABT_BUILD_DIR}"
        OUTPUT_DIR "${WL_WABT_OUTPUT_DIR}"
        CONFIG "${WL_WABT_CONFIG}"
    )
endif()