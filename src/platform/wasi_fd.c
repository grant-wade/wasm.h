#include "wasi_fd.h"
#include "host.h"
#include "../module.h"
#include "../portable.h"
#include "../runtime.h"
#include "wasi_state.h"
#include "wasi_io.h"
#include "../memory_internal.h"
#include <errno.h>
#include <stdio.h>

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE wasm_error_t wasm__wasi_fd_datasync(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;

    (void)userdata;

    if (!mod || !results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (wasm__wasi_is_stdio_fd((uint32_t)args[0].of.i32)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }
    if (wasm__wasi_host_datasync(entry->host_fd))
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    else
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_fdstat_set_flags(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint32_t flags;

    (void)userdata;

    if (!mod || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    flags = (uint32_t)args[1].of.i32;
    entry->fdflags = (uint16_t)flags;

#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (!wasm__wasi_is_stdio_fd((uint32_t)args[0].of.i32)) {
        int host_flags = fcntl(entry->host_fd, F_GETFL);

        if (host_flags < 0) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
#ifdef O_APPEND
        if (flags & 0x1u)
            host_flags |= O_APPEND;
        else
            host_flags &= ~O_APPEND;
#endif
#ifdef O_NONBLOCK
        if (flags & 0x4u)
            host_flags |= O_NONBLOCK;
        else
            host_flags &= ~O_NONBLOCK;
#endif
        if (fcntl(entry->host_fd, F_SETFL, host_flags) != 0) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
    }
#endif
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_fdstat_set_rights(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint64_t base;
    uint64_t inheriting;

    (void)userdata;

    if (!mod || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    base = (uint64_t)args[1].of.i64;
    inheriting = (uint64_t)args[2].of.i64;
    if ((base & ~entry->rights_base) != 0u || (inheriting & ~entry->rights_inheriting) != 0u) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_NOTCAPABLE);
        return WASM_OK;
    }
    entry->rights_base = base;
    entry->rights_inheriting = inheriting;
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_filestat_get(wasm_module_t* mod,
                                               const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results,
                                               void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_filestat_get", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[1].of.i32, 64u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || !wasm__wasi_entry_has_host(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    {
        wasm__wasi_host_stat_t st;

        if (!wasm__wasi_entry_stat(&mod->rt->allocator, entry, &st)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        wasm__wasi_store_filestat(memory->data + (uint32_t)args[1].of.i32, &st);
    }
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_filestat_set_size(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;

    (void)userdata;

    if (!mod || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__wasi_host_ftruncate(entry->host_fd, (uint64_t)args[1].of.i64))
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_tell(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (wasm__wasi_get_memory0(mod, "fd_tell", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)(uint32_t)args[1].of.i32, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (wasm__wasi_is_stdio_fd((uint32_t)args[0].of.i32)) {
        wasm__store_le64(memory->data + (uint32_t)args[1].of.i32, 0u);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    {
        uint64_t offset = 0u;

        if (!wasm__wasi_host_seek(entry->host_fd, 0, SEEK_CUR, &offset)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        wasm__store_le64(memory->data + (uint32_t)args[1].of.i32, offset);
    }
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_advise(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;

    (void)userdata;

    if (!mod || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_allocate(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;

    (void)userdata;

    if (!mod || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    entry = wasm__wasi_fd_entry(rt, (uint32_t)args[0].of.i32);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if ((uint64_t)args[2].of.i64 == 0u) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }
#if defined(__linux__)
    if (posix_fallocate(entry->host_fd, (off_t)args[1].of.i64, (off_t)args[2].of.i64) == 0)
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    else
#endif
    {
        wasm__wasi_host_stat_t st;
        uint64_t requested_end = (uint64_t)args[1].of.i64 + (uint64_t)args[2].of.i64;

        if (requested_end < (uint64_t)args[1].of.i64) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
            return WASM_OK;
        }
        if (!wasm__wasi_host_fstat(entry->host_fd, &st)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        if ((uint64_t)st.st_size < requested_end && !wasm__wasi_host_ftruncate(entry->host_fd, requested_end)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_sched_yield(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 0 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (sched_yield() != 0)
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#else
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
#endif
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_errno_stub(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata) {
    uint32_t errno_code = (uint32_t)(uintptr_t)userdata;

    (void)mod;
    (void)args;
    (void)num_args;

    if (!results || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    wasm__wasi_set_errno(results, num_results, errno_code);
    return WASM_OK;
}

#endif
