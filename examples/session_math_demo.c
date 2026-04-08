#include <stdio.h>

#define WASM_IMPL
#include "session_math.h"

static int report_error(const char* prefix) {
	fprintf(stderr, "%s: %s (%s)\n",
			prefix,
			session_math_get_last_error_string(),
			session_math_get_last_error_message());
	return 1;
}

static int expect_i32(const char* label, int32_t actual, int32_t expected) {
	if (actual == expected) return 1;

	fprintf(stderr, "%s: expected %d, got %d\n", label, expected, actual);
	return 0;
}

static int expect_f64(const char* label, double actual, double expected, double tolerance) {
	double delta = actual - expected;

	if (delta < 0.0) delta = -delta;
	if (delta <= tolerance) return 1;

	fprintf(stderr, "%s: expected %.9f, got %.9f\n", label, expected, actual);
	return 0;
}

int main(void) {
	wasm_error_t err;
	int exit_code = 1;

	err = session_math_init_embedded(NULL);
	if (err != WASM_OK) {
		report_error("session_math_init_embedded failed");
		goto cleanup;
	}

	if (!expect_i32("ready after init", session_math_ready(), 1)) goto cleanup;
	if (!expect_f64("add", session_math_add(2.5, 0.75), 3.25, 1e-9)) goto cleanup;
	if (!expect_f64("sub", session_math_sub(9.0, 2.5), 6.5, 1e-9)) goto cleanup;
	if (!expect_f64("pow", session_math_pow(2.0, 8.0), 256.0, 1e-9)) goto cleanup;
	if (!expect_f64("exp", session_math_exp(0.0), 1.0, 1e-9)) goto cleanup;
	if (!expect_i32("initial bias", session_math_get_bias(), 7)) goto cleanup;
	if (!expect_i32("initial total", session_math_get_total(), 100)) goto cleanup;
	if (!expect_i32("first add_scaled", session_math_add_scaled(3, 4), 117)) goto cleanup;

	session_math_set_bias(2);
	if (session_math_get_last_error() != WASM_OK) {
		report_error("session_math_set_bias failed");
		goto cleanup;
	}
	if (!expect_i32("updated bias", session_math_get_bias(), 2)) goto cleanup;
	if (!expect_i32("second add_scaled", session_math_add_scaled(1, 5), 126)) goto cleanup;
	if (!expect_i32("current total", session_math_get_total(), 126)) goto cleanup;

	session_math_reset_total();
	if (session_math_get_last_error() != WASM_OK) {
		report_error("session_math_reset_total failed");
		goto cleanup;
	}
	if (!expect_i32("reset total", session_math_get_total(), 0)) goto cleanup;

	printf("session_math demo passed\n");
	exit_code = 0;

cleanup:
	session_math_free();
	return exit_code;
}