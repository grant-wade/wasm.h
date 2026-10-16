#include "../internal.h"

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE void wasm__wasi_set_errno(wasm_value_t* results, uint32_t num_results, uint32_t errno_code) {
    if (results && num_results > 0) results[0] = wasm_i32((int32_t)errno_code);
}

WASM_PRIVATE uint32_t wasm__wasi_errno_from_host(int err_code) {
    switch (err_code) {
        case 0:
            return WASM__WASI_ERRNO_SUCCESS;
#ifdef EACCES
        case EACCES:
            return WASM__WASI_ERRNO_ACCES;
#endif
#ifdef EAGAIN
        case EAGAIN:
            return WASM__WASI_ERRNO_AGAIN;
#endif
#ifdef EBADF
        case EBADF:
            return WASM__WASI_ERRNO_BADF;
#endif
#ifdef EEXIST
        case EEXIST:
            return WASM__WASI_ERRNO_EXIST;
#endif
#ifdef EFAULT
        case EFAULT:
            return WASM__WASI_ERRNO_FAULT;
#endif
#ifdef EINVAL
        case EINVAL:
            return WASM__WASI_ERRNO_INVAL;
#endif
#ifdef EISDIR
        case EISDIR:
            return WASM__WASI_ERRNO_ISDIR;
#endif
#ifdef ENOENT
        case ENOENT:
            return WASM__WASI_ERRNO_NOENT;
#endif
#ifdef ENOMEM
        case ENOMEM:
            return WASM__WASI_ERRNO_NOMEM;
#endif
#ifdef ENOSPC
        case ENOSPC:
            return WASM__WASI_ERRNO_NOSPC;
#endif
#ifdef ENOSYS
        case ENOSYS:
            return WASM__WASI_ERRNO_NOSYS;
#endif
#ifdef ENOTDIR
        case ENOTDIR:
            return WASM__WASI_ERRNO_NOTDIR;
#endif
#ifdef ENOTEMPTY
        case ENOTEMPTY:
            return WASM__WASI_ERRNO_NOTEMPTY;
#endif
#ifdef ENOTSUP
        case ENOTSUP:
            return WASM__WASI_ERRNO_NOTSUP;
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || EOPNOTSUPP != ENOTSUP)
        case EOPNOTSUPP:
            return WASM__WASI_ERRNO_NOTSUP;
#endif
#ifdef EPERM
        case EPERM:
            return WASM__WASI_ERRNO_PERM;
#endif
#ifdef EPIPE
        case EPIPE:
            return WASM__WASI_ERRNO_PIPE;
#endif
#ifdef ESPIPE
        case ESPIPE:
            return WASM__WASI_ERRNO_SPIPE;
#endif
        default:
            return WASM__WASI_ERRNO_IO;
    }
}

WASM_PRIVATE wasm_error_t wasm__wasi_get_memory0(wasm_module_t* mod,
                                           const char* op_name,
                                           wasm_memory_t** out_memory,
                                           uint64_t* out_mem_size) {
    wasm_runtime_t* rt;
    wasm_memory_t* memory;

    if (!mod || !mod->rt || !op_name || !out_memory || !out_mem_size) return WASM_ERR_MALFORMED;

    rt = mod->rt;

    memory = wasm__memory_at(mod, 0);
    if (!memory) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "wasi %s requires memory 0", op_name);
        return WASM_ERR_MALFORMED;
    }

    *out_memory = memory;
    *out_mem_size = wasm__memory_byte_size(memory);
    return WASM_OK;
}

WASM_PRIVATE uint64_t wasm__wasi_string_list_size(wasm__wasi_string_count_fn count_fn,
                                            wasm__wasi_string_at_fn at_fn) {
    uint32_t count;
    uint32_t i;
    uint64_t total = 0;

    if (!count_fn || !at_fn) return UINT64_MAX;

    count = count_fn();
    for (i = 0; i < count; i++) {
        const char* value = at_fn(i);
        size_t len = value ? strlen(value) : 0u;

        if ((uint64_t)len >= UINT64_MAX - total) return UINT64_MAX;
        total += (uint64_t)len + 1u;
    }

    return total;
}

WASM_PRIVATE uint32_t wasm__wasi_string_list_count_args(void) {
    return wasm__platform_wasi_arg_count();
}

WASM_PRIVATE const char* wasm__wasi_string_list_at_args(uint32_t index) {
    return wasm__platform_wasi_arg_at(index);
}

WASM_PRIVATE uint32_t wasm__wasi_string_list_count_env(void) {
    return wasm__platform_wasi_env_count();
}

WASM_PRIVATE const char* wasm__wasi_string_list_at_env(uint32_t index) {
    return wasm__platform_wasi_env_at(index);
}

WASM_PRIVATE uint32_t wasm__wasi_string_sizes_get(wasm_module_t* mod,
                                            wasm__wasi_string_count_fn count_fn,
                                            wasm__wasi_string_at_fn at_fn,
                                            uint32_t count_ptr,
                                            uint32_t size_ptr,
                                            wasm_value_t* results,
                                            uint32_t num_results) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t total_size;
    uint32_t count;
    wasm_error_t err;

    err = wasm__wasi_get_memory0(mod, "string sizes_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    count = count_fn ? count_fn() : 0u;
    total_size = wasm__wasi_string_list_size(count_fn, at_fn);
    if (total_size == UINT64_MAX || total_size > UINT32_MAX) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)count_ptr, 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)size_ptr, 4u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    wasm__store_le32(memory->data + count_ptr, count);
    wasm__store_le32(memory->data + size_ptr, (uint32_t)total_size);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE uint32_t wasm__wasi_string_list_get(wasm_module_t* mod,
                                           wasm__wasi_string_count_fn count_fn,
                                           wasm__wasi_string_at_fn at_fn,
                                           uint32_t entries_ptr,
                                           uint32_t data_ptr,
                                           wasm_value_t* results,
                                           uint32_t num_results) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t strings_size;
    uint32_t count;
    uint32_t i;
    uint32_t cursor;
    wasm_error_t err;

    err = wasm__wasi_get_memory0(mod, "string get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    count = count_fn ? count_fn() : 0u;
    strings_size = wasm__wasi_string_list_size(count_fn, at_fn);
    if (strings_size == UINT64_MAX || strings_size > UINT32_MAX) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)entries_ptr, (uint64_t)count * 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)data_ptr, strings_size, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    cursor = data_ptr;
    for (i = 0; i < count; i++) {
        const char* value = at_fn(i);
        size_t len = value ? strlen(value) : 0u;

        wasm__store_le32(memory->data + entries_ptr + (size_t)i * 4u, cursor);
        if (len > 0) memcpy(memory->data + cursor, value, len);
        memory->data[cursor + len] = 0;
        cursor += (uint32_t)len + 1u;
    }

    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE int wasm__wasi_is_stdio_fd(uint32_t fd) {
    return fd == WASM__WASI_FD_STDIN || fd == WASM__WASI_FD_STDOUT || fd == WASM__WASI_FD_STDERR;
}

WASM_PRIVATE wasm_error_t wasm__wasi_args_sizes_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_sizes_get(mod,
                                       wasm__wasi_string_list_count_args,
                                       wasm__wasi_string_list_at_args,
                                       (uint32_t)args[0].of.i32,
                                       (uint32_t)args[1].of.i32,
                                       results,
                                       num_results);
}

WASM_PRIVATE wasm_error_t wasm__wasi_args_get(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_list_get(mod,
                                      wasm__wasi_string_list_count_args,
                                      wasm__wasi_string_list_at_args,
                                      (uint32_t)args[0].of.i32,
                                      (uint32_t)args[1].of.i32,
                                      results,
                                      num_results);
}

WASM_PRIVATE wasm_error_t wasm__wasi_environ_sizes_get(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_sizes_get(mod,
                                       wasm__wasi_string_list_count_env,
                                       wasm__wasi_string_list_at_env,
                                       (uint32_t)args[0].of.i32,
                                       (uint32_t)args[1].of.i32,
                                       results,
                                       num_results);
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_write(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    wasm_memory_t* memory;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    FILE* stream;
    uint64_t mem_size;
    uint64_t total_written = 0;
    uint32_t fd;
    uint32_t iovs_ptr;
    uint32_t iovs_len;
    uint32_t nwritten_ptr;
    uint32_t i;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;

    err = wasm__wasi_get_memory0(mod, "fd_write", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[1].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    iovs_ptr = (uint32_t)args[1].of.i32;
    iovs_len = (uint32_t)args[2].of.i32;
    nwritten_ptr = (uint32_t)args[3].of.i32;

    entry = wasm__wasi_fd_entry(rt, fd);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    if (fd == WASM__WASI_FD_STDOUT)
        stream = stdout;
    else if (fd == WASM__WASI_FD_STDERR)
        stream = stderr;
    else {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    if (!wasm__range_in_bounds_u64((uint64_t)nwritten_ptr, 4u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)iovs_ptr, (uint64_t)iovs_len * 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    for (i = 0; i < iovs_len; i++) {
        const uint8_t* iov_base = memory->data + iovs_ptr + (size_t)i * 8u;
        uint32_t buf_ptr = wasm__load_le32(iov_base);
        uint32_t buf_len = wasm__load_le32(iov_base + 4u);
        size_t written = 0u;

        if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
            return WASM_OK;
        }
        if (buf_len == 0) continue;

        if (stream)
            written = fwrite(memory->data + buf_ptr, 1u, buf_len, stream);
        else {
            if (!wasm__wasi_host_write(entry->host_fd, memory->data + buf_ptr, (size_t)buf_len, &written)) {
                wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
                return WASM_OK;
            }
        }
        if (written != buf_len) {
            if (stream) clearerr(stream);
            wasm__wasi_set_errno(results, num_results,
                                 written > 0u ? WASM__WASI_ERRNO_SUCCESS : WASM__WASI_ERRNO_IO);
            if (written > 0u) total_written += written;
            break;
            return WASM_OK;
        }
        total_written += written;
        if (total_written > UINT32_MAX) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
            return WASM_OK;
        }
    }

    if (stream) fflush(stream);
    wasm__store_le32(memory->data + nwritten_ptr, (uint32_t)total_written);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_read(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata) {
    wasm_memory_t* memory;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint64_t mem_size;
    uint64_t total_read = 0;
    uint32_t fd;
    uint32_t iovs_ptr;
    uint32_t iovs_len;
    uint32_t nread_ptr;
    uint32_t i;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;

    err = wasm__wasi_get_memory0(mod, "fd_read", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[1].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    iovs_ptr = (uint32_t)args[1].of.i32;
    iovs_len = (uint32_t)args[2].of.i32;
    nread_ptr = (uint32_t)args[3].of.i32;

    entry = wasm__wasi_fd_entry(rt, fd);
    if (!entry || entry->host_fd < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    if (!wasm__range_in_bounds_u64((uint64_t)nread_ptr, 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)iovs_ptr, (uint64_t)iovs_len * 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    for (i = 0; i < iovs_len; i++) {
        uint8_t* iov_base = memory->data + iovs_ptr + (size_t)i * 8u;
        uint32_t buf_ptr = wasm__load_le32(iov_base);
        uint32_t buf_len = wasm__load_le32(iov_base + 4u);
        size_t read_count = 0u;

        if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
            return WASM_OK;
        }
        if (buf_len == 0) continue;

        if (!wasm__wasi_host_read(entry->host_fd, memory->data + buf_ptr, (size_t)buf_len, &read_count)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }
        total_read += (uint64_t)read_count;
        if (read_count != (size_t)buf_len) break;

        if (total_read > UINT32_MAX) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
            return WASM_OK;
        }
    }

    wasm__store_le32(memory->data + nread_ptr, (uint32_t)total_read);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_proc_exit(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata) {
    (void)mod;
    (void)args;
    (void)results;
    (void)userdata;

    if (num_args != 1 || num_results != 0) return WASM_ERR_TYPE_MISMATCH;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_close(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint32_t fd;

    (void)mod;
    (void)userdata;

    if (!results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod ? mod->rt : NULL;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    entry = wasm__wasi_fd_entry(rt, fd);
    if (!entry) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (fd == WASM__WASI_FD_STDIN || fd == WASM__WASI_FD_STDOUT || fd == WASM__WASI_FD_STDERR)
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    else {
        if (entry->host_fd < 0) {
            wasm__wasi_fd_entry_reset(entry);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        } else if (wasm__wasi_host_close(entry->host_fd) == 0) {
            entry->owns_host_fd = 0;
            wasm__wasi_fd_entry_reset(entry);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        } else
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_seek(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata) {
    wasm_memory_t* memory;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint64_t mem_size;
    uint32_t fd;
    uint32_t whence;
    uint32_t newoffset_ptr;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;

    err = wasm__wasi_get_memory0(mod, "fd_seek", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    whence = (uint32_t)args[2].of.i32;
    newoffset_ptr = (uint32_t)args[3].of.i32;
    entry = wasm__wasi_fd_entry(rt, fd);

    if (whence != WASM__WASI_WHENCE_SET && whence != WASM__WASI_WHENCE_CUR && whence != WASM__WASI_WHENCE_END) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)newoffset_ptr, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    if (wasm__wasi_is_stdio_fd(fd)) {
        wasm__store_le64(memory->data + newoffset_ptr, 0u);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }

    {
        int native_whence = SEEK_SET;
        uint64_t new_offset = 0u;

        if (!entry || entry->host_fd < 0) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
            return WASM_OK;
        }

        if (whence == WASM__WASI_WHENCE_CUR)
            native_whence = SEEK_CUR;
        else if (whence == WASM__WASI_WHENCE_END)
            native_whence = SEEK_END;

        if (!wasm__wasi_host_seek(entry->host_fd, args[1].of.i64, native_whence, &new_offset)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }

        wasm__store_le64(memory->data + newoffset_ptr, new_offset);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_sync(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata) {
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint32_t fd;

    (void)mod;
    (void)userdata;

    if (!results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod ? mod->rt : NULL;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    entry = wasm__wasi_fd_entry(rt, fd);
    if (fd == WASM__WASI_FD_STDOUT) fflush(stdout);
    if (fd == WASM__WASI_FD_STDERR) fflush(stderr);
    if (fd == WASM__WASI_FD_STDIN || fd == WASM__WASI_FD_STDOUT || fd == WASM__WASI_FD_STDERR) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }

    if (!entry || entry->host_fd < 0)
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
    else if (wasm__wasi_host_sync(entry->host_fd))
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    else
        wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_environ_get(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_list_get(mod,
                                      wasm__wasi_string_list_count_env,
                                      wasm__wasi_string_list_at_env,
                                      (uint32_t)args[0].of.i32,
                                      (uint32_t)args[1].of.i32,
                                      results,
                                      num_results);
}

WASM_PRIVATE wasm_error_t wasm__wasi_fd_fdstat_get(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    wasm_memory_t* memory;
    wasm_runtime_t* rt;
    wasm__wasi_fd_entry_t* entry;
    uint64_t mem_size;
    uint32_t fd;
    uint32_t stat_ptr;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    rt = mod->rt;
    if (wasm__wasi_state_ensure(rt) != WASM_OK) return WASM_ERR_OOM;
    err = wasm__wasi_get_memory0(mod, "fd_fdstat_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    stat_ptr = (uint32_t)args[1].of.i32;
    entry = wasm__wasi_fd_entry(rt, fd);
    if (!entry || !wasm__wasi_entry_can_path(entry)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)stat_ptr, 24u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    memory->data[stat_ptr + 1u] = 0u;
    wasm__store_le16(memory->data + stat_ptr + 2u, 0u);
    memset(memory->data + stat_ptr + 4u, 0, 4u);

    if (wasm__wasi_is_stdio_fd(fd)) {
        memory->data[stat_ptr + 0u] = entry->filetype;
        wasm__store_le64(memory->data + stat_ptr + 8u, entry->rights_base);
        wasm__store_le64(memory->data + stat_ptr + 16u, entry->rights_inheriting);
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
        return WASM_OK;
    }

    {
        wasm__wasi_host_stat_t st;

        if (!wasm__wasi_entry_stat(entry, &st)) {
            wasm__wasi_set_errno(results, num_results, wasm__wasi_errno_from_host(errno));
            return WASM_OK;
        }

        entry->filetype = wasm__wasi_filetype_from_mode(st.st_mode);
        memory->data[stat_ptr + 0u] = entry->filetype;
        wasm__store_le64(memory->data + stat_ptr + 8u, entry->rights_base);
        wasm__store_le64(memory->data + stat_ptr + 16u, entry->rights_inheriting);
    }
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_random_get(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint32_t buf_ptr;
    uint32_t buf_len;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    err = wasm__wasi_get_memory0(mod, "random_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    buf_ptr = (uint32_t)args[0].of.i32;
    buf_len = (uint32_t)args[1].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (!wasm__platform_wasi_fill_random(memory->data + buf_ptr, buf_len)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_IO);
        return WASM_OK;
    }

    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_clock_time_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t timestamp;
    uint32_t clock_id;
    uint32_t out_ptr;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    err = wasm__wasi_get_memory0(mod, "clock_time_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[2].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    clock_id = (uint32_t)args[0].of.i32;
    out_ptr = (uint32_t)args[2].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)out_ptr, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (clock_id != WASM__WASI_CLOCKID_REALTIME &&
        clock_id != WASM__WASI_CLOCKID_MONOTONIC &&
        clock_id != WASM__WASI_CLOCKID_PROCESS_CPUTIME_ID &&
        clock_id != WASM__WASI_CLOCKID_THREAD_CPUTIME_ID) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__platform_wasi_clock_time_get(clock_id, (uint64_t)args[1].of.i64, &timestamp)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_IO);
        return WASM_OK;
    }

    wasm__store_le64(memory->data + out_ptr, timestamp);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__wasi_clock_res_get(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint32_t clock_id;
    uint32_t out_ptr;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (wasm__wasi_get_memory0(mod, "clock_res_get", &memory, &mem_size) != WASM_OK) return WASM_ERR_MALFORMED;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    clock_id = (uint32_t)args[0].of.i32;
    out_ptr = (uint32_t)args[1].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)out_ptr, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    if (clock_id != WASM__WASI_CLOCKID_REALTIME &&
        clock_id != WASM__WASI_CLOCKID_MONOTONIC &&
        clock_id != WASM__WASI_CLOCKID_PROCESS_CPUTIME_ID &&
        clock_id != WASM__WASI_CLOCKID_THREAD_CPUTIME_ID) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    wasm__store_le64(memory->data + out_ptr, 1u);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

#endif
