#include "../support.h"

#if WASM_ENABLE_PLATFORM
static void emit_import_memory(wasm_builder_t* b,
                               const char* module,
                               const char* name,
                               int is_64,
                               int has_max,
                               uint64_t initial_pages,
                               uint64_t max_pages) {
    uint32_t module_len = (uint32_t)strlen(module);
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(b, module_len);
    emit_bytes(b, (const uint8_t*)module, module_len);
    emit_leb128_u32(b, name_len);
    emit_bytes(b, (const uint8_t*)name, name_len);
    emit(b, 0x02);
    emit_memory_type(b, is_64, has_max, initial_pages, max_pages);
}

TEST(test_public_emscripten_stub_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_memory_t* memory;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 13);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
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
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
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
        emit(&sec, 0x7E);
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

        emit_leb128_u32(&sec, 36);
        emit_import_func(&sec, "env", "__syscall_faccessat", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_close", 1);
        emit_import_func(&sec, "env", "emscripten_date_now", 2);
        emit_import_func(&sec, "env", "__syscall_fchmod", 3);
        emit_import_func(&sec, "env", "__syscall_chmod", 3);
        emit_import_func(&sec, "env", "__syscall_fchown32", 4);
        emit_import_func(&sec, "env", "__syscall_fcntl64", 4);
        emit_import_func(&sec, "env", "__syscall_openat", 0);
        emit_import_func(&sec, "env", "__syscall_ioctl", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_write", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_read", 0);
        emit_import_func(&sec, "env", "__syscall_fstat64", 3);
        emit_import_func(&sec, "env", "__syscall_stat64", 3);
        emit_import_func(&sec, "env", "__syscall_newfstatat", 0);
        emit_import_func(&sec, "env", "__syscall_lstat64", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_sync", 1);
        emit_import_func(&sec, "env", "__syscall_ftruncate64", 6);
        emit_import_func(&sec, "env", "__syscall_getcwd", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_sizes_get", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_seek", 5);
        emit_import_func(&sec, "env", "__syscall_mkdirat", 4);
        emit_import_func(&sec, "env", "emscripten_get_now", 2);
        emit_import_func(&sec, "env", "_tzset_js", 7);
        emit_import_func(&sec, "env", "_localtime_js", 8);
        emit_import_func(&sec, "env", "_munmap_js", 10);
        emit_import_func(&sec, "env", "_mmap_js", 9);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_time_get", 11);
        emit_import_func(&sec, "env", "__syscall_readlinkat", 0);
        emit_import_func(&sec, "env", "__syscall_rmdir", 1);
        emit_import_func(&sec, "env", "emscripten_get_heap_max", 12);
        emit_import_func(&sec, "env", "__syscall_unlinkat", 4);
        emit_import_func(&sec, "env", "__syscall_utimensat", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_get", 3);
        emit_import_func(&sec, "env", "emscripten_resize_heap", 1);
        emit_import_memory(&sec, "env", "memory", 0, 1, 128u, 32768u);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_kind(&sec, "memory", 0x02, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);
    err = wasm_bind_emscripten_stubs(&rt);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    memory = wasm_module_memory(m, 0);
    CHECK_MSG(t, memory != NULL, "%s", "expected imported env.memory binding");
    if (memory) {
        CHECK_MSG(t, memory->pages == 128u, "expected 128 initial pages, got %llu",
                  (unsigned long long)memory->pages);
        CHECK_MSG(t, memory->max_pages == 32768u, "expected max_pages=32768, got %llu",
                  (unsigned long long)memory->max_pages);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}
#endif

#if WASM_ENABLE_PLATFORM
static const test_case platform_emscripten_cases[] = {
    { "public api: Emscripten stubs bind toolchain env imports and memory", test_public_emscripten_stub_helpers },
};
const test_suite platform_emscripten_suite = { "platform.emscripten", platform_emscripten_cases, COUNTOF(platform_emscripten_cases) };
#else
const test_suite platform_emscripten_suite = { "platform.emscripten", NULL, 0 };
#endif
