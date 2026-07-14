set(WASM_H_EXAMPLES_EMCC_SEARCH_HINTS)
if(DEFINED ENV{EMSDK})
	list(APPEND WASM_H_EXAMPLES_EMCC_SEARCH_HINTS
		"$ENV{EMSDK}"
		"$ENV{EMSDK}/upstream/emscripten"
	)
endif()
if(DEFINED ENV{EMSCRIPTEN})
	list(APPEND WASM_H_EXAMPLES_EMCC_SEARCH_HINTS "$ENV{EMSCRIPTEN}")
endif()

find_program(WASM_H_EXAMPLES_EMCC_EXECUTABLE
	NAMES emcc emcc.bat emcc.py
	HINTS ${WASM_H_EXAMPLES_EMCC_SEARCH_HINTS}
	DOC "Path to emcc used for wasm2api example builds."
)

if(WASM_H_EXAMPLES_EMCC_EXECUTABLE)
	message(STATUS "Using emcc example compiler: ${WASM_H_EXAMPLES_EMCC_EXECUTABLE}")

	set(SESSION_MATH_WASM "${CMAKE_CURRENT_BINARY_DIR}/session_math.wasm")
	set(SESSION_MATH_GEN_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/session_math")
	set(SESSION_MATH_GEN_H "${SESSION_MATH_GEN_PREFIX}.h")
	set(SESSION_MATH_GEN_C "${SESSION_MATH_GEN_PREFIX}.c")

	add_custom_command(
		OUTPUT "${SESSION_MATH_WASM}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
		COMMAND "${WASM_H_EXAMPLES_EMCC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/session_math_wasm.c"
				-sSTANDALONE_WASM=1
				-Wl,--no-entry
				-Wl,--export=add
				-Wl,--export=sub
				-Wl,--export=init_state
				-Wl,--export=ready
				-Wl,--export=add_scaled
				-Wl,--export=set_bias
				-Wl,--export=get_bias
				-Wl,--export=get_total
				-Wl,--export=reset_total
				-O1
				-o "${SESSION_MATH_WASM}"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/session_math_wasm.c" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
		VERBATIM
	)
	add_custom_target(session_math_wasm DEPENDS "${SESSION_MATH_WASM}")

	add_custom_command(
		OUTPUT "${SESSION_MATH_GEN_H}" "${SESSION_MATH_GEN_C}"
		COMMAND $<TARGET_FILE:wasm2api> --singleton --embed --init-func init_state "${SESSION_MATH_WASM}" "${SESSION_MATH_GEN_PREFIX}"
		DEPENDS wasm2api "${SESSION_MATH_WASM}" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
		VERBATIM
	)
	add_custom_target(session_math_generate DEPENDS "${SESSION_MATH_GEN_H}" "${SESSION_MATH_GEN_C}")

	wl_add_native_executable(session_math_demo session_math_demo.c "${SESSION_MATH_GEN_C}")
	add_dependencies(session_math_demo session_math_generate)

	if(BUILD_TESTING)
		add_test(NAME session_math_demo COMMAND session_math_demo)
		set_tests_properties(session_math_demo PROPERTIES LABELS "unit")
	endif()
else()
	message(STATUS
		"emcc not found; skipping session_math wasm2api example "
		"(searched for emcc, emcc.bat, emcc.py; EMSDK='$ENV{EMSDK}'; EMSCRIPTEN='$ENV{EMSCRIPTEN}')."
	)
endif()
