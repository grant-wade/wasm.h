#define WASM_IMPL
#include "basic_add.h"

#include <stdio.h>

static void print_runtime_error(const wasm_runtime_t* rt, const char* prefix) {
	const char* error_name;
	const char* error_message;

	if (!rt) return;

	error_name = wasm_error_string(rt->last_error);
	error_message = rt->error_msg[0] ? rt->error_msg : "(no detail)";
	fprintf(stderr, "%s: %s (%s)\n", prefix, error_name, error_message);
}

int main(void) {
	size_t wasm_len = 0;
	wasm_runtime_t rt;
	basic_add_ctx_t* ctx = NULL;
	int exit_code = 1;
	int32_t wl_case_result;

	if (wasm_init(&rt, NULL) != WASM_OK) {
		print_runtime_error(&rt, "wasm_init failed");
		goto cleanup;
	}

	if (wasm_bind_wasi_stubs(&rt) != WASM_OK) {
		print_runtime_error(&rt, "wasm_bind_wasi_stubs failed");
		goto cleanup_runtime;
	}

	ctx = basic_add_init_embedded(&rt);
	(void)basic_add_embedded_wasm(&wasm_len);
	if (!ctx) {
		print_runtime_error(&rt, "basic_add_init_embedded failed");
		goto cleanup_runtime;
	}

	printf("loaded embedded basic_add module (%zu bytes)\n", wasm_len);

	wl_case_result = basic_add_wl_case(ctx);
	printf("wl_case() = %d\n", wl_case_result);
	printf("last error   = %s\n", basic_add_last_error_string(ctx));
	printf("error detail = %s\n", basic_add_last_error_message(ctx));

	exit_code = 0;

	basic_add_free(ctx);

cleanup_runtime:
	wasm_destroy(&rt);
cleanup:
	return exit_code;
}