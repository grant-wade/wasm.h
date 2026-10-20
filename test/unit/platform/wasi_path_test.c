#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/platform/wasi_path.h"
#include "../../../src/platform/wasi_state.h"
#include "../../../src/portable.h"
#endif

#if WASM_ENABLE_PLATFORM
static uint32_t wasm_test_wasi_path_open(wasm_module_t* m, uint32_t dir_fd, const char* path,
                                         uint32_t oflags, uint32_t* out_fd) {
    wasm_value_t args[9];
    wasm_value_t result = wasm_i32(-1);
    uint8_t fd_bytes[4] = { 0 };
    size_t len = strlen(path);

    if (wasm_memory_write(m, 0, 256, path, len) != WASM_OK) return UINT32_MAX;
    args[0] = wasm_i32((int32_t)dir_fd);
    args[1] = wasm_i32(1);
    args[2] = wasm_i32(256);
    args[3] = wasm_i32((int32_t)len);
    args[4] = wasm_i32((int32_t)oflags);
    args[5] = wasm_i64(-1);
    args[6] = wasm_i64(-1);
    args[7] = wasm_i32(0);
    args[8] = wasm_i32(128);
    if (wasm__wasi_path_open(m, args, 9, &result, 1, NULL) != WASM_OK) return UINT32_MAX;
    if (out_fd && wasm_memory_read(m, 0, 128, fd_bytes, sizeof(fd_bytes)) == WASM_OK)
        *out_fd = wasm__load_le32(fd_bytes);
    return (uint32_t)result.of.i32;
}

#if WASM__PLATFORM_POSIX
static uint32_t wasm_test_wasi_symlink(wasm_module_t* m, uint32_t dir_fd, const char* target, const char* link) {
    wasm_value_t args[5];
    wasm_value_t result = wasm_i32(-1);
    size_t target_len = strlen(target);
    size_t link_len = strlen(link);

    if (wasm_memory_write(m, 0, 256, target, target_len) != WASM_OK) return UINT32_MAX;
    if (wasm_memory_write(m, 0, 512, link, link_len) != WASM_OK) return UINT32_MAX;
    args[0] = wasm_i32(256);
    args[1] = wasm_i32((int32_t)target_len);
    args[2] = wasm_i32((int32_t)dir_fd);
    args[3] = wasm_i32(512);
    args[4] = wasm_i32((int32_t)link_len);
    if (wasm__wasi_path_symlink(m, args, 5, &result, 1, NULL) != WASM_OK) return UINT32_MAX;
    return (uint32_t)result.of.i32;
}
#endif

TEST(test_wasi_paths_stay_beneath_preopens) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t errno_value;

    emit_header(&mod);
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_memory_type(&sec, 0, 0, 1u, 0u);
        emit_section(&mod, 5, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_kind(&sec, "memory", 0x02, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_wasi_stubs(&rt));
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm__wasi_state_ensure(&rt));

    errno_value = wasm_test_wasi_path_open(m, WASM__WASI_FD_STDIN, "/etc/hosts", 0u, NULL);
    WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_BADF);
    errno_value = wasm_test_wasi_path_open(m, WASM__WASI_FD_STDOUT, "hosts", 0u, NULL);
    WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_BADF);

#if WASM__PLATFORM_POSIX
    {
        char root[64];
        char path[128];
        int root_fd;
        uint32_t root_wasi_fd = 0u;
        uint32_t fd = 0u;
        FILE* f;

        snprintf(root, sizeof(root), "/tmp/wasm_test_wasi_%ld", (long)getpid());
        CHECK_MSG(t, mkdir(root, 0700) == 0, "%s", "mkdir root failed");
        snprintf(path, sizeof(path), "%s/sub", root);
        CHECK_MSG(t, mkdir(path, 0700) == 0, "%s", "mkdir failed");
        snprintf(path, sizeof(path), "%s/file.txt", root);
        f = fopen(path, "w");
        CHECK_MSG(t, f != NULL, "%s", "fopen failed");
        if (f) fclose(f);

        root_fd = open(root, O_RDONLY);
        CHECK_MSG(t, root_fd >= 0, "%s", "open root failed");
        WASM_CHECK_OK(t, wasm__wasi_state_add_fd(&rt, root_fd, 1, WASM__WASI_FILETYPE_DIRECTORY,
                                                 WASM__WASI_RIGHTS_ALL, WASM__WASI_RIGHTS_ALL,
                                                 NULL, "/sandbox", &root_wasi_fd));

        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "file.txt", 0u, &fd);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "sub/./../file.txt", 0u, &fd);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "/etc/hosts", 0u, NULL);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "../file.txt", 0u, NULL);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "sub/../../file.txt", 0u, NULL);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);

        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "sub", 0x2u, &fd);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);
        if (errno_value == WASM__WASI_ERRNO_SUCCESS) {
            errno_value = wasm_test_wasi_path_open(m, fd, "../file.txt", 0u, NULL);
            WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        }

        errno_value = wasm_test_wasi_symlink(m, root_wasi_fd, "/etc/hosts", "abs_link");
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_symlink(m, root_wasi_fd, "../../file.txt", "sub/up_link");
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_symlink(m, root_wasi_fd, "../file.txt", "sub/ok_link");
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);

        snprintf(path, sizeof(path), "%s/sub/ok_link", root);
        (void)unlink(path);
        snprintf(path, sizeof(path), "%s/sub", root);
        (void)rmdir(path);
        snprintf(path, sizeof(path), "%s/file.txt", root);
        (void)unlink(path);
        (void)rmdir(root);
    }
#endif

    wasm_module_free(m);
    wasm_destroy(&rt);
}
#endif

#if WASM_ENABLE_PLATFORM
static const test_case platform_wasi_path_cases[] = {
    { "WASI: path operations need a preopen and stay beneath it", test_wasi_paths_stay_beneath_preopens },
};
const test_suite platform_wasi_path_suite = { "platform.wasi_path", platform_wasi_path_cases, COUNTOF(platform_wasi_path_cases) };
#else
const test_suite platform_wasi_path_suite = { "platform.wasi_path", NULL, 0 };
#endif
