cmake_minimum_required(VERSION 3.20)

project(wl VERSION 0.1.0 LANGUAGES C)

include(CheckLibraryExists)
include(CTest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(WL_ENABLE_PLATFORM "Enable platform-backed wl.h APIs." ON)
option(WL_ENABLE_WERROR "Treat warnings as errors." ON)
option(WL_ENABLE_EMCC_TESTS "Enable emcc-based Wasm fixture tests when emcc is available." ON)

if(WL_ENABLE_PLATFORM)
    set(WL_ENABLE_PLATFORM_VALUE 1)
else()
    set(WL_ENABLE_PLATFORM_VALUE 0)
endif()

add_library(wl_project_options INTERFACE)
target_include_directories(wl_project_options INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_definitions(wl_project_options INTERFACE
    _POSIX_C_SOURCE=200809L
    WL_ENABLE_PLATFORM=${WL_ENABLE_PLATFORM_VALUE}
)

if(MSVC)
    target_compile_definitions(wl_project_options INTERFACE
        _CRT_SECURE_NO_WARNINGS
        _CRT_RAND_S
    )
    target_compile_options(wl_project_options INTERFACE
        /std:c11
        /W4
        $<$<BOOL:${WL_ENABLE_WERROR}>:/WX>
    )
else()
    target_compile_options(wl_project_options INTERFACE
        -Wall
        -Wextra
        $<$<BOOL:${WL_ENABLE_WERROR}>:-Werror>
    )
endif()

add_library(wl_math INTERFACE)
check_library_exists(m cos "" WL_HAVE_LIBM)
if(WL_HAVE_LIBM)
    target_link_libraries(wl_math INTERFACE m)
endif()

get_property(WL_GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(WL_BUILD_CONFIG_ARGS)
set(WL_CTEST_CONFIG_ARGS)
if(WL_GENERATOR_IS_MULTI_CONFIG)
    list(APPEND WL_BUILD_CONFIG_ARGS --config $<CONFIG>)
    list(APPEND WL_CTEST_CONFIG_ARGS -C $<CONFIG>)
endif()

function(wl_add_native_executable target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE wl_project_options wl_math)
endfunction()

wl_add_native_executable(wl_test wl_test.c)
wl_add_native_executable(wasm_test wasm_test.c)
wl_add_native_executable(wasm2api wasm2api.c)
wl_add_native_executable(wasm wasm.c)

add_subdirectory(examples)

if(BUILD_TESTING)
    add_test(NAME wl_test COMMAND wl_test)
    set_tests_properties(wl_test PROPERTIES LABELS "unit")

    add_test(NAME wasm_test COMMAND wasm_test)
    set_tests_properties(wasm_test PROPERTIES LABELS "unit")

    add_subdirectory(test)

    set(WL_CHECK_TARGET_DEPS wl_test wasm_test)
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