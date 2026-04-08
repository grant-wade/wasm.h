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


static int run_flat_api(void) {
	wasm_error_t err;
	int exit_code = 1;

	err = session_math_init_embedded(NULL);
	if (err != WASM_OK) {
		report_error("flat init failed");
		goto cleanup;
	}

	if (!expect_i32("flat ready after init", session_math_ready(), 1)) goto cleanup;
	if (!expect_f64("flat add", session_math_add(2.5, 0.75), 3.25, 1e-9)) goto cleanup;
	if (!expect_f64("flat sub", session_math_sub(9.0, 2.5), 6.5, 1e-9)) goto cleanup;
	if (!expect_f64("flat pow", session_math_pow(2.0, 8.0), 256.0, 1e-9)) goto cleanup;
	if (!expect_f64("flat exp", session_math_exp(0.0), 1.0, 1e-9)) goto cleanup;
	if (!expect_i32("flat initial bias", session_math_get_bias(), 7)) goto cleanup;
	if (!expect_i32("flat initial total", session_math_get_total(), 100)) goto cleanup;
	if (!expect_i32("flat first add_scaled", session_math_add_scaled(3, 4), 117)) goto cleanup;

	session_math_set_bias(2);
	if (session_math_get_last_error() != WASM_OK) {
		report_error("flat set_bias failed");
		goto cleanup;
	}
	if (!expect_i32("flat updated bias", session_math_get_bias(), 2)) goto cleanup;
	if (!expect_i32("flat second add_scaled", session_math_add_scaled(1, 5), 126)) goto cleanup;
	if (!expect_i32("flat current total", session_math_get_total(), 126)) goto cleanup;

	session_math_reset_total();
	if (session_math_get_last_error() != WASM_OK) {
		report_error("flat reset_total failed");
		goto cleanup;
	}
	if (!expect_i32("flat reset total", session_math_get_total(), 0)) goto cleanup;

	exit_code = 0;

cleanup:
	session_math_free();
	return exit_code;
}

static int run_object_api(void) {
	session_math_api_t api = session_math_api_init_embedded(NULL);
	int exit_code = 1;

	if (session_math_get_last_error() != WASM_OK) {
		fprintf(stderr, "object init failed: %s (%s)\n",
				session_math_get_last_error_string(),
				session_math_get_last_error_message());
		goto cleanup;
	}

	if (!expect_i32("object ready after init", session_math_ready(), 1)) goto cleanup;
	if (!expect_f64("object add", session_math_add(2.5, 0.75), 3.25, 1e-9)) goto cleanup;
	if (!expect_f64("object sub", session_math_sub(9.0, 2.5), 6.5, 1e-9)) goto cleanup;
	if (!expect_f64("object pow", session_math_pow(2.0, 8.0), 256.0, 1e-9)) goto cleanup;
	if (!expect_f64("object exp", session_math_exp(0.0), 1.0, 1e-9)) goto cleanup;
	if (!expect_i32("object initial bias", session_math_get_bias(), 7)) goto cleanup;
	if (!expect_i32("object initial total", session_math_get_total(), 100)) goto cleanup;
	if (!expect_i32("object first add_scaled", session_math_add_scaled(3, 4), 117)) goto cleanup;

	session_math_set_bias(2);
	if (session_math_get_last_error() != WASM_OK) {
		fprintf(stderr, "object set_bias failed: %s (%s)\n",
				session_math_get_last_error_string(),
				session_math_get_last_error_message());
		goto cleanup;
	}
	if (!expect_i32("object updated bias", session_math_get_bias(), 2)) goto cleanup;
	if (!expect_i32("object second add_scaled", session_math_add_scaled(1, 5), 126)) goto cleanup;
	if (!expect_i32("object current total", session_math_get_total(), 126)) goto cleanup;

	session_math_reset_total();
	if (session_math_get_last_error() != WASM_OK) {
		fprintf(stderr, "object reset_total failed: %s (%s)\n",
				session_math_get_last_error_string(),
				session_math_get_last_error_message());
		goto cleanup;
	}
	if (!expect_i32("object reset total", session_math_get_total(), 0)) goto cleanup;

	exit_code = 0;

cleanup:
	session_math_api_free(&api);
	return exit_code;
}

int main(void) {
	if (run_flat_api() != 0) return 1;
	if (run_object_api() != 0) return 1;

	printf("session_math demo passed\n");
	return 0;
}