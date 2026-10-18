#include "wasi_fd.h"
#include "wasi_io.h"
#include "wasi_path.h"
#include "wasi_state.h"
#include "../runtime.h"

#if WASM_ENABLE_PLATFORM
wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt) {
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;
    err = wasm__wasi_state_ensure(rt);
    if (err != WASM_OK) return err;

#define WASM__BIND_WASI(name, sig, callback, data)                                        \
    do {                                                                                  \
        err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, name, sig, callback, data); \
        if (err != WASM_OK) return err;                                                   \
    } while (0)

    WASM__BIND_WASI("args_get", "ii(i)", wasm__wasi_args_get, NULL);
    WASM__BIND_WASI("args_sizes_get", "ii(i)", wasm__wasi_args_sizes_get, NULL);
    WASM__BIND_WASI("environ_get", "ii(i)", wasm__wasi_environ_get, NULL);
    WASM__BIND_WASI("environ_sizes_get", "ii(i)", wasm__wasi_environ_sizes_get, NULL);
    WASM__BIND_WASI("clock_res_get", "ii(i)", wasm__wasi_clock_res_get, NULL);
    WASM__BIND_WASI("clock_time_get", "iIi(i)", wasm__wasi_clock_time_get, NULL);
    WASM__BIND_WASI("fd_advise", "iIIi(i)", wasm__wasi_fd_advise, NULL);
    WASM__BIND_WASI("fd_allocate", "iII(i)", wasm__wasi_fd_allocate, NULL);
    WASM__BIND_WASI("fd_close", "i(i)", wasm__wasi_fd_close, NULL);
    WASM__BIND_WASI("fd_datasync", "i(i)", wasm__wasi_fd_datasync, NULL);
    WASM__BIND_WASI("fd_fdstat_get", "ii(i)", wasm__wasi_fd_fdstat_get, NULL);
    WASM__BIND_WASI("fd_fdstat_set_flags", "ii(i)", wasm__wasi_fd_fdstat_set_flags, NULL);
    WASM__BIND_WASI("fd_fdstat_set_rights", "iII(i)", wasm__wasi_fd_fdstat_set_rights, NULL);
    WASM__BIND_WASI("fd_filestat_get", "ii(i)", wasm__wasi_fd_filestat_get, NULL);
    WASM__BIND_WASI("fd_filestat_set_size", "iI(i)", wasm__wasi_fd_filestat_set_size, NULL);
    WASM__BIND_WASI("fd_filestat_set_times", "iIIi(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);
    WASM__BIND_WASI("fd_pread", "iiiIi(i)", wasm__wasi_fd_pread, NULL);
    WASM__BIND_WASI("fd_prestat_get", "ii(i)", wasm__wasi_fd_prestat_get, NULL);
    WASM__BIND_WASI("fd_prestat_dir_name", "iii(i)", wasm__wasi_fd_prestat_dir_name, NULL);
    WASM__BIND_WASI("fd_pwrite", "iiiIi(i)", wasm__wasi_fd_pwrite, NULL);
    WASM__BIND_WASI("fd_read", "iiii(i)", wasm__wasi_fd_read, NULL);
    WASM__BIND_WASI("fd_readdir", "iiiIi(i)", wasm__wasi_fd_readdir, NULL);
    WASM__BIND_WASI("fd_renumber", "ii(i)", wasm__wasi_fd_renumber, NULL);
    WASM__BIND_WASI("fd_seek", "iIii(i)", wasm__wasi_fd_seek, NULL);
    WASM__BIND_WASI("fd_sync", "i(i)", wasm__wasi_fd_sync, NULL);
    WASM__BIND_WASI("fd_tell", "ii(i)", wasm__wasi_fd_tell, NULL);
    WASM__BIND_WASI("fd_write", "iiii(i)", wasm__wasi_fd_write, NULL);
    WASM__BIND_WASI("path_create_directory", "iii(i)", wasm__wasi_path_create_directory, NULL);
    WASM__BIND_WASI("path_filestat_get", "iiiii(i)", wasm__wasi_path_filestat_get, NULL);
    WASM__BIND_WASI("path_filestat_set_times", "iiiiIIi(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);
    WASM__BIND_WASI("path_link", "iiiiiii(i)", wasm__wasi_path_link, NULL);
    WASM__BIND_WASI("path_open", "iiiiiIIii(i)", wasm__wasi_path_open, NULL);
    WASM__BIND_WASI("path_readlink", "iiiiii(i)", wasm__wasi_path_readlink, NULL);
    WASM__BIND_WASI("path_remove_directory", "iii(i)", wasm__wasi_path_remove_directory, NULL);
    WASM__BIND_WASI("path_rename", "iiiiii(i)", wasm__wasi_path_rename, NULL);
    WASM__BIND_WASI("path_symlink", "iiiii(i)", wasm__wasi_path_symlink, NULL);
    WASM__BIND_WASI("path_unlink_file", "iii(i)", wasm__wasi_path_unlink_file, NULL);
    WASM__BIND_WASI("poll_oneoff", "iiii(i)", wasm__wasi_poll_oneoff, NULL);
    WASM__BIND_WASI("proc_exit", "i(v)", wasm__wasi_proc_exit, NULL);
    /* Raising a host signal would let a guest stop or kill the embedding process. */
    WASM__BIND_WASI("proc_raise", "i(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);
    WASM__BIND_WASI("sched_yield", "(i)", wasm__wasi_sched_yield, NULL);
    WASM__BIND_WASI("random_get", "ii(i)", wasm__wasi_random_get, NULL);
    WASM__BIND_WASI("sock_accept", "iii(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);
    WASM__BIND_WASI("sock_recv", "iiiiii(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);
    WASM__BIND_WASI("sock_send", "iiiii(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);
    WASM__BIND_WASI("sock_shutdown", "ii(i)", wasm__wasi_errno_stub,
                    (void*)(uintptr_t)WASM__WASI_ERRNO_NOSYS);

#undef WASM__BIND_WASI

    return WASM_OK;
}

static wasm_error_t wasm__wasi_set_strings(wasm_runtime_t* rt, int is_env, uint32_t count,
                                           const char* const* values) {
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;
    err = wasm__wasi_state_ensure(rt);
    if (err == WASM_OK)
        err = wasm__wasi_strings_set(&rt->allocator, is_env ? &rt->wasi_state->env : &rt->wasi_state->args, count, values);
    if (err != WASM_OK)
        WASM__SET_ERR(rt, err, "cannot set WASI %s: %s", is_env ? "environment" : "arguments",
                      err == WASM_ERR_OOM ? "out of memory" : "a string is NULL");
    return err;
}

wasm_error_t wasm_wasi_set_args(wasm_runtime_t* rt, uint32_t argc, const char* const* argv) {
    return wasm__wasi_set_strings(rt, 0, argc, argv);
}

wasm_error_t wasm_wasi_set_env(wasm_runtime_t* rt, uint32_t count, const char* const* env) {
    return wasm__wasi_set_strings(rt, 1, count, env);
}

wasm_error_t wasm_wasi_preopen_dir(wasm_runtime_t* rt, const char* host_path, const char* guest_path) {
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;
    err = wasm__wasi_state_ensure(rt);
    if (err == WASM_OK) err = wasm__wasi_state_preopen_dir(rt, host_path, guest_path);
    if (err == WASM_ERR_OOM)
        WASM__SET_ERR(rt, err, "cannot preopen '%s': out of memory or descriptors",
                      host_path ? host_path : "");
    else if (err != WASM_OK)
        WASM__SET_ERR(rt, err, "cannot preopen '%s' as '%s': not an accessible directory",
                      host_path ? host_path : "", guest_path ? guest_path : "");
    return err;
}

#endif
