#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/platform/wasi_state.h"
#endif

#if WASM_ENABLE_PLATFORM
TEST(test_wasi_proc_raise_does_not_signal_the_host) {
    /* raise(sig) forwards to wasi_snapshot_preview1.proc_raise. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x25,
        0x01, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72,
        0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x0a, 0x70, 0x72, 0x6f, 0x63, 0x5f, 0x72, 0x61, 0x69, 0x73, 0x65, 0x00,
        0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x09, 0x01, 0x05, 0x72, 0x61, 0x69, 0x73, 0x65, 0x00, 0x01, 0x0a, 0x08,
        0x01, 0x06, 0x00, 0x20, 0x00, 0x10, 0x00, 0x0b,
    };
    static const int32_t signals[] = { 2, 6, 9, 11, 15 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_wasi_stubs(&rt));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        wasm_value_t arg = wasm_i32(signals[i]);
        wasm_value_t result = wasm_i32(0);

        WASM_CHECK_OK(t, wasm_call(m, "raise", &arg, 1, &result, 1));
        CHECK_MSG(t, result.of.i32 == 52, "expected ENOSYS for signal %d, got %d", (int)signals[i],
                  (int)result.of.i32);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_public_wasi_preview1_surface_imports) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 17);

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
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 9);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 6);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 46);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_res_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_time_get", 1);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_advise", 2);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_allocate", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_close", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_datasync", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_set_flags", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_set_rights", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_filestat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_filestat_set_size", 15);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_filestat_set_times", 2);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_pread", 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_prestat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_prestat_dir_name", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_pwrite", 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_read", 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_readdir", 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_renumber", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_seek", 16);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_sync", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_tell", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_write", 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_create_directory", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_filestat_get", 8);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_filestat_set_times", 9);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_link", 10);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_open", 11);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_readlink", 12);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_remove_directory", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_rename", 12);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_symlink", 8);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_unlink_file", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "poll_oneoff", 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "proc_exit", 14);
        emit_import_func(&sec, "wasi_snapshot_preview1", "proc_raise", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sched_yield", 13);
        emit_import_func(&sec, "wasi_snapshot_preview1", "random_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_accept", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_recv", 12);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_send", 8);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_shutdown", 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);
    CHECK_MSG(t, wasm__wasi_fd_entry(&rt, 3u) == NULL,
              "%s", "WASI bindings must not implicitly preopen the host working directory");

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);

    wasm_module_free(m);
    wasm_destroy(&rt);
}
#endif

#if WASM_ENABLE_PLATFORM
static const test_case platform_wasi_bind_cases[] = {
    { "public api: WASI stubs bind the full preview1 surface", test_public_wasi_preview1_surface_imports },
    { "wasi: proc_raise reports ENOSYS instead of signaling the host", test_wasi_proc_raise_does_not_signal_the_host },
};
const test_suite platform_wasi_bind_suite = { "platform.wasi_bind", platform_wasi_bind_cases, COUNTOF(platform_wasi_bind_cases) };
#else
const test_suite platform_wasi_bind_suite = { "platform.wasi_bind", NULL, 0 };
#endif
