#include "../internal.h"

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE int32_t wasm__emscripten_errno_from_host(int err_code) {
    switch (err_code) {
        case 0:
            return 0;
#ifdef EPERM
        case EPERM:
            return 1;
#endif
#ifdef ENOENT
        case ENOENT:
            return 2;
#endif
#ifdef EINTR
        case EINTR:
            return 4;
#endif
#ifdef EIO
        case EIO:
            return 5;
#endif
#ifdef EBADF
        case EBADF:
            return 9;
#endif
#ifdef EAGAIN
        case EAGAIN:
            return 11;
#endif
#ifdef ENOMEM
        case ENOMEM:
            return 12;
#endif
#ifdef EACCES
        case EACCES:
            return 13;
#endif
#ifdef EFAULT
        case EFAULT:
            return 14;
#endif
#ifdef EBUSY
        case EBUSY:
            return 16;
#endif
#ifdef EEXIST
        case EEXIST:
            return 17;
#endif
#ifdef ENODEV
        case ENODEV:
            return 19;
#endif
#ifdef ENOTDIR
        case ENOTDIR:
            return 20;
#endif
#ifdef EISDIR
        case EISDIR:
            return 21;
#endif
#ifdef EINVAL
        case EINVAL:
            return 22;
#endif
#ifdef EMFILE
        case EMFILE:
            return 24;
#endif
#ifdef ENOTTY
        case ENOTTY:
            return 25;
#endif
#ifdef EFBIG
        case EFBIG:
            return 27;
#endif
#ifdef ENOSPC
        case ENOSPC:
            return 28;
#endif
#ifdef ESPIPE
        case ESPIPE:
            return 29;
#endif
#ifdef EROFS
        case EROFS:
            return 30;
#endif
#ifdef EPIPE
        case EPIPE:
            return 32;
#endif
#ifdef ERANGE
        case ERANGE:
            return 34;
#endif
#ifdef ENAMETOOLONG
        case ENAMETOOLONG:
            return 36;
#endif
#ifdef ENOSYS
        case ENOSYS:
            return 38;
#endif
#ifdef ENOTEMPTY
        case ENOTEMPTY:
            return 39;
#endif
#ifdef ELOOP
        case ELOOP:
            return 40;
#endif
        default:
            return 5;
    }
}

WASM_PRIVATE int32_t wasm__emscripten_neg_errno(int err_code) {
    if (err_code <= 0) err_code = EINVAL;
    return -wasm__emscripten_errno_from_host(err_code);
}

WASM_PRIVATE wasm_error_t wasm__emscripten_return_i32(wasm_value_t* results,
                                                uint32_t num_results,
                                                int32_t value) {
    if (!results || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    results[0] = wasm_i32(value);
    return WASM_OK;
}

WASM_PRIVATE int wasm__emscripten_read_guest_string(wasm_memory_t* memory,
                                              uint64_t mem_size,
                                              uint32_t ptr,
                                              char** out_string) {
    const uint8_t* start;
    const uint8_t* end;
    size_t available;
    size_t len;
    char* copy;

    if (!memory || !out_string || ptr > mem_size) return 0;
    *out_string = NULL;
    available = (size_t)(mem_size - (uint64_t)ptr);
    start = memory->data + ptr;
    end = (const uint8_t*)memchr(start, 0, available);
    if (!end) return 0;
    len = (size_t)(end - start);
    copy = (char*)WASM_MALLOC(len + 1u);
    if (!copy) return 0;
    if (len > 0u) memcpy(copy, start, len);
    copy[len] = '\0';
    *out_string = copy;
    return 1;
}

WASM_PRIVATE int wasm__emscripten_store_i32(wasm_memory_t* memory,
                                      uint64_t mem_size,
                                      uint32_t ptr,
                                      int32_t value) {
    if (!memory || !wasm__range_in_bounds_u64((uint64_t)ptr, 4u, mem_size)) return 0;
    wasm__store_le32(memory->data + ptr, (uint32_t)value);
    return 1;
}

WASM_PRIVATE int wasm__emscripten_store_c_string(wasm_memory_t* memory,
                                           uint64_t mem_size,
                                           uint32_t ptr,
                                           const char* text) {
    size_t len = text ? strlen(text) : 0u;

    if (!memory || !wasm__range_in_bounds_u64((uint64_t)ptr, (uint64_t)len + 1u, mem_size)) return 0;
    if (len > 0u) memcpy(memory->data + ptr, text, len);
    memory->data[ptr + len] = '\0';
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__ensure_emscripten_memory_import(wasm_runtime_t* rt,
                                                          uint64_t min_pages,
                                                          uint64_t max_pages,
                                                          int is_64) {
    wasm__emscripten_state_t* state;
    wasm_memory_import_t import_desc;
    size_t old_size = 0u;
    size_t new_size = 0u;
    uint8_t* new_data;

    if (!rt) return WASM_ERR_MALFORMED;

    state = rt->emscripten_state;
    if (state) {
        if (state->memory.is_64 != is_64 || state->memory.max_pages > max_pages || state->memory.pages > max_pages) {
            WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "%s", "builtin emscripten memory import type mismatch");
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (state->memory.pages >= min_pages) return WASM_OK;
        if (!wasm__memory_byte_size_fits_host(&state->memory, &old_size)) return WASM_ERR_OOM;
        state->memory.pages = min_pages;
        if (!wasm__memory_byte_size_fits_host(&state->memory, &new_size)) {
            state->memory.pages = old_size / WASM_PAGE_SIZE;
            WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "builtin emscripten memory import too large");
            return WASM_ERR_OOM;
        }
        state->memory.pages = old_size / WASM_PAGE_SIZE;
        new_data = (uint8_t*)WASM_REALLOC(state->memory.data, new_size);
        if (!new_data && new_size > 0u) return WASM_ERR_OOM;
        if (new_size > old_size) memset(new_data + old_size, 0, new_size - old_size);
        state->memory.data = new_data;
        state->memory.pages = min_pages;
        return WASM_OK;
    }

    state = (wasm__emscripten_state_t*)WASM_CALLOC(1u, sizeof(*state));
    if (!state) return WASM_ERR_OOM;

    state->memory.pages = min_pages;
    state->memory.max_pages = max_pages;
    state->memory.is_64 = is_64;
    if (!wasm__memory_byte_size_fits_host(&state->memory, &new_size)) {
        WASM_FREE(state);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "builtin emscripten memory import too large");
        return WASM_ERR_OOM;
    }
    if (new_size > 0u) {
        state->memory.data = (uint8_t*)WASM_CALLOC(1u, new_size);
        if (!state->memory.data) {
            WASM_FREE(state);
            return WASM_ERR_OOM;
        }
    }

    memset(&import_desc, 0, sizeof(import_desc));
    import_desc.module = WASM__EMSCRIPTEN_MODULE_NAME;
    import_desc.name = WASM__EMSCRIPTEN_MEMORY_NAME;
    import_desc.memory = &state->memory;

    rt->emscripten_state = state;
    if (wasm_register_memory_import(rt, &import_desc) != WASM_OK) {
        wasm_error_t err = rt->last_error;

        WASM_FREE(state->memory.data);
        WASM_FREE(state);
        rt->emscripten_state = NULL;
        return err != WASM_OK ? err : WASM_ERR_MALFORMED;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__emscripten_date_now(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    uint64_t now_ns = 0u;

    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 0 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (!wasm__platform_wasi_clock_time_get(WASM__WASI_CLOCKID_REALTIME, 0u, &now_ns))
        now_ns = (uint64_t)(time(NULL) > 0 ? time(NULL) : 0) * 1000000000ull;
    results[0] = wasm_f64((double)now_ns / 1000000.0);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__emscripten_get_now(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    uint64_t now_ns = 0u;

    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 0 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (!wasm__platform_wasi_clock_time_get(WASM__WASI_CLOCKID_MONOTONIC, 0u, &now_ns) &&
        !wasm__platform_wasi_clock_time_get(WASM__WASI_CLOCKID_REALTIME, 0u, &now_ns))
        now_ns = (uint64_t)(time(NULL) > 0 ? time(NULL) : 0) * 1000000000ull;
    results[0] = wasm_f64((double)now_ns / 1000000.0);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__emscripten_get_heap_max(wasm_module_t* mod,
                                                  const wasm_value_t* args, uint32_t num_args,
                                                  wasm_value_t* results, uint32_t num_results,
                                                  void* userdata) {
    wasm_memory_t* memory;

    (void)args;
    (void)userdata;

    if (!results || num_args != 0 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    memory = (mod && mod->rt && mod->rt->emscripten_state) ? &mod->rt->emscripten_state->memory : NULL;
    return wasm__emscripten_return_i32(results,
                                       num_results,
                                       memory ? (int32_t)(memory->max_pages * (uint64_t)WASM_PAGE_SIZE) : 0);
}

WASM_PRIVATE wasm_error_t wasm__emscripten_resize_heap(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata) {
    wasm_memory_t* memory;
    uint64_t requested_bytes;
    uint64_t target_pages;

    (void)userdata;

    if (!mod || !results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, 0);

    memory = wasm_memory_ref_at(mod, 0);
    if (!memory) return wasm__emscripten_return_i32(results, num_results, 0);

    requested_bytes = (uint64_t)(uint32_t)args[0].of.i32;
    target_pages = (requested_bytes + (uint64_t)WASM_PAGE_SIZE - 1u) / (uint64_t)WASM_PAGE_SIZE;
    if (target_pages <= memory->pages) return wasm__emscripten_return_i32(results, num_results, 1);
    if (target_pages > memory->max_pages) return wasm__emscripten_return_i32(results, num_results, 0);

    return wasm__emscripten_return_i32(results,
                                       num_results,
                                       wasm_memory_grow(mod, target_pages - memory->pages) >= 0 ? 1 : 0);
}

WASM_PRIVATE wasm_error_t wasm__emscripten_tzset_js(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    time_t now;
    struct tm* local_now;

    (void)results;
    (void)userdata;

    if (!mod || !args || num_args != 4 || num_results != 0) return WASM_ERR_TYPE_MISMATCH;
    if (wasm__wasi_get_memory0(mod, "_tzset_js", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;

    now = time(NULL);
    local_now = localtime(&now);
    if (args[0].of.i32 >= 0) (void)wasm__emscripten_store_i32(memory, mem_size, (uint32_t)args[0].of.i32, 0);
    if (args[1].of.i32 >= 0)
        (void)wasm__emscripten_store_i32(memory, mem_size, (uint32_t)args[1].of.i32,
                                         local_now && local_now->tm_isdst > 0 ? 1 : 0);
    if (args[2].of.i32 >= 0) (void)wasm__emscripten_store_c_string(memory, mem_size, (uint32_t)args[2].of.i32, "UTC");
    if (args[3].of.i32 >= 0) (void)wasm__emscripten_store_c_string(memory, mem_size, (uint32_t)args[3].of.i32, "UTC");
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__emscripten_localtime_js(wasm_module_t* mod,
                                                  const wasm_value_t* args, uint32_t num_args,
                                                  wasm_value_t* results, uint32_t num_results,
                                                  void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    time_t raw_time;
    struct tm* local_tm;
    uint32_t tm_ptr;

    (void)results;
    (void)userdata;

    if (!mod || !args || num_args != 2 || num_results != 0) return WASM_ERR_TYPE_MISMATCH;
    if (args[1].of.i32 < 0) return WASM_OK;
    if (wasm__wasi_get_memory0(mod, "_localtime_js", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;

    tm_ptr = (uint32_t)args[1].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)tm_ptr, 44u, mem_size)) return WASM_OK;

    raw_time = (time_t)args[0].of.i64;
    local_tm = localtime(&raw_time);
    if (!local_tm) return WASM_OK;

    wasm__store_le32(memory->data + tm_ptr + 0u, (uint32_t)local_tm->tm_sec);
    wasm__store_le32(memory->data + tm_ptr + 4u, (uint32_t)local_tm->tm_min);
    wasm__store_le32(memory->data + tm_ptr + 8u, (uint32_t)local_tm->tm_hour);
    wasm__store_le32(memory->data + tm_ptr + 12u, (uint32_t)local_tm->tm_mday);
    wasm__store_le32(memory->data + tm_ptr + 16u, (uint32_t)local_tm->tm_mon);
    wasm__store_le32(memory->data + tm_ptr + 20u, (uint32_t)local_tm->tm_year);
    wasm__store_le32(memory->data + tm_ptr + 24u, (uint32_t)local_tm->tm_wday);
    wasm__store_le32(memory->data + tm_ptr + 28u, (uint32_t)local_tm->tm_yday);
    wasm__store_le32(memory->data + tm_ptr + 32u, (uint32_t)local_tm->tm_isdst);
    wasm__store_le32(memory->data + tm_ptr + 36u, 0u);
    wasm__store_le32(memory->data + tm_ptr + 40u, 0u);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__emscripten_mmap_js(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;

    (void)userdata;

    if (!mod || !args || !results || num_args != 7 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (wasm__wasi_get_memory0(mod, "_mmap_js", &memory, &mem_size) == WASM_OK && args[5].of.i32 >= 0)
        (void)wasm__emscripten_store_i32(memory, mem_size, (uint32_t)args[5].of.i32, 0);
    return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(ENOSYS));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_munmap_js(wasm_module_t* mod,
                                               const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results,
                                               void* userdata) {
    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 6 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    return wasm__emscripten_return_i32(results, num_results, 0);
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_faccessat(wasm_module_t* mod,
                                                       const wasm_value_t* args, uint32_t num_args,
                                                       wasm_value_t* results, uint32_t num_results,
                                                       void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    int rc;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[1].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    if (wasm__wasi_get_memory0(mod, "__syscall_faccessat", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[1].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = faccessat(args[0].of.i32, path, args[2].of.i32, args[3].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_fchmod(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata) {
    int rc;

    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = fchmod(args[0].of.i32, (mode_t)(uint32_t)args[1].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_chmod(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    int rc;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    if (wasm__wasi_get_memory0(mod, "__syscall_chmod", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[0].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = chmod(path, (mode_t)(uint32_t)args[1].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_fchown32(wasm_module_t* mod,
                                                      const wasm_value_t* args, uint32_t num_args,
                                                      wasm_value_t* results, uint32_t num_results,
                                                      void* userdata) {
    int rc;

    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = fchown(args[0].of.i32, (uid_t)(uint32_t)args[1].of.i32, (gid_t)(uint32_t)args[2].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_fcntl64(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata) {
    int rc;

    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = fcntl(args[0].of.i32, args[1].of.i32, args[2].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    return wasm__emscripten_return_i32(results, num_results, rc >= 0 ? rc : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_openat(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    int rc;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[1].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    if (wasm__wasi_get_memory0(mod, "__syscall_openat", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[1].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = openat(args[0].of.i32, path, args[2].of.i32, (mode_t)(uint32_t)args[3].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc >= 0 ? rc : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_ioctl(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata) {
    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(ENOTTY));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_stat_unsupported(wasm_module_t* mod,
                                                              const wasm_value_t* args, uint32_t num_args,
                                                              wasm_value_t* results, uint32_t num_results,
                                                              void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)userdata;

    if (!results || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(ENOSYS));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_ftruncate64(wasm_module_t* mod,
                                                         const wasm_value_t* args, uint32_t num_args,
                                                         wasm_value_t* results, uint32_t num_results,
                                                         void* userdata) {
    int rc;

    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = ftruncate(args[0].of.i32, (off_t)args[1].of.i64);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_getcwd(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint32_t buf_ptr;
    uint32_t buf_len;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 <= 0)
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EINVAL));
    if (wasm__wasi_get_memory0(mod, "__syscall_getcwd", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;

    buf_ptr = (uint32_t)args[0].of.i32;
    buf_len = (uint32_t)args[1].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));

#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (!getcwd((char*)memory->data + buf_ptr, (size_t)buf_len))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(errno));
    return wasm__emscripten_return_i32(results,
                                       num_results,
                                       (int32_t)(strlen((char*)memory->data + buf_ptr) + 1u));
#else
    return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(ENOSYS));
#endif
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_mkdirat(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    int rc;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[1].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    if (wasm__wasi_get_memory0(mod, "__syscall_mkdirat", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[1].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = mkdirat(args[0].of.i32, path, (mode_t)(uint32_t)args[2].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_readlinkat(wasm_module_t* mod,
                                                        const wasm_value_t* args, uint32_t num_args,
                                                        wasm_value_t* results, uint32_t num_results,
                                                        void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    uint32_t buf_ptr;
    uint32_t buf_len;
    int32_t rc_value;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[1].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0)
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EINVAL));
    if (wasm__wasi_get_memory0(mod, "__syscall_readlinkat", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[1].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));

    buf_ptr = (uint32_t)args[2].of.i32;
    buf_len = (uint32_t)args[3].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
        WASM_FREE(path);
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    }

#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    {
        ssize_t rc = readlinkat(args[0].of.i32, path, (char*)memory->data + buf_ptr, (size_t)buf_len);

        rc_value = rc >= 0 ? (int32_t)rc : wasm__emscripten_neg_errno(errno);
    }
#else
    rc_value = wasm__emscripten_neg_errno(ENOSYS);
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc_value);
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_rmdir(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    int rc;

    (void)userdata;

    if (!mod || !args || !results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    if (wasm__wasi_get_memory0(mod, "__syscall_rmdir", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[0].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = rmdir(path);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_unlinkat(wasm_module_t* mod,
                                                      const wasm_value_t* args, uint32_t num_args,
                                                      wasm_value_t* results, uint32_t num_results,
                                                      void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    char* path = NULL;
    int rc;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[1].of.i32 < 0) return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
    if (wasm__wasi_get_memory0(mod, "__syscall_unlinkat", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (!wasm__emscripten_read_guest_string(memory, mem_size, (uint32_t)args[1].of.i32, &path))
        return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(EFAULT));
#if defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    rc = unlinkat(args[0].of.i32, path, args[2].of.i32);
#else
    rc = -1;
    errno = ENOSYS;
#endif
    WASM_FREE(path);
    return wasm__emscripten_return_i32(results, num_results, rc == 0 ? 0 : wasm__emscripten_neg_errno(errno));
}

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_utimensat(wasm_module_t* mod,
                                                       const wasm_value_t* args, uint32_t num_args,
                                                       wasm_value_t* results, uint32_t num_results,
                                                       void* userdata) {
    (void)mod;
    (void)args;
    (void)userdata;

    if (!results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    return wasm__emscripten_return_i32(results, num_results, wasm__emscripten_neg_errno(ENOSYS));
}

wasm_error_t wasm_bind_emscripten_stubs(wasm_runtime_t* rt) {
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;

    rt->emscripten_stubs_enabled = 1;

    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "emscripten_date_now", "(F)",
                              wasm__emscripten_date_now, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "emscripten_get_now", "(F)",
                              wasm__emscripten_get_now, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "emscripten_get_heap_max", "(i)",
                              wasm__emscripten_get_heap_max, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "emscripten_resize_heap", "i(i)",
                              wasm__emscripten_resize_heap, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "_tzset_js", "iiii(v)",
                              wasm__emscripten_tzset_js, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "_localtime_js", "Ii(v)",
                              wasm__emscripten_localtime_js, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "_mmap_js", "iiiiIii(i)",
                              wasm__emscripten_mmap_js, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "_munmap_js", "iiiiiI(i)",
                              wasm__emscripten_munmap_js, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_faccessat", "iiii(i)",
                              wasm__emscripten_syscall_faccessat, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_fchmod", "ii(i)",
                              wasm__emscripten_syscall_fchmod, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_chmod", "ii(i)",
                              wasm__emscripten_syscall_chmod, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_fchown32", "iii(i)",
                              wasm__emscripten_syscall_fchown32, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_fcntl64", "iii(i)",
                              wasm__emscripten_syscall_fcntl64, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_openat", "iiii(i)",
                              wasm__emscripten_syscall_openat, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_ioctl", "iii(i)",
                              wasm__emscripten_syscall_ioctl, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_fstat64", "ii(i)",
                              wasm__emscripten_syscall_stat_unsupported, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_stat64", "ii(i)",
                              wasm__emscripten_syscall_stat_unsupported, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_newfstatat", "iiii(i)",
                              wasm__emscripten_syscall_stat_unsupported, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_lstat64", "ii(i)",
                              wasm__emscripten_syscall_stat_unsupported, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_ftruncate64", "iI(i)",
                              wasm__emscripten_syscall_ftruncate64, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_getcwd", "ii(i)",
                              wasm__emscripten_syscall_getcwd, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_mkdirat", "iii(i)",
                              wasm__emscripten_syscall_mkdirat, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_readlinkat", "iiii(i)",
                              wasm__emscripten_syscall_readlinkat, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_rmdir", "i(i)",
                              wasm__emscripten_syscall_rmdir, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_unlinkat", "iii(i)",
                              wasm__emscripten_syscall_unlinkat, NULL);
    if (err != WASM_OK) return err;
    err = wasm_bind_host_func(rt, WASM__EMSCRIPTEN_MODULE_NAME, "__syscall_utimensat", "iiii(i)",
                              wasm__emscripten_syscall_utimensat, NULL);
    if (err != WASM_OK) return err;

    return WASM_OK;
}

#endif
