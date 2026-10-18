#include "wasi_path.h"
#include "host.h"
#include "../module.h"
#include "../runtime.h"
#include "../internal/alloc.h"
#include "../portable.h"
#include "wasi_state.h"
#include "wasi_io.h"
#include "../memory_internal.h"
#include <errno.h>
#include <string.h>

#if WASM_ENABLE_PLATFORM
#if WASM__PLATFORM_POSIX
static int wasm__wasi_dirflags_to_host(uint32_t flags) {
    int host_flags = 0;

#ifdef AT_SYMLINK_NOFOLLOW
    if ((flags & 0x1u) == 0u) host_flags |= AT_SYMLINK_NOFOLLOW;
#else
    (void)flags;
#endif
    return host_flags;
}

#endif

static int wasm__wasi_oflags_to_host(uint32_t oflags, uint32_t fdflags, uint64_t rights) {
    int host_flags = 0;
    int can_read = (rights & 0x2u) != 0u;
    int can_write = (rights & 0x40u) != 0u || (fdflags & 0x1u) != 0u;

#ifdef O_BINARY
    host_flags |= O_BINARY;
#endif
    if ((oflags & 0x2u) != 0u || !can_write)
        host_flags |= O_RDONLY;
    else if (can_read)
        host_flags |= O_RDWR;
    else
        host_flags |= O_WRONLY;
    if ((fdflags & 0x1u) != 0u) host_flags |= O_APPEND;
#ifdef O_NONBLOCK
    if ((fdflags & 0x4u) != 0u) host_flags |= O_NONBLOCK;
#endif
#ifdef O_DSYNC
    if ((fdflags & 0x2u) != 0u) host_flags |= O_DSYNC;
#endif
#ifdef O_RSYNC
    if ((fdflags & 0x8u) != 0u) host_flags |= O_RSYNC;
#endif
#ifdef O_SYNC
    if ((fdflags & 0x10u) != 0u) host_flags |= O_SYNC;
#endif
    if ((oflags & 0x1u) != 0u) host_flags |= O_CREAT;
#ifdef O_DIRECTORY
    if ((oflags & 0x2u) != 0u) host_flags |= O_DIRECTORY;
#endif
    if ((oflags & 0x4u) != 0u) host_flags |= O_EXCL;
    if ((oflags & 0x8u) != 0u) host_flags |= O_TRUNC;
    return host_flags;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_prestat_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint32_t prestat_ptr;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_prestat_get", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    prestat_ptr = (uint32_t)args[1].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)prestat_ptr, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !entry->is_preopen || !entry->preopen_path) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    memset(memory->data + prestat_ptr, 0, 8u);
    memory->data[prestat_ptr] = WASM__WASI_PREOPENTYPE_DIR;
    wasm__store_le32(memory->data + prestat_ptr + 4u, (uint32_t)strlen(entry->preopen_path));
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_prestat_dir_name(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint32_t path_ptr;
    uint32_t path_len;
    size_t name_len;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_prestat_dir_name", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0 || args[2].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !entry->is_preopen || !entry->preopen_path) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    path_ptr = (uint32_t)args[1].of.i32;
    path_len = (uint32_t)args[2].of.i32;
    name_len = strlen(entry->preopen_path);
    if (path_len < name_len || !wasm__range_in_bounds_u64((uint64_t)path_ptr, path_len, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    memcpy(memory->data + path_ptr, entry->preopen_path, name_len);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_create_directory(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_create_directory", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0 || args[2].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if ((path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[1].of.i32, (uint32_t)args[2].of.i32, &path)) != WASM__WASI_ERRNO_SUCCESS) {
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (mkdirat(entry->host_fd, path, 0777) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#elif defined(_WIN32)
    {
        char* full_path = NULL;
        wchar_t* wide_path = NULL;

        if (!wasm__wasi_windows_resolve_path(entry, path, &full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_utf8_to_wide(full_path, &wide_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
        else if (_wmkdir(wide_path) != 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        WASM_FREE(full_path);
        WASM_FREE(wide_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_filestat_get(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;
    uint32_t out_ptr;

    (void)userdata;

    if (!mod || !args || !results || num_args != 5 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_filestat_get", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0 || args[4].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    out_ptr = (uint32_t)args[4].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)out_ptr, 64u, mem_size) ||
        (path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[2].of.i32, (uint32_t)args[3].of.i32, &path)) != WASM__WASI_ERRNO_SUCCESS) {
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    {
        struct stat st;

        if (fstatat(entry->host_fd, path, &st, wasm__wasi_dirflags_to_host((uint32_t)args[1].of.i32)) != 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else {
            wasm__wasi_store_filestat(memory->data + out_ptr, &st);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        }
    }
#elif defined(_WIN32)
    {
        char* full_path = NULL;
        wasm__wasi_host_stat_t st;

        if (!wasm__wasi_windows_resolve_path(entry, path, &full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_stat_path(full_path, &st))
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else {
            wasm__wasi_store_filestat(memory->data + out_ptr, &st);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        }
        WASM_FREE(full_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_link(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* old_entry;
    wasm__wasi_fd_entry_t* new_entry;
    char* old_path = NULL;
    char* new_path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 7 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_link", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    old_entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    new_entry = wasm__wasi_fd_entry(rt, (uint32_t)args[4].of.i32);
    if (!old_entry || !new_entry || !wasm__wasi_entry_can_path(old_entry) || !wasm__wasi_entry_can_path(new_entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if ((path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[2].of.i32, (uint32_t)args[3].of.i32, &old_path)) != WASM__WASI_ERRNO_SUCCESS ||
        (path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[5].of.i32, (uint32_t)args[6].of.i32, &new_path)) != WASM__WASI_ERRNO_SUCCESS) {
        WASM_FREE(old_path);
        WASM_FREE(new_path);
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (linkat(old_entry->host_fd, old_path, new_entry->host_fd, new_path,
               wasm__wasi_dirflags_to_host((uint32_t)args[1].of.i32)) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#elif defined(_WIN32)
    {
        char* old_full_path = NULL;
        char* new_full_path = NULL;
        wchar_t* old_wide_path = NULL;
        wchar_t* new_wide_path = NULL;

        if (!wasm__wasi_windows_resolve_path(old_entry, old_path, &old_full_path) ||
            !wasm__wasi_windows_resolve_path(new_entry, new_path, &new_full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_utf8_to_wide(old_full_path, &old_wide_path) ||
                 !wasm__wasi_windows_utf8_to_wide(new_full_path, &new_wide_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
        else if (!CreateHardLinkW(new_wide_path, old_wide_path, NULL))
            wasm__wasi_set_errno(results, num_results, wasm__wasi_windows_error_to_wasi(GetLastError()));
        else
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        WASM_FREE(old_full_path);
        WASM_FREE(new_full_path);
        WASM_FREE(old_wide_path);
        WASM_FREE(new_wide_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(old_path);
    WASM_FREE(new_path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_open(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;
    uint32_t opened_fd_ptr;

    (void)userdata;

    if (!mod || !args || !results || num_args != 9 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_open", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    opened_fd_ptr = (uint32_t)args[8].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)opened_fd_ptr, 4u, mem_size) ||
        (path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[2].of.i32, (uint32_t)args[3].of.i32, &path)) != WASM__WASI_ERRNO_SUCCESS) {
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    {
        int host_fd = openat(entry->host_fd, path,
                             wasm__wasi_oflags_to_host((uint32_t)args[4].of.i32, (uint32_t)args[7].of.i32,
                                                       (uint64_t)args[5].of.i64),
                             0666);

        if (host_fd < 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else {
            struct stat st;
            uint32_t new_fd = 0u;

            if (fstat(host_fd, &st) != 0) {
                int saved_errno = errno;

                (void)wasm__wasi_host_close(host_fd);
                wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(saved_errno));
            } else if (wasm__wasi_state_add_fd(rt, host_fd, 1, wasm__wasi_filetype_from_mode(st.st_mode),
                                               (uint64_t)args[5].of.i64, (uint64_t)args[6].of.i64,
                                               NULL, NULL, &new_fd) != WASM_OK) {
                (void)wasm__wasi_host_close(host_fd);
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
            } else {
                wasm__store_le32(memory->data + opened_fd_ptr, new_fd);
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
            }
        }
    }
#elif defined(_WIN32)
    {
        char* full_path = NULL;
        uint32_t oflags = (uint32_t)args[4].of.i32;
        int host_flags = wasm__wasi_oflags_to_host(oflags, (uint32_t)args[7].of.i32, (uint64_t)args[5].of.i64);
        uint32_t new_fd = 0u;
        wasm__wasi_host_stat_t st;
        uint8_t filetype = WASM__WASI_FILETYPE_UNKNOWN;

        if (!wasm__wasi_windows_resolve_path(entry, path, &full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if ((oflags & 0x2u) != 0u) {
            if (!wasm__wasi_windows_stat_path(full_path, &st))
                wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            else if (wasm__wasi_filetype_from_mode(st.st_mode) != WASM__WASI_FILETYPE_DIRECTORY)
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOTDIR);
            else if (wasm__wasi_state_add_fd(rt, -1, 0, WASM__WASI_FILETYPE_DIRECTORY,
                                             (uint64_t)args[5].of.i64, (uint64_t)args[6].of.i64,
                                             full_path, NULL, &new_fd) != WASM_OK)
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
            else {
                wasm__store_le32(memory->data + opened_fd_ptr, new_fd);
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
            }
        } else {
            int host_fd = wasm__wasi_windows_open_path(full_path, host_flags);

            if (host_fd < 0)
                wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            else if (!wasm__wasi_host_fstat(host_fd, &st)) {
                int saved_errno = errno;

                (void)wasm__wasi_host_close(host_fd);
                wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(saved_errno));
            } else {
                filetype = wasm__wasi_filetype_from_mode(st.st_mode);
                if (wasm__wasi_state_add_fd(rt, host_fd, 1, filetype,
                                            (uint64_t)args[5].of.i64, (uint64_t)args[6].of.i64,
                                            full_path, NULL, &new_fd) != WASM_OK) {
                    (void)wasm__wasi_host_close(host_fd);
                    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
                } else {
                    wasm__store_le32(memory->data + opened_fd_ptr, new_fd);
                    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
                }
            }
        }
        WASM_FREE(full_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_readlink(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 6 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_readlink", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[3].of.i32, (uint64_t)(uint32_t)args[4].of.i32, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[5].of.i32, 4u, mem_size) ||
        (path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[1].of.i32, (uint32_t)args[2].of.i32, &path)) != WASM__WASI_ERRNO_SUCCESS) {
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    {
        ssize_t rc = readlinkat(entry->host_fd, path, (char*)memory->data + (uint32_t)args[3].of.i32,
                                (size_t)(uint32_t)args[4].of.i32);
        if (rc < 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else {
            wasm__store_le32(memory->data + (uint32_t)args[5].of.i32, (uint32_t)rc);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        }
    }
#elif defined(_WIN32)
    {
        char* full_path = NULL;
        char* target_path = NULL;
        size_t target_len;

        if (!wasm__wasi_windows_resolve_path(entry, path, &full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_readlink_path(full_path, &target_path)) {
            DWORD last_error = GetLastError();

            if (last_error != ERROR_SUCCESS)
                wasm__wasi_set_errno(results, num_results, wasm__wasi_windows_error_to_wasi(last_error));
            else
                wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        } else {
            target_len = strlen(target_path);
            if (target_len > (size_t)(uint32_t)args[4].of.i32) target_len = (size_t)(uint32_t)args[4].of.i32;
            memcpy(memory->data + (uint32_t)args[3].of.i32, target_path, target_len);
            wasm__store_le32(memory->data + (uint32_t)args[5].of.i32, (uint32_t)target_len);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        }
        WASM_FREE(full_path);
        WASM_FREE(target_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_remove_directory(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_remove_directory", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if ((path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[1].of.i32, (uint32_t)args[2].of.i32, &path)) != WASM__WASI_ERRNO_SUCCESS) {
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
#ifdef AT_REMOVEDIR
    if (unlinkat(entry->host_fd, path, AT_REMOVEDIR) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#else
    if (rmdir(path) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#endif
#elif defined(_WIN32)
    {
        char* full_path = NULL;
        wchar_t* wide_path = NULL;

        if (!wasm__wasi_windows_resolve_path(entry, path, &full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_utf8_to_wide(full_path, &wide_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
        else if (_wrmdir(wide_path) != 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        WASM_FREE(full_path);
        WASM_FREE(wide_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_rename(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* old_entry;
    wasm__wasi_fd_entry_t* new_entry;
    char* old_path = NULL;
    char* new_path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 6 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_rename", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    old_entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    new_entry = wasm__wasi_fd_entry(rt, (uint32_t)args[3].of.i32);
    if (!old_entry || !new_entry || !wasm__wasi_entry_can_path(old_entry) || !wasm__wasi_entry_can_path(new_entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if ((path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[1].of.i32, (uint32_t)args[2].of.i32, &old_path)) != WASM__WASI_ERRNO_SUCCESS ||
        (path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[4].of.i32, (uint32_t)args[5].of.i32, &new_path)) != WASM__WASI_ERRNO_SUCCESS) {
        WASM_FREE(old_path);
        WASM_FREE(new_path);
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (renameat(old_entry->host_fd, old_path, new_entry->host_fd, new_path) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#elif defined(_WIN32)
    {
        char* old_full_path = NULL;
        char* new_full_path = NULL;
        wchar_t* old_wide_path = NULL;
        wchar_t* new_wide_path = NULL;

        if (!wasm__wasi_windows_resolve_path(old_entry, old_path, &old_full_path) ||
            !wasm__wasi_windows_resolve_path(new_entry, new_path, &new_full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_utf8_to_wide(old_full_path, &old_wide_path) ||
                 !wasm__wasi_windows_utf8_to_wide(new_full_path, &new_wide_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
        else if (_wrename(old_wide_path, new_wide_path) != 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        WASM_FREE(old_full_path);
        WASM_FREE(new_full_path);
        WASM_FREE(old_wide_path);
        WASM_FREE(new_wide_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(old_path);
    WASM_FREE(new_path);
    return WASM_OK;
}

static int wasm__wasi_symlink_target_contained(const char* target, const char* link_path) {
    uint32_t depth = 0u;
    const char* last = link_path;
    const char* p;
    char* parent;
    size_t parent_len;
    int ok;

    for (p = link_path; *p; p++) {
        if (*p == '/' || *p == '\\') last = p;
    }
    parent_len = last == link_path ? 0u : (size_t)(last - link_path);
    parent = (char*)WASM_MALLOC(parent_len + 1u);
    if (!parent) return 0;
    memcpy(parent, link_path, parent_len);
    parent[parent_len] = 0;
    ok = wasm__wasi_path_walk(parent, 0u, &depth) && wasm__wasi_path_walk(target, depth, NULL);
    WASM_FREE(parent);
    return ok;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_symlink(wasm_module_t* mod,
                                            const wasm_value_t* args, uint32_t num_args,
                                            wasm_value_t* results, uint32_t num_results,
                                            void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* old_path = NULL;
    char* new_path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 5 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_symlink", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[2].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__wasi_read_guest_string(memory, mem_size, (uint32_t)args[0].of.i32, (uint32_t)args[1].of.i32, &old_path) ||
        (path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[3].of.i32, (uint32_t)args[4].of.i32, &new_path)) !=
            WASM__WASI_ERRNO_SUCCESS) {
        WASM_FREE(old_path);
        WASM_FREE(new_path);
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
    if (!wasm__wasi_symlink_target_contained(old_path, new_path)) {
        WASM_FREE(old_path);
        WASM_FREE(new_path);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOTCAPABLE);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (symlinkat(old_path, entry->host_fd, new_path) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#elif defined(_WIN32)
    {
        char* new_full_path = NULL;
        char* old_target_probe = NULL;
        wchar_t* old_wide_path = NULL;
        wchar_t* new_wide_path = NULL;
        wasm__wasi_host_stat_t st;
        DWORD flags = 0u;

#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
        flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
        if (!wasm__wasi_windows_resolve_path(entry, new_path, &new_full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else {
            if (wasm__wasi_windows_path_is_absolute(old_path))
                old_target_probe = wasm__wasi_strdup(old_path);
            else
                (void)wasm__wasi_windows_resolve_path(entry, old_path, &old_target_probe);
            if (old_target_probe && wasm__wasi_windows_stat_path(old_target_probe, &st) &&
                wasm__wasi_filetype_from_mode(st.st_mode) == WASM__WASI_FILETYPE_DIRECTORY)
                flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
            if (!wasm__wasi_windows_utf8_to_wide(old_path, &old_wide_path) ||
                !wasm__wasi_windows_utf8_to_wide(new_full_path, &new_wide_path))
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
            else if (!CreateSymbolicLinkW(new_wide_path, old_wide_path, flags))
                wasm__wasi_set_errno(results, num_results, wasm__wasi_windows_error_to_wasi(GetLastError()));
            else
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        }
        WASM_FREE(new_full_path);
        WASM_FREE(old_target_probe);
        WASM_FREE(old_wide_path);
        WASM_FREE(new_wide_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(old_path);
    WASM_FREE(new_path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_path_unlink_file(wasm_module_t* mod,
                                                const wasm_value_t* args, uint32_t num_args,
                                                wasm_value_t* results, uint32_t num_results,
                                                void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    char* path = NULL;
    uint32_t path_errno = WASM__WASI_ERRNO_FAULT;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "path_unlink_file", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if ((path_errno = wasm__wasi_read_guest_path(memory, mem_size, (uint32_t)args[1].of.i32, (uint32_t)args[2].of.i32, &path)) != WASM__WASI_ERRNO_SUCCESS) {
        wasm__wasi_set_errno(results, num_results, path_errno);
        return WASM_OK;
    }
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (unlinkat(entry->host_fd, path, 0) != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#elif defined(_WIN32)
    {
        char* full_path = NULL;
        wchar_t* wide_path = NULL;

        if (!wasm__wasi_windows_resolve_path(entry, path, &full_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        else if (!wasm__wasi_windows_utf8_to_wide(full_path, &wide_path))
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
        else if (_wunlink(wide_path) != 0)
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
        else
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        WASM_FREE(full_path);
        WASM_FREE(wide_path);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    WASM_FREE(path);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_renumber(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* from_entry;
    wasm__wasi_fd_entry_t* to_entry;
    uint32_t from_fd;
    uint32_t to_fd;

    (void)userdata;

    if (!mod || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    from_fd = (uint32_t)args[0].of.i32;
    to_fd = (uint32_t)args[1].of.i32;
    if (from_fd >= WASM__WASI_MAX_FDS || to_fd >= WASM__WASI_MAX_FDS) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    from_entry = wasm__wasi_fd_entry(rt, from_fd);
    if (!from_entry) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (from_fd == to_fd) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }
    to_entry = &rt->wasi_state->fds[to_fd];
    if (to_entry->used) wasm__wasi_fd_entry_reset(to_entry);
    *to_entry = *from_entry;
    memset(from_entry, 0, sizeof(*from_entry));
    from_entry->host_fd = -1;
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_pread(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint64_t total_read = 0u;
    uint32_t i;

    (void)userdata;

    if (!mod || !args || !results || num_args != 5 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_pread", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[4].of.i32, 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[1].of.i32, (uint64_t)(uint32_t)args[2].of.i32 * 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    for (i = 0; i < (uint32_t)args[2].of.i32; i++) {
        uint8_t* iov = memory->data + (uint32_t)args[1].of.i32 + i * 8u;
        uint32_t buf_ptr = wasm__load_le32(iov);
        uint32_t buf_len = wasm__load_le32(iov + 4u);
        size_t read_count = 0u;

        if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
            return WASM_OK;
        }
        if (!wasm__wasi_host_pread(entry->host_fd, memory->data + buf_ptr, (size_t)buf_len,
                                   (uint64_t)args[3].of.i64 + total_read, &read_count)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        total_read += (uint64_t)read_count;
        if (read_count != (size_t)buf_len) break;
    }
    wasm__store_le32(memory->data + (uint32_t)args[4].of.i32, (uint32_t)total_read);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_pwrite(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint64_t total_written = 0u;
    uint32_t i;

    (void)userdata;

    if (!mod || !args || !results || num_args != 5 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_pwrite", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[4].of.i32, 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[1].of.i32, (uint64_t)(uint32_t)args[2].of.i32 * 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    for (i = 0; i < (uint32_t)args[2].of.i32; i++) {
        const uint8_t* iov = memory->data + (uint32_t)args[1].of.i32 + i * 8u;
        uint32_t buf_ptr = wasm__load_le32(iov);
        uint32_t buf_len = wasm__load_le32(iov + 4u);
        size_t write_count = 0u;

        if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
            return WASM_OK;
        }
        if (!wasm__wasi_host_pwrite(entry->host_fd, memory->data + buf_ptr, (size_t)buf_len,
                                    (uint64_t)args[3].of.i64 + total_written, &write_count)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        total_written += (uint64_t)write_count;
        if (write_count != (size_t)buf_len) break;
    }
    wasm__store_le32(memory->data + (uint32_t)args[4].of.i32, (uint32_t)total_written);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_readdir(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint32_t buf_ptr;
    uint32_t buf_len;
    uint32_t out_ptr;

    (void)userdata;

    if (!mod || !args || !results || num_args != 5 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_readdir", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    buf_ptr = (uint32_t)args[1].of.i32;
    buf_len = (uint32_t)args[2].of.i32;
    out_ptr = (uint32_t)args[4].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)out_ptr, 4u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    {
        int dup_fd = dup(entry->host_fd);
        DIR* dir;
        struct dirent* host_ent;
        uint64_t cookie = (uint64_t)args[3].of.i64;
        uint64_t index = 0u;
        uint32_t used = 0u;

        if (dup_fd < 0) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        dir = fdopendir(dup_fd);
        if (!dir) {
            (void)wasm__wasi_host_close(dup_fd);
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        while ((host_ent = readdir(dir)) != NULL) {
            size_t name_len = strlen(host_ent->d_name);
            uint32_t record_size = 24u + (uint32_t)name_len;

            if (index++ < cookie) continue;
            if (used + record_size > buf_len) break;
            memset(memory->data + buf_ptr + used, 0, 24u);
            wasm__store_le64(memory->data + buf_ptr + used + 0u, index);
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            /* The BSDs hide d_ino under strict POSIX feature macros; d_fileno is always declared. */
            wasm__store_le64(memory->data + buf_ptr + used + 8u, (uint64_t)host_ent->d_fileno);
#else
            wasm__store_le64(memory->data + buf_ptr + used + 8u, (uint64_t)host_ent->d_ino);
#endif
            wasm__store_le32(memory->data + buf_ptr + used + 16u, (uint32_t)name_len);
#ifdef DT_DIR
            switch (host_ent->d_type) {
                case DT_BLK:
                    memory->data[buf_ptr + used + 20u] = WASM__WASI_FILETYPE_BLOCK_DEVICE;
                    break;
                case DT_CHR:
                    memory->data[buf_ptr + used + 20u] = WASM__WASI_FILETYPE_CHARACTER_DEVICE;
                    break;
                case DT_DIR:
                    memory->data[buf_ptr + used + 20u] = WASM__WASI_FILETYPE_DIRECTORY;
                    break;
                case DT_LNK:
                    memory->data[buf_ptr + used + 20u] = WASM__WASI_FILETYPE_SYMBOLIC_LINK;
                    break;
                case DT_REG:
                    memory->data[buf_ptr + used + 20u] = WASM__WASI_FILETYPE_REGULAR_FILE;
                    break;
                default:
                    memory->data[buf_ptr + used + 20u] = WASM__WASI_FILETYPE_UNKNOWN;
                    break;
            }
#endif
            memcpy(memory->data + buf_ptr + used + 24u, host_ent->d_name, name_len);
            used += record_size;
        }
        closedir(dir);
        wasm__store_le32(memory->data + out_ptr, used);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    }
#elif defined(_WIN32)
    {
        const char* base_path = wasm__wasi_entry_host_path(entry);
        char* glob_path = NULL;
        wchar_t* wide_glob_path = NULL;
        WIN32_FIND_DATAW find_data;
        HANDLE find_handle;
        uint64_t cookie = (uint64_t)args[3].of.i64;
        uint64_t index = 0u;
        uint32_t used = 0u;
        int done = 0;

        if (!base_path || !wasm__wasi_windows_join_path(base_path, "*", &glob_path)) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
            return WASM_OK;
        }
        if (!wasm__wasi_windows_utf8_to_wide(glob_path, &wide_glob_path)) {
            WASM_FREE(glob_path);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
            return WASM_OK;
        }
        find_handle = FindFirstFileW(wide_glob_path, &find_data);
        if (find_handle == INVALID_HANDLE_VALUE) {
            DWORD last_error = GetLastError();

            WASM_FREE(glob_path);
            WASM_FREE(wide_glob_path);
            if (last_error == ERROR_FILE_NOT_FOUND) {
                wasm__store_le32(memory->data + out_ptr, 0u);
                wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
            } else
                wasm__wasi_set_errno(results, num_results, wasm__wasi_windows_error_to_wasi(last_error));
            return WASM_OK;
        }

        while (!done) {
            char* name = NULL;
            size_t name_len;
            uint32_t record_size;

            if (index++ >= cookie) {
                if (!wasm__wasi_windows_wide_to_utf8(find_data.cFileName, &name)) {
                    FindClose(find_handle);
                    WASM_FREE(glob_path);
                    WASM_FREE(wide_glob_path);
                    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOMEM);
                    return WASM_OK;
                }
                name_len = strlen(name);
                record_size = 24u + (uint32_t)name_len;
                if (used + record_size > buf_len) {
                    WASM_FREE(name);
                    break;
                }
                memset(memory->data + buf_ptr + used, 0, 24u);
                wasm__store_le64(memory->data + buf_ptr + used + 0u, index);
                wasm__store_le64(memory->data + buf_ptr + used + 8u, 0u);
                wasm__store_le32(memory->data + buf_ptr + used + 16u, (uint32_t)name_len);
                memory->data[buf_ptr + used + 20u] =
                    wasm__wasi_windows_filetype_from_attributes(find_data.dwFileAttributes);
                memcpy(memory->data + buf_ptr + used + 24u, name, name_len);
                used += record_size;
                WASM_FREE(name);
            }
            if (!FindNextFileW(find_handle, &find_data)) {
                DWORD last_error = GetLastError();

                if (last_error != ERROR_NO_MORE_FILES) {
                    FindClose(find_handle);
                    WASM_FREE(glob_path);
                    WASM_FREE(wide_glob_path);
                    wasm__wasi_set_errno(results, num_results, wasm__wasi_windows_error_to_wasi(last_error));
                    return WASM_OK;
                }
                done = 1;
            }
        }

        FindClose(find_handle);
        WASM_FREE(glob_path);
        WASM_FREE(wide_glob_path);
        wasm__store_le32(memory->data + out_ptr, used);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    }
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOSYS);
#endif
    return WASM_OK;
}

#endif
