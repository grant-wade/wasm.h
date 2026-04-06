wl_add_native_executable(wasm_runner runner.c)

if(NOT WL_ENABLE_EMCC_TESTS)
    message(STATUS "emcc fixture tests disabled (WL_ENABLE_EMCC_TESTS=OFF).")
    return()
endif()

set(WL_EMCC_SEARCH_HINTS)
if(DEFINED ENV{EMSDK})
    list(APPEND WL_EMCC_SEARCH_HINTS
        "$ENV{EMSDK}"
        "$ENV{EMSDK}/upstream/emscripten"
    )
endif()
if(DEFINED ENV{EMSCRIPTEN})
    list(APPEND WL_EMCC_SEARCH_HINTS "$ENV{EMSCRIPTEN}")
endif()

find_program(WL_EMCC_EXECUTABLE
    NAMES emcc emcc.bat emcc.py
    HINTS ${WL_EMCC_SEARCH_HINTS}
    DOC "Path to emcc used for Emscripten-based Wasm fixture tests."
)
if(NOT WL_EMCC_EXECUTABLE)
    message(STATUS
        "emcc not found; skipping emcc fixture targets "
        "(searched for emcc, emcc.bat, emcc.py; EMSDK='$ENV{EMSDK}'; EMSCRIPTEN='$ENV{EMSCRIPTEN}')."
    )
    return()
endif()

message(STATUS "Using emcc fixture compiler: ${WL_EMCC_EXECUTABLE}")

set(WL_EMCC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/out")
set(WL_EMCC_OUTPUTS)
set(WL_EMCC_LIST_LINES "fixtures:")

function(wl_add_emcc_case name)
    cmake_parse_arguments(WL_CASE "" "" "RUNNER_ARGS;EMCC_FLAGS" ${ARGN})

    set(output "${WL_EMCC_OUT_DIR}/${name}.wasm")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WL_EMCC_OUT_DIR}"
        COMMAND "${WL_EMCC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name}.c"
                -sSTANDALONE_WASM=1
                -Wl,--no-entry
                -Wl,--export=wl_case
                ${WL_CASE_EMCC_FLAGS}
                -o "${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name}.c"
        VERBATIM
    )

    set(WL_EMCC_OUTPUTS ${WL_EMCC_OUTPUTS} "${output}" PARENT_SCOPE)
    string(JOIN " " runner_args_text ${WL_CASE_RUNNER_ARGS})
    set(WL_EMCC_LIST_LINES ${WL_EMCC_LIST_LINES} "  ${name} ${runner_args_text}" PARENT_SCOPE)

    add_test(NAME "emcc.${name}" COMMAND $<TARGET_FILE:wasm_runner> "${output}" ${WL_CASE_RUNNER_ARGS})
    set_tests_properties("emcc.${name}" PROPERTIES
        LABELS "emcc"
        FIXTURES_REQUIRED "emcc-built"
    )
endfunction()

wl_add_emcc_case(basic_add RUNNER_ARGS call-i32 wl_case 10 EMCC_FLAGS -O0)
wl_add_emcc_case(function_pointer RUNNER_ARGS call-i32 wl_case 37 EMCC_FLAGS -O1)
wl_add_emcc_case(bulk_memory RUNNER_ARGS call-i32 wl_case 410 EMCC_FLAGS -O0 -mbulk-memory)
wl_add_emcc_case(sign_extension RUNNER_ARGS call-i32 wl_case -32895 EMCC_FLAGS -O2)
wl_add_emcc_case(switch_table RUNNER_ARGS call-i32 wl_case 40 EMCC_FLAGS -O2)
wl_add_emcc_case(start_constructor RUNNER_ARGS call-i32 wl_case 77 EMCC_FLAGS -O0 -Wl,--export=__wasm_call_ctors)
wl_add_emcc_case(i64_return RUNNER_ARGS call-i64 wl_case 1099511627785 EMCC_FLAGS -O1)
wl_add_emcc_case(trunc_sat RUNNER_ARGS call-i32 wl_case 255 EMCC_FLAGS -O2)
wl_add_emcc_case(tail_call RUNNER_ARGS call-i32 wl_case 29 EMCC_FLAGS -O1 -mtail-call)
wl_add_emcc_case(stdout_print RUNNER_ARGS call-stdout wl_case stdout-fixture-ok EMCC_FLAGS -O0)
wl_add_emcc_case(unresolved_import RUNNER_ARGS load-fail EMCC_FLAGS -O0 -Wl,--allow-undefined)

add_custom_target(wasm-emcc-build DEPENDS wasm_runner ${WL_EMCC_OUTPUTS})

add_test(
    NAME emcc.build
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} ${WL_BUILD_CONFIG_ARGS} --target wasm-emcc-build
)
set_tests_properties(emcc.build PROPERTIES
    LABELS "emcc"
    FIXTURES_SETUP "emcc-built"
)

add_custom_target(wasm-emcc-run
    COMMAND ${CMAKE_COMMAND}
            -DWL_CTEST_COMMAND:FILEPATH=${CMAKE_CTEST_COMMAND}
            -DWL_CTEST_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}
            -DWL_CTEST_LABEL:STRING=emcc
            $<$<BOOL:${WL_GENERATOR_IS_MULTI_CONFIG}>:-DWL_CTEST_CONFIG:STRING=$<CONFIG>>
            -P ${PROJECT_SOURCE_DIR}/cmake/RunCTestNoFail.cmake
    DEPENDS wasm-emcc-build
    USES_TERMINAL
)

add_custom_target(wasm-emcc-run-strict
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --test-dir ${PROJECT_BINARY_DIR} -L emcc ${WL_CTEST_CONFIG_ARGS}
    DEPENDS wasm-emcc-build
    USES_TERMINAL
)

add_custom_target(wasm-emcc DEPENDS wasm-emcc-run)

add_custom_target(wasm-emcc-list
    COMMAND ${CMAKE_COMMAND} -E echo "fixtures:"
    COMMAND ${CMAKE_COMMAND} -E echo "  basic_add            call-i32 wl_case 10"
    COMMAND ${CMAKE_COMMAND} -E echo "  function_pointer     call-i32 wl_case 37"
    COMMAND ${CMAKE_COMMAND} -E echo "  bulk_memory          call-i32 wl_case 410"
    COMMAND ${CMAKE_COMMAND} -E echo "  sign_extension       call-i32 wl_case -32895"
    COMMAND ${CMAKE_COMMAND} -E echo "  switch_table         call-i32 wl_case 40"
    COMMAND ${CMAKE_COMMAND} -E echo "  start_constructor    call-i32 wl_case 77"
    COMMAND ${CMAKE_COMMAND} -E echo "  i64_return           call-i64 wl_case 1099511627785"
    COMMAND ${CMAKE_COMMAND} -E echo "  trunc_sat            call-i32 wl_case 255"
    COMMAND ${CMAKE_COMMAND} -E echo "  tail_call            call-i32 wl_case 29"
    COMMAND ${CMAKE_COMMAND} -E echo "  stdout_print         call-stdout wl_case stdout-fixture-ok"
    COMMAND ${CMAKE_COMMAND} -E echo "  unresolved_import    load-fail"
    USES_TERMINAL
)