cmake_minimum_required(VERSION 3.20)

project(wasm_h VERSION 1.0.0 LANGUAGES C)

include(CheckLibraryExists)
include(CMakePackageConfigHelpers)
include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Honor the pre-release WL_* option names for existing build directories.
if(DEFINED WL_ENABLE_PLATFORM AND NOT DEFINED WASM_H_ENABLE_PLATFORM)
    set(WASM_H_ENABLE_PLATFORM "${WL_ENABLE_PLATFORM}" CACHE BOOL "Enable platform-backed runtime and test helpers.")
endif()
if(DEFINED WL_ENABLE_WERROR AND NOT DEFINED WASM_H_ENABLE_WERROR)
    set(WASM_H_ENABLE_WERROR "${WL_ENABLE_WERROR}" CACHE BOOL "Treat warnings as errors.")
endif()
if(DEFINED WL_ENABLE_EMCC_TESTS AND NOT DEFINED WASM_H_ENABLE_EMCC_TESTS)
    set(WASM_H_ENABLE_EMCC_TESTS "${WL_ENABLE_EMCC_TESTS}" CACHE BOOL "Enable emcc-based Wasm fixture tests when emcc is available.")
endif()

option(WASM_H_ENABLE_PLATFORM "Enable platform-backed runtime and test helpers." ON)
option(WASM_H_ENABLE_WERROR "Treat warnings as errors." ON)
option(WASM_H_ENABLE_EMCC_TESTS "Enable emcc-based Wasm fixture tests when emcc is available." ON)
option(WASM_H_BUILD_FUZZER "Build the libFuzzer wasm_load harness (Clang only)." OFF)

# Internal compatibility variables used by the existing test graph.
set(WL_ENABLE_PLATFORM "${WASM_H_ENABLE_PLATFORM}")
set(WL_ENABLE_WERROR "${WASM_H_ENABLE_WERROR}")
set(WL_ENABLE_EMCC_TESTS "${WASM_H_ENABLE_EMCC_TESTS}")
if(NOT WASM_H_ENABLE_PLATFORM)
    set(WL_ENABLE_EMCC_TESTS OFF)
endif()

if(WASM_H_ENABLE_PLATFORM)
    set(WASM_H_ENABLE_PLATFORM_VALUE 1)
else()
    set(WASM_H_ENABLE_PLATFORM_VALUE 0)
endif()
set(WL_ENABLE_PLATFORM_VALUE "${WASM_H_ENABLE_PLATFORM_VALUE}")

set(WASM_HEADER_STAMP "${PROJECT_BINARY_DIR}/wasm-header.stamp")
add_custom_target(wasm-header ALL
    COMMAND ${CMAKE_COMMAND} -DWASM_AMALGAMATE_STAMP=${WASM_HEADER_STAMP}
            -P ${PROJECT_SOURCE_DIR}/cmake/Amalgamate.cmake
    BYPRODUCTS "${WASM_HEADER_STAMP}"
    COMMENT "Generating distribution wasm.h"
    VERBATIM
)
add_custom_target(wasm-header-check
    COMMAND ${CMAKE_COMMAND} -DWASM_AMALGAMATE_CHECK=ON -P ${PROJECT_SOURCE_DIR}/cmake/Amalgamate.cmake
    VERBATIM
)

add_library(wasm_h INTERFACE)
add_dependencies(wasm_h wasm-header)
add_library(wasm_h::wasm_h ALIAS wasm_h)
target_include_directories(wasm_h INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(wl_project_options INTERFACE)
target_include_directories(wl_project_options INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_definitions(wl_project_options INTERFACE
    _POSIX_C_SOURCE=200809L
    WASM_ENABLE_PLATFORM=${WASM_H_ENABLE_PLATFORM_VALUE}
    WL_ENABLE_PLATFORM=${WASM_H_ENABLE_PLATFORM_VALUE}
)

if(MSVC)
    target_compile_definitions(wl_project_options INTERFACE
        _CRT_SECURE_NO_WARNINGS
        _CRT_RAND_S
    )
    target_compile_options(wl_project_options INTERFACE
        /std:c11
        /W4
        $<$<BOOL:${WASM_H_ENABLE_WERROR}>:/WX>
    )
else()
    target_compile_options(wl_project_options INTERFACE
        -Wall
        -Wextra
        $<$<BOOL:${WASM_H_ENABLE_WERROR}>:-Werror>
    )
endif()

add_library(wl_math INTERFACE)
check_library_exists(m cos "" WL_HAVE_LIBM)
if(WL_HAVE_LIBM)
    target_link_libraries(wl_math INTERFACE m)
    target_link_libraries(wasm_h INTERFACE m)
endif()

# The modular library compiles the same implementation as independent C99 units.
set(WASM_RUNTIME_SOURCES
    src/call.c
    src/decode.c
    src/exec/calls.c
    src/exec/control.c
    src/exec/exceptions.c
    src/exec/frames.c
    src/exec/gc.c
    src/exec/bulk.c
    src/exec/simd.c
    src/exec/numeric.c
    src/exec/interpreter.c
    src/gc.c
    src/init_expr.c
    src/inspect.c
    src/load.c
    src/memory.c
    src/memory_internal.c
    src/module.c
    src/platform/disabled.c
    src/platform/emscripten.c
    src/platform/wasi_bind.c
    src/platform/wasi_fd.c
    src/platform/wasi_io.c
    src/platform/wasi_path.c
    src/platform/wasi_state.c
    src/portable.c
    src/reader.c
    src/runtime.c
    src/simd/compare.c
    src/simd/float.c
    src/simd/integer_small.c
    src/simd/integer_wide.c
    src/simd/lanes.c
    src/support.c
    src/types/canonical.c
    src/types/cross_module.c
    src/types/decode.c
    src/types/references.c
    src/validate/function.c
    src/validate/gc.c
    src/validate/module.c
    src/validate/numeric.c
    src/validate/simd.c
    src/validate/stack.c
    src/values.c
)
add_library(wasm_runtime STATIC ${WASM_RUNTIME_SOURCES})
add_library(wasm_h::runtime ALIAS wasm_runtime)
target_include_directories(wasm_runtime PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(wasm_runtime PRIVATE wl_project_options PUBLIC wl_math)

get_property(WL_GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(WL_BUILD_CONFIG_ARGS)
set(WL_CTEST_CONFIG_ARGS)
if(WL_GENERATOR_IS_MULTI_CONFIG)
    list(APPEND WL_BUILD_CONFIG_ARGS --config $<CONFIG>)
    list(APPEND WL_CTEST_CONFIG_ARGS -C $<CONFIG>)
endif()

function(wl_add_native_executable target)
    add_executable(${target} ${ARGN})
    add_dependencies(${target} wasm-header)
    set_property(SOURCE ${ARGN} APPEND PROPERTY OBJECT_DEPENDS "${WASM_HEADER_STAMP}")
    target_link_libraries(${target} PRIVATE wl_project_options wl_math)
endfunction()

wl_add_native_executable(wasm2api wasm2api.c)
wl_add_native_executable(wasm wasm.c)

add_subdirectory(examples)

if(BUILD_TESTING)
    add_subdirectory(test)

    set(WL_CHECK_TARGET_DEPS wl_test wasm_test wasm_modular_test wasm_core_test wasm_modular_core_test spectest_modular_runner)
    if(TARGET session_math_demo)
        list(APPEND WL_CHECK_TARGET_DEPS session_math_demo)
    endif()
    if(TARGET wasm-spectest-build)
        list(APPEND WL_CHECK_TARGET_DEPS wasm-spectest-build)
    endif()
    if(TARGET wasm-emcc-build)
        list(APPEND WL_CHECK_TARGET_DEPS wasm-emcc-build)
    endif()

    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --test-dir ${PROJECT_BINARY_DIR} ${WL_CTEST_CONFIG_ARGS}
        DEPENDS ${WL_CHECK_TARGET_DEPS}
        USES_TERMINAL
    )
endif()

if(WASM_H_BUILD_FUZZER)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "WASM_H_BUILD_FUZZER requires Clang with libFuzzer support.")
    endif()
    add_executable(wasm_load_fuzzer test/fuzz_wasm_load.c)
    add_dependencies(wasm_load_fuzzer wasm-header)
    set_property(SOURCE test/fuzz_wasm_load.c APPEND PROPERTY OBJECT_DEPENDS "${WASM_HEADER_STAMP}")
    target_include_directories(wasm_load_fuzzer PRIVATE ${PROJECT_SOURCE_DIR})
    target_compile_options(wasm_load_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(wasm_load_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(wasm_load_fuzzer PRIVATE wl_math)
endif()

install(FILES wasm.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS wasm_h EXPORT wasm_hTargets)
install(EXPORT wasm_hTargets
    FILE wasm_hTargets.cmake
    NAMESPACE wasm_h::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wasm_h
)
configure_package_config_file(
    cmake/wasm_hConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wasm_hConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wasm_h
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wasm_hConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/wasm_hConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/wasm_hConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wasm_h
)
