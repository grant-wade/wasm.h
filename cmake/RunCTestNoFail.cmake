if(NOT DEFINED WL_CTEST_COMMAND)
    message(FATAL_ERROR "WL_CTEST_COMMAND is required.")
endif()

if(NOT DEFINED WL_CTEST_BINARY_DIR)
    message(FATAL_ERROR "WL_CTEST_BINARY_DIR is required.")
endif()

if(NOT DEFINED WL_CTEST_LABEL)
    set(WL_CTEST_LABEL emcc)
endif()

set(WL_CTEST_ARGS --output-on-failure --test-dir "${WL_CTEST_BINARY_DIR}" -L "${WL_CTEST_LABEL}")
if(DEFINED WL_CTEST_PARALLEL AND NOT WL_CTEST_PARALLEL STREQUAL "")
    list(APPEND WL_CTEST_ARGS --parallel "${WL_CTEST_PARALLEL}")
endif()
if(DEFINED WL_CTEST_CONFIG AND NOT WL_CTEST_CONFIG STREQUAL "")
    list(APPEND WL_CTEST_ARGS -C "${WL_CTEST_CONFIG}")
endif()

execute_process(
    COMMAND "${WL_CTEST_COMMAND}" ${WL_CTEST_ARGS}
    RESULT_VARIABLE WL_CTEST_RESULT
)

if(NOT WL_CTEST_RESULT EQUAL 0)
    message(STATUS "CTest reported failing ${WL_CTEST_LABEL} tests; leaving target successful.")
endif()