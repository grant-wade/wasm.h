#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/portable.h"
#endif

#if WASM_ENABLE_PLATFORM
static uint32_t wasm_test_wasi_call(test_ctx* t, wasm_module_t* m, const char* name, const wasm_value_t* args,
                                    uint32_t num_args) {
    wasm_value_t result = wasm_i32(-1);

    WASM_CHECK_OK(t, wasm_call(m, name, args, num_args, &result, 1));
    return (uint32_t)result.of.i32;
}

TEST(test_wasi_config_args_env_preopens_and_poll) {
    /* Each export forwards its arguments to the wasi_snapshot_preview1 import of the same name. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x03, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60,
        0x03, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x04, 0x7f, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x02, 0xf0, 0x01, 0x06,
        0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72, 0x65,
        0x76, 0x69, 0x65, 0x77, 0x31, 0x0e, 0x61, 0x72, 0x67, 0x73, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x73, 0x5f, 0x67,
        0x65, 0x74, 0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74,
        0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x08, 0x61, 0x72, 0x67, 0x73, 0x5f, 0x67, 0x65, 0x74,
        0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70,
        0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x11, 0x65, 0x6e, 0x76, 0x69, 0x72, 0x6f, 0x6e, 0x5f, 0x73, 0x69,
        0x7a, 0x65, 0x73, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61,
        0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x0e, 0x66, 0x64, 0x5f,
        0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69,
        0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31,
        0x13, 0x66, 0x64, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74, 0x5f, 0x64, 0x69, 0x72, 0x5f, 0x6e, 0x61,
        0x6d, 0x65, 0x00, 0x01, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74,
        0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x0b, 0x70, 0x6f, 0x6c, 0x6c, 0x5f, 0x6f, 0x6e, 0x65,
        0x6f, 0x66, 0x66, 0x00, 0x02, 0x03, 0x07, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x05, 0x03, 0x01, 0x00,
        0x01, 0x07, 0x6f, 0x07, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x0e, 0x61, 0x72, 0x67, 0x73,
        0x5f, 0x73, 0x69, 0x7a, 0x65, 0x73, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x06, 0x08, 0x61, 0x72, 0x67, 0x73, 0x5f,
        0x67, 0x65, 0x74, 0x00, 0x07, 0x11, 0x65, 0x6e, 0x76, 0x69, 0x72, 0x6f, 0x6e, 0x5f, 0x73, 0x69, 0x7a, 0x65,
        0x73, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x08, 0x0e, 0x66, 0x64, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74,
        0x5f, 0x67, 0x65, 0x74, 0x00, 0x09, 0x13, 0x66, 0x64, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74, 0x5f,
        0x64, 0x69, 0x72, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x00, 0x0a, 0x0b, 0x70, 0x6f, 0x6c, 0x6c, 0x5f, 0x6f, 0x6e,
        0x65, 0x6f, 0x66, 0x66, 0x00, 0x0b, 0x0a, 0x3d, 0x06, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x00, 0x0b,
        0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x01, 0x0b, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x02, 0x0b,
        0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x03, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x10,
        0x04, 0x0b, 0x0c, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x20, 0x03, 0x10, 0x05, 0x0b,
    };
    static const char* const argv[] = { "prog", "a b" };
    static const char* const env[] = { "KEY=value" };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[4];
    uint8_t buf[64];
    uint32_t value = 0;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_wasi_set_args(&rt, 2, argv));
    WASM_CHECK_OK(t, wasm_bind_wasi_stubs(&rt));
    WASM_CHECK_OK(t, wasm_wasi_set_env(&rt, 1, env));
    WASM_CHECK_OK(t, wasm_wasi_preopen_dir(&rt, ".", "/work"));
    CHECK(t, wasm_wasi_preopen_dir(&rt, "wasm_test_missing_dir/nested", "/missing") == WASM_ERR_MALFORMED);
    CHECK_MSG(t, strstr(rt.error_msg, "wasm_test_missing_dir") != NULL, "%s", rt.error_msg);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(0);
    args[1] = wasm_i32(4);
    CHECK(t, wasm_test_wasi_call(t, m, "args_sizes_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 0, &value, 4));
    CHECK(t, value == 2u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 4, &value, 4));
    CHECK(t, value == 9u);
    args[0] = wasm_i32(16);
    args[1] = wasm_i32(32);
    CHECK(t, wasm_test_wasi_call(t, m, "args_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 32, buf, 9));
    CHECK(t, memcmp(buf, "prog\0a b\0", 9) == 0);

    args[0] = wasm_i32(0);
    args[1] = wasm_i32(4);
    CHECK(t, wasm_test_wasi_call(t, m, "environ_sizes_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 0, &value, 4));
    CHECK(t, value == 1u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 4, &value, 4));
    CHECK(t, value == 10u);

    args[0] = wasm_i32(3);
    args[1] = wasm_i32(0);
    CHECK(t, wasm_test_wasi_call(t, m, "fd_prestat_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 4, &value, 4));
    CHECK(t, value == 5u);
    args[1] = wasm_i32(64);
    args[2] = wasm_i32(5);
    CHECK(t, wasm_test_wasi_call(t, m, "fd_prestat_dir_name", args, 3) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 64, buf, 5));
    CHECK(t, memcmp(buf, "/work", 5) == 0);
    args[0] = wasm_i32(4);
    args[1] = wasm_i32(0);
    CHECK(t, wasm_test_wasi_call(t, m, "fd_prestat_get", args, 2) == 8u);

    /* One relative 1 ms monotonic clock subscription with userdata 42, and one on a closed fd. */
    memset(buf, 0, sizeof(buf));
    buf[0] = 42;
    buf[8] = 0;
    buf[16] = 1;
    buf[24 + 2] = 0x0F;
    buf[24 + 1] = 0x42;
    buf[24] = 0x40;
    WASM_CHECK_OK(t, wasm_memory_write(m, 0, 256, buf, 48));
    args[0] = wasm_i32(256);
    args[1] = wasm_i32(512);
    args[2] = wasm_i32(1);
    args[3] = wasm_i32(600);
    CHECK(t, wasm_test_wasi_call(t, m, "poll_oneoff", args, 4) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 600, &value, 4));
    CHECK(t, value == 1u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 512, buf, 32));
    CHECK(t, buf[0] == 42 && buf[8] == 0 && buf[9] == 0 && buf[10] == 0);

    memset(buf, 0, sizeof(buf));
    buf[0] = 7;
    buf[8] = 1;
    buf[16] = 40;
    WASM_CHECK_OK(t, wasm_memory_write(m, 0, 256 + 48, buf, 48));
    args[2] = wasm_i32(2);
    CHECK(t, wasm_test_wasi_call(t, m, "poll_oneoff", args, 4) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 600, &value, 4));
    CHECK_MSG(t, value == 1u, "expected only the fd event, got %u events", (unsigned)value);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 512, buf, 32));
    CHECK(t, buf[0] == 7 && buf[8] == 8 && buf[10] == 1);

    args[2] = wasm_i32(0);
    CHECK(t, wasm_test_wasi_call(t, m, "poll_oneoff", args, 4) == 28u);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_public_wasi_metadata_helpers) {
    static const uint8_t random_sentinel[16] = {
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
    };
    static const uint8_t stats_sentinel[24] = {
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
    };
    static const uint8_t size_sentinel[16] = {
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
    };

    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_value_t result;
    uint8_t size_bytes[16];
    uint8_t random_bytes[16];
    uint8_t clock_bytes[8];
    uint8_t fdstat_bytes[24];
    uint64_t clock_value;
    uint64_t rights_base;
    uint64_t rights_inheriting;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "random_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_time_get", 1);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);
        emit_export_func(&sec, "args_sizes", 7);
        emit_export_func(&sec, "args_get", 8);
        emit_export_func(&sec, "env_sizes", 9);
        emit_export_func(&sec, "env_get", 10);
        emit_export_func(&sec, "fdstat", 11);
        emit_export_func(&sec, "random", 12);
        emit_export_func(&sec, "clock", 13);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 7);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 32);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 36);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 96);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 112);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 40);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 44);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 104);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 120);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 3);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 192);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 4);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 128);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 16);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 5);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 160);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 6);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        uint8_t clock_sentinel[8];
        wasm_builder_t sec = { 0 };
        memset(clock_sentinel, 0xFF, sizeof(clock_sentinel));

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 32);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(size_sentinel));
        emit_bytes(&sec, size_sentinel, (uint32_t)sizeof(size_sentinel));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 128);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(random_sentinel));
        emit_bytes(&sec, random_sentinel, (uint32_t)sizeof(random_sentinel));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 160);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(clock_sentinel));
        emit_bytes(&sec, clock_sentinel, (uint32_t)sizeof(clock_sentinel));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 192);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(stats_sentinel));
        emit_bytes(&sec, stats_sentinel, (uint32_t)sizeof(stats_sentinel));

        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "args_sizes", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 32, size_bytes, 8);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes), 0);
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes + 4), 0);
    }

    err = wasm_call(m, "args_get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "env_sizes", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 40, size_bytes + 8, 8);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes + 8), 0);
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes + 12), 0);
    }

    err = wasm_call(m, "env_get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "fdstat", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 192, fdstat_bytes, sizeof(fdstat_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        rights_base = wasm__load_le64(fdstat_bytes + 8);
        rights_inheriting = wasm__load_le64(fdstat_bytes + 16);
        CHECK_MSG(t, fdstat_bytes[0] == 2u, "expected character device filetype, got %u", (unsigned)fdstat_bytes[0]);
        CHECK_MSG(t, wasm__load_le16(fdstat_bytes + 2) == 0u, "expected zero fdflags");
        CHECK_MSG(t, rights_base == UINT64_MAX, "expected all base rights, got %llu", (unsigned long long)rights_base);
        CHECK_MSG(t, rights_inheriting == UINT64_MAX,
                  "expected all inheriting rights, got %llu",
                  (unsigned long long)rights_inheriting);
    }

    err = wasm_call(m, "random", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 128, random_bytes, sizeof(random_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, memcmp(random_bytes, random_sentinel, sizeof(random_bytes)) != 0,
                  "%s", "expected random_get to overwrite sentinel bytes");
    }

    err = wasm_call(m, "clock", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 160, clock_bytes, sizeof(clock_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        clock_value = wasm__load_le64(clock_bytes);
        CHECK_MSG(t, clock_value != UINT64_MAX,
                  "%s", "expected clock_time_get to overwrite the sentinel timestamp");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}
#endif

#if WASM_ENABLE_PLATFORM
static const test_case platform_wasi_state_cases[] = {
    { "public api: WASI stubs expose args, env, fdstat, random, and clock helpers", test_public_wasi_metadata_helpers },
    { "wasi: per-runtime args, environment, preopens, and poll_oneoff", test_wasi_config_args_env_preopens_and_poll },
};
const test_suite platform_wasi_state_suite = { "platform.wasi_state", platform_wasi_state_cases, COUNTOF(platform_wasi_state_cases) };
#else
const test_suite platform_wasi_state_suite = { "platform.wasi_state", NULL, 0 };
#endif
