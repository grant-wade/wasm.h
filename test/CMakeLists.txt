wl_add_native_executable(wasm_runner fixture_runner.c)
wl_add_native_executable(spectest_runner spectest_runner.c)
wl_add_native_executable(component_runner component_runner.c)

find_program(WL_WASM_TOOLS_EXECUTABLE
    NAMES wasm-tools
    DOC "Path to the system wasm-tools executable used for spectest JSON generation."
)

set(WL_WASM_TOOLS_JSON_FROM_WAST_AVAILABLE OFF)
set(WL_WASM_TOOLS_PARSE_AVAILABLE OFF)
set(WL_WASM_TOOLS_COMPONENT_EMBED_AVAILABLE OFF)
set(WL_WASM_TOOLS_COMPONENT_NEW_AVAILABLE OFF)
if(WL_WASM_TOOLS_EXECUTABLE)
    execute_process(
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" json-from-wast --help
        RESULT_VARIABLE WL_WASM_TOOLS_JSON_FROM_WAST_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(WL_WASM_TOOLS_JSON_FROM_WAST_RESULT EQUAL 0)
        set(WL_WASM_TOOLS_JSON_FROM_WAST_AVAILABLE ON)
    endif()

    execute_process(
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" parse --help
        RESULT_VARIABLE WL_WASM_TOOLS_PARSE_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(WL_WASM_TOOLS_PARSE_RESULT EQUAL 0)
        set(WL_WASM_TOOLS_PARSE_AVAILABLE ON)
    endif()

    execute_process(
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" component embed --help
        RESULT_VARIABLE WL_WASM_TOOLS_COMPONENT_EMBED_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(WL_WASM_TOOLS_COMPONENT_EMBED_RESULT EQUAL 0)
        set(WL_WASM_TOOLS_COMPONENT_EMBED_AVAILABLE ON)
    endif()

    execute_process(
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" component new --help
        RESULT_VARIABLE WL_WASM_TOOLS_COMPONENT_NEW_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(WL_WASM_TOOLS_COMPONENT_NEW_RESULT EQUAL 0)
        set(WL_WASM_TOOLS_COMPONENT_NEW_AVAILABLE ON)
    endif()

    if(NOT WL_WASM_TOOLS_JSON_FROM_WAST_AVAILABLE OR NOT WL_WASM_TOOLS_PARSE_AVAILABLE)
        message(STATUS
            "wasm-tools found at ${WL_WASM_TOOLS_EXECUTABLE} but required subcommands are unavailable; skipping spectest targets."
        )
    endif()
else()
    message(STATUS "wasm-tools not found; skipping spectest targets.")
endif()

if(WL_WASM_TOOLS_JSON_FROM_WAST_AVAILABLE AND WL_WASM_TOOLS_PARSE_AVAILABLE)
    set(WL_SPECTEST_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/spec")
    set(WL_SPECTEST_SUPPORT_WASM "${CMAKE_CURRENT_BINARY_DIR}/spectest_support.wasm")
    set(WL_SPECTEST_FIXTURE "spectest-built")

    cmake_host_system_information(RESULT WL_SPECTEST_PARALLEL_LEVEL QUERY NUMBER_OF_LOGICAL_CORES)
    if(NOT WL_SPECTEST_PARALLEL_LEVEL OR WL_SPECTEST_PARALLEL_LEVEL LESS 1)
        set(WL_SPECTEST_PARALLEL_LEVEL 1)
    endif()

    add_custom_command(
        OUTPUT "${WL_SPECTEST_SUPPORT_WASM}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" parse
                "${CMAKE_CURRENT_SOURCE_DIR}/spectest_support.wat"
                -o "${WL_SPECTEST_SUPPORT_WASM}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/spectest_support.wat"
        VERBATIM
    )

    add_custom_target(spectest-support-build DEPENDS "${WL_SPECTEST_SUPPORT_WASM}")

    file(GLOB WL_SPECTEST_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/spec/*.wast")

    set(WL_SPECTEST_BUILD_TARGETS spectest_runner spectest-support-build)
    foreach(WL_SPECTEST_FILE IN LISTS WL_SPECTEST_FILES)
        get_filename_component(WL_SPECTEST_NAME "${WL_SPECTEST_FILE}" NAME_WE)
        string(MAKE_C_IDENTIFIER "${WL_SPECTEST_NAME}" WL_SPECTEST_ID)
        set(WL_SPECTEST_JSON "${WL_SPECTEST_OUT_DIR}/${WL_SPECTEST_NAME}.json")
        set(WL_SPECTEST_STATUS "${WL_SPECTEST_OUT_DIR}/${WL_SPECTEST_NAME}.status")
        set(WL_SPECTEST_LOG "${WL_SPECTEST_OUT_DIR}/${WL_SPECTEST_NAME}.json-from-wast.log")
        set(WL_SPECTEST_TARGET "spectest_case_${WL_SPECTEST_ID}")

        add_custom_command(
            OUTPUT
                "${WL_SPECTEST_JSON}"
                "${WL_SPECTEST_STATUS}"
            BYPRODUCTS "${WL_SPECTEST_LOG}"
            COMMAND ${CMAKE_COMMAND}
                    -DWL_WASM_TOOLS:FILEPATH=${WL_WASM_TOOLS_EXECUTABLE}
                    -DWL_INPUT:FILEPATH=${WL_SPECTEST_FILE}
                    -DWL_OUTPUT_JSON:FILEPATH=${WL_SPECTEST_JSON}
                    -DWL_STATUS:FILEPATH=${WL_SPECTEST_STATUS}
                    -DWL_LOG:FILEPATH=${WL_SPECTEST_LOG}
                    -P ${PROJECT_SOURCE_DIR}/cmake/RunWast2JsonSafe.cmake
            DEPENDS
                "${WL_SPECTEST_FILE}"
                ${PROJECT_SOURCE_DIR}/cmake/RunWast2JsonSafe.cmake
            VERBATIM
        )

        add_custom_target("${WL_SPECTEST_TARGET}"
            DEPENDS
                spectest_runner
                spectest-support-build
                "${WL_SPECTEST_JSON}"
        )
        list(APPEND WL_SPECTEST_BUILD_TARGETS "${WL_SPECTEST_TARGET}")

        add_test(
            NAME "spectest.${WL_SPECTEST_NAME}"
            COMMAND $<TARGET_FILE:spectest_runner>
                    "${WL_SPECTEST_JSON}"
                    "${WL_WASM_TOOLS_EXECUTABLE}"
                    "${WL_SPECTEST_SUPPORT_WASM}"
                    "${WL_SPECTEST_STATUS}"
        )
        set_tests_properties("spectest.${WL_SPECTEST_NAME}" PROPERTIES
            LABELS "spectest"
            FIXTURES_REQUIRED "${WL_SPECTEST_FIXTURE}"
            SKIP_RETURN_CODE 125
        )
    endforeach()

    add_custom_target(wasm-spectest-build DEPENDS ${WL_SPECTEST_BUILD_TARGETS})

    add_test(
        NAME spectest.build
        COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} ${WL_BUILD_CONFIG_ARGS} --target wasm-spectest-build
    )
    set_tests_properties(spectest.build PROPERTIES
        LABELS "spectest;build"
        FIXTURES_SETUP "${WL_SPECTEST_FIXTURE}"
    )

    add_custom_target(wasm-spectest-run
        COMMAND ${CMAKE_COMMAND}
                -DWL_CTEST_COMMAND:FILEPATH=${CMAKE_CTEST_COMMAND}
                -DWL_CTEST_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}
                -DWL_CTEST_LABEL:STRING=spectest
                -DWL_CTEST_PARALLEL:STRING=${WL_SPECTEST_PARALLEL_LEVEL}
                $<$<BOOL:${WL_GENERATOR_IS_MULTI_CONFIG}>:-DWL_CTEST_CONFIG:STRING=$<CONFIG>>
                -P ${PROJECT_SOURCE_DIR}/cmake/RunCTestNoFail.cmake
        DEPENDS wasm-spectest-build
        USES_TERMINAL
    )

    add_custom_target(wasm-spectest-run-strict
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel ${WL_SPECTEST_PARALLEL_LEVEL} --test-dir ${PROJECT_BINARY_DIR} -L spectest ${WL_CTEST_CONFIG_ARGS}
        DEPENDS wasm-spectest-build
        USES_TERMINAL
    )

    add_custom_target(wasm-spectest DEPENDS wasm-spectest-run)
endif()

find_program(WL_WASMTIME_EXECUTABLE
    NAMES wasmtime
    DOC "Path to the system wasmtime executable used for future component reference comparisons."
)

if(WL_WASM_TOOLS_COMPONENT_EMBED_AVAILABLE AND WL_WASM_TOOLS_COMPONENT_NEW_AVAILABLE)
    set(WL_COMPONENT_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/component")
    set(WL_COMPONENT_SMOKE_WIT "${CMAKE_CURRENT_SOURCE_DIR}/component_smoke.wit")
    set(WL_COMPONENT_SMOKE_DUMMY "${WL_COMPONENT_OUT_DIR}/smoke.dummy.wasm")
    set(WL_COMPONENT_SMOKE_WASM "${WL_COMPONENT_OUT_DIR}/smoke.component.wasm")
    set(WL_COMPONENT_BUILD_OUTPUTS "${WL_COMPONENT_SMOKE_WASM}")
    set(WL_COMPONENT_FIXTURE "wasi-component-built")

    add_custom_command(
        OUTPUT "${WL_COMPONENT_SMOKE_DUMMY}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WL_COMPONENT_OUT_DIR}"
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" component embed
                --dummy
                --world smoke
                "${WL_COMPONENT_SMOKE_WIT}"
                -o "${WL_COMPONENT_SMOKE_DUMMY}"
        DEPENDS
            "${WL_COMPONENT_SMOKE_WIT}"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${WL_COMPONENT_SMOKE_WASM}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WL_COMPONENT_OUT_DIR}"
        COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" component new
                "${WL_COMPONENT_SMOKE_DUMMY}"
                -o "${WL_COMPONENT_SMOKE_WASM}"
        DEPENDS
            "${WL_COMPONENT_SMOKE_DUMMY}"
        VERBATIM
    )

    if(WL_WASM_TOOLS_PARSE_AVAILABLE)
        set(WL_COMPONENT_INSTANCE_SMOKE_WAT "${CMAKE_CURRENT_SOURCE_DIR}/component_instance_smoke.wat")
        set(WL_COMPONENT_INSTANCE_SMOKE_WASM "${WL_COMPONENT_OUT_DIR}/instance-smoke.component.wasm")
        set(WL_COMPONENT_START_SMOKE_WAT "${CMAKE_CURRENT_SOURCE_DIR}/component_start_smoke.wat")
        set(WL_COMPONENT_START_SMOKE_WASM "${WL_COMPONENT_OUT_DIR}/start-smoke.component.wasm")

        add_custom_command(
            OUTPUT "${WL_COMPONENT_INSTANCE_SMOKE_WASM}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WL_COMPONENT_OUT_DIR}"
            COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" parse
                    "${WL_COMPONENT_INSTANCE_SMOKE_WAT}"
                    -o "${WL_COMPONENT_INSTANCE_SMOKE_WASM}"
            DEPENDS
                "${WL_COMPONENT_INSTANCE_SMOKE_WAT}"
            VERBATIM
        )

        add_custom_command(
            OUTPUT "${WL_COMPONENT_START_SMOKE_WASM}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WL_COMPONENT_OUT_DIR}"
            COMMAND "${WL_WASM_TOOLS_EXECUTABLE}" parse
                    "${WL_COMPONENT_START_SMOKE_WAT}"
                    -o "${WL_COMPONENT_START_SMOKE_WASM}"
            DEPENDS
                "${WL_COMPONENT_START_SMOKE_WAT}"
            VERBATIM
        )

        list(APPEND WL_COMPONENT_BUILD_OUTPUTS "${WL_COMPONENT_INSTANCE_SMOKE_WASM}")
        list(APPEND WL_COMPONENT_BUILD_OUTPUTS "${WL_COMPONENT_START_SMOKE_WASM}")
    endif()

    add_custom_target(wasi-component-build DEPENDS component_runner ${WL_COMPONENT_BUILD_OUTPUTS})

    if(BUILD_TESTING)
        add_test(
            NAME wasi.component.build
            COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} ${WL_BUILD_CONFIG_ARGS} --target wasi-component-build
        )
        set_tests_properties(wasi.component.build PROPERTIES
            LABELS "wasi-component;build"
            FIXTURES_SETUP "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.smoke
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" load-status parsed-sections
        )
        set_tests_properties(wasi.component.smoke PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.dump
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" dump-contains "section[0]"
        )
        set_tests_properties(wasi.component.dump PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.modules
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" core-module-count 2
        )
        set_tests_properties(wasi.component.modules PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.imports
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" import-name 0 host-log
        )
        set_tests_properties(wasi.component.imports PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.exports
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" export-name 0 ping
        )
        set_tests_properties(wasi.component.exports PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.import-type-params
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" type-param-count 0 2
        )
        set_tests_properties(wasi.component.import-type-params PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.export-type-params
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" export-type 0 1
        )
        set_tests_properties(wasi.component.export-type-params PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.export-type-results
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" type-result-count 1 1
        )
        set_tests_properties(wasi.component.export-type-results PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.aliases
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" alias-count 5
        )
        set_tests_properties(wasi.component.aliases PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.alias-names
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" alias-name 0 cm32p2_memory
        )
        set_tests_properties(wasi.component.alias-names PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.instances
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" core-instance-count 4
        )
        set_tests_properties(wasi.component.instances PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.instance-kinds
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" core-instance-kind 0 from-exports
        )
        set_tests_properties(wasi.component.instance-kinds PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.instance-exports
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" core-instance-export-name 0 0 host-log
        )
        set_tests_properties(wasi.component.instance-exports PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.instance-modules
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" core-instance-module 1 0
        )
        set_tests_properties(wasi.component.instance-modules PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.instance-args
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" core-instance-arg-name 1 0 cm32p2
        )
        set_tests_properties(wasi.component.instance-args PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.start-present
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_START_SMOKE_WASM}" start-present 1
        )
        set_tests_properties(wasi.component.start-present PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.start-func
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_START_SMOKE_WASM}" start-func 0
        )
        set_tests_properties(wasi.component.start-func PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.start-args
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_START_SMOKE_WASM}" start-args 0
        )
        set_tests_properties(wasi.component.start-args PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.start-results
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_START_SMOKE_WASM}" start-results 0
        )
        set_tests_properties(wasi.component.start-results PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        if(WL_WASM_TOOLS_PARSE_AVAILABLE)
            add_test(
                NAME wasi.component.component-instances
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" component-instance-count 2
            )
            set_tests_properties(wasi.component.component-instances PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.component-instance-kinds
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" component-instance-kind 0 from-exports
            )
            set_tests_properties(wasi.component.component-instance-kinds PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.component-instance-exports
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" component-instance-export-name 0 0 host-log
            )
            set_tests_properties(wasi.component.component-instance-exports PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.component-instance-components
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" component-instance-component 1 0
            )
            set_tests_properties(wasi.component.component-instance-components PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.component-instance-args
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" component-instance-arg-name 1 0 host-log
            )
            set_tests_properties(wasi.component.component-instance-args PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.nested-components
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" nested-component-count 1
            )
            set_tests_properties(wasi.component.nested-components PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.nested-component-sections
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" nested-component-section-count 0 2
            )
            set_tests_properties(wasi.component.nested-component-sections PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

            add_test(
                NAME wasi.component.nested-component-imports
                COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_INSTANCE_SMOKE_WASM}" nested-component-import-name 0 0 host-log
            )
            set_tests_properties(wasi.component.nested-component-imports PROPERTIES
                LABELS "wasi-component"
                FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
            )

        endif()

        add_test(
            NAME wasi.component.canons
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" canon-count 2
        )
        set_tests_properties(wasi.component.canons PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.canon-kinds
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" canon-kind 0 lower
        )
        set_tests_properties(wasi.component.canon-kinds PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )

        add_test(
            NAME wasi.component.canon-options
            COMMAND $<TARGET_FILE:component_runner> "${WL_COMPONENT_SMOKE_WASM}" canon-option-count 1 1
        )
        set_tests_properties(wasi.component.canon-options PROPERTIES
            LABELS "wasi-component"
            FIXTURES_REQUIRED "${WL_COMPONENT_FIXTURE}"
        )
    endif()

    add_custom_target(wasi-component-run
        COMMAND ${CMAKE_COMMAND}
                -DWL_CTEST_COMMAND:FILEPATH=${CMAKE_CTEST_COMMAND}
                -DWL_CTEST_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}
                -DWL_CTEST_LABEL:STRING=wasi-component
                $<$<BOOL:${WL_GENERATOR_IS_MULTI_CONFIG}>:-DWL_CTEST_CONFIG:STRING=$<CONFIG>>
                -P ${PROJECT_SOURCE_DIR}/cmake/RunCTestNoFail.cmake
        DEPENDS wasi-component-build
        USES_TERMINAL
    )

    add_custom_target(wasi-component-run-strict
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --test-dir ${PROJECT_BINARY_DIR} -L wasi-component ${WL_CTEST_CONFIG_ARGS}
        DEPENDS wasi-component-build
        USES_TERMINAL
    )
else()
    message(STATUS "wasm-tools component subcommands unavailable; skipping wasi component smoke targets.")
endif()

if(WL_WASMTIME_EXECUTABLE)
    message(STATUS "Found wasmtime at ${WL_WASMTIME_EXECUTABLE}; reference comparison hooks can be enabled as component execution coverage grows.")
else()
    message(STATUS "wasmtime not found; skipping component reference comparison targets.")
endif()

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