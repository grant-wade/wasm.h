#include <stdio.h>

#define WASM_IMPL
#include "session_math.h"

static int report_init_error(session_math_ctx_t* ctx, const char* prefix) {
	if (!ctx) {
		fprintf(stderr, "%s: init returned NULL\n", prefix);
		return 1;
	}

	fprintf(stderr, "%s: %s (%s)\n",
			prefix,
			session_math_last_error_string(ctx),
			session_math_last_error_message(ctx));
	return 1;
}

static int expect_i32(const char* label, int32_t actual, int32_t expected) {
	if (actual == expected) return 1;

	fprintf(stderr, "%s: expected %d, got %d\n", label, expected, actual);
	return 0;
}

int main(void) {
	session_math_ctx_t* ctx = NULL;
	int exit_code = 1;

	ctx = session_math_init_embedded(NULL);
	if (!ctx) return report_init_error(ctx, "session_math_init_embedded failed");
	if (session_math_last_error(ctx) != WASM_OK) {
		report_init_error(ctx, "session_math_init_embedded failed");
		goto cleanup;
	}

	if (!expect_i32("ready after init", session_math_ready(ctx), 1)) goto cleanup;
	if (!expect_i32("initial bias", session_math_get_bias(ctx), 7)) goto cleanup;
	if (!expect_i32("initial total", session_math_get_total(ctx), 100)) goto cleanup;
	if (!expect_i32("first add_scaled", session_math_add_scaled(ctx, 3, 4), 117)) goto cleanup;

	session_math_set_bias(ctx, 2);
	if (session_math_last_error(ctx) != WASM_OK) {
		report_init_error(ctx, "session_math_set_bias failed");
		goto cleanup;
	}
	if (!expect_i32("updated bias", session_math_get_bias(ctx), 2)) goto cleanup;
	if (!expect_i32("second add_scaled", session_math_add_scaled(ctx, 1, 5), 126)) goto cleanup;
	if (!expect_i32("current total", session_math_get_total(ctx), 126)) goto cleanup;

	session_math_reset_total(ctx);
	if (session_math_last_error(ctx) != WASM_OK) {
		report_init_error(ctx, "session_math_reset_total failed");
		goto cleanup;
	}
	if (!expect_i32("reset total", session_math_get_total(ctx), 0)) goto cleanup;

	printf("session_math demo passed\n");
	exit_code = 0;

cleanup:
	session_math_free(ctx);
	return exit_code;
}