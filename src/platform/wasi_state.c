#include "wasi_state.h"
#include "host.h"
#include "../runtime.h"
#include "../internal/alloc.h"
#include "../portable.h"
#include "../memory_internal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE char* wasm__wasi_strdup(const char* text) {
    size_t len;
    char* copy;

    if (!text) return NULL;
    len = strlen(text);
    copy = (char*)WASM_MALLOC(len + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
}

WASM_PRIVATE int wasm__wasi_host_close(int fd) {
#if defined(_WIN32)
    return _close(fd);
#else
    return close(fd);
#endif
}

static int wasm__wasi_host_fileno(FILE* stream) {
#if defined(_WIN32)
    return _fileno(stream);
#else
    return fileno(stream);
#endif
}

WASM_PRIVATE uint8_t wasm__wasi_filetype_from_mode(int mode) {
#if defined(_WIN32)
    switch (mode & _S_IFMT) {
        case _S_IFCHR:
            return WASM__WASI_FILETYPE_CHARACTER_DEVICE;
        case _S_IFDIR:
            return WASM__WASI_FILETYPE_DIRECTORY;
        case _S_IFREG:
            return WASM__WASI_FILETYPE_REGULAR_FILE;
        default:
            return WASM__WASI_FILETYPE_UNKNOWN;
    }
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
    if (S_ISBLK(mode)) return WASM__WASI_FILETYPE_BLOCK_DEVICE;
    if (S_ISCHR(mode)) return WASM__WASI_FILETYPE_CHARACTER_DEVICE;
    if (S_ISDIR(mode)) return WASM__WASI_FILETYPE_DIRECTORY;
    if (S_ISREG(mode)) return WASM__WASI_FILETYPE_REGULAR_FILE;
#ifdef S_ISLNK
    if (S_ISLNK(mode)) return WASM__WASI_FILETYPE_SYMBOLIC_LINK;
#endif
#ifdef S_ISSOCK
    if (S_ISSOCK(mode)) return WASM__WASI_FILETYPE_SOCKET_STREAM;
#endif
#else
    (void)mode;
#endif
    return WASM__WASI_FILETYPE_UNKNOWN;
}

WASM_PRIVATE void wasm__wasi_fd_entry_reset(wasm__wasi_fd_entry_t* entry) {
    if (!entry) return;
    if (entry->owns_host_fd && entry->host_fd >= 0) (void)wasm__wasi_host_close(entry->host_fd);
    WASM_FREE(entry->host_path);
    WASM_FREE(entry->preopen_path);
    memset(entry, 0, sizeof(*entry));
    entry->host_fd = -1;
}

WASM_PRIVATE void wasm__wasi_state_destroy(wasm_runtime_t* rt) {
    uint32_t i;
    wasm__wasi_state_t* state;

    if (!rt || !rt->wasi_state) return;

    state = rt->wasi_state;
    for (i = 0; i < WASM__WASI_MAX_FDS; i++) wasm__wasi_fd_entry_reset(&state->fds[i]);
    WASM_FREE(state);
    rt->wasi_state = NULL;
}

static void wasm__wasi_fd_entry_init_stdio(wasm__wasi_fd_entry_t* entry,
                                           int host_fd,
                                           uint8_t filetype) {
    if (!entry) return;
    memset(entry, 0, sizeof(*entry));
    entry->used = 1;
    entry->host_fd = host_fd;
    entry->owns_host_fd = 0;
    entry->filetype = filetype;
    entry->rights_base = WASM__WASI_RIGHTS_ALL;
    entry->rights_inheriting = WASM__WASI_RIGHTS_ALL;
}

WASM_PRIVATE wasm_error_t wasm__wasi_state_add_fd(wasm_runtime_t* rt,
                                            int host_fd,
                                            int owns_host_fd,
                                            uint8_t filetype,
                                            uint64_t rights_base,
                                            uint64_t rights_inheriting,
                                            const char* host_path,
                                            const char* preopen_path,
                                            uint32_t* out_fd) {
    uint32_t i;
    wasm__wasi_fd_entry_t* entry;
    char* copied_host_path = NULL;
    char* copied_preopen_path = NULL;

    if (!rt || !rt->wasi_state) return WASM_ERR_MALFORMED;
    if (host_path) {
        copied_host_path = wasm__wasi_strdup(host_path);
        if (!copied_host_path) return WASM_ERR_OOM;
    }
    if (preopen_path) {
        copied_preopen_path = wasm__wasi_strdup(preopen_path);
        if (!copied_preopen_path) {
            WASM_FREE(copied_host_path);
            return WASM_ERR_OOM;
        }
    }

    for (i = 3u; i < WASM__WASI_MAX_FDS; i++) {
        entry = &rt->wasi_state->fds[i];
        if (entry->used) continue;
        memset(entry, 0, sizeof(*entry));
        entry->used = 1;
        entry->host_fd = host_fd;
        entry->owns_host_fd = owns_host_fd;
        entry->filetype = filetype;
        entry->rights_base = rights_base;
        entry->rights_inheriting = rights_inheriting;
        entry->is_preopen = preopen_path ? 1 : 0;
        entry->can_path = preopen_path != NULL || filetype == WASM__WASI_FILETYPE_DIRECTORY;
        entry->host_path = copied_host_path;
        entry->preopen_path = copied_preopen_path;
        if (out_fd) *out_fd = i;
        return WASM_OK;
    }

    WASM_FREE(copied_host_path);
    WASM_FREE(copied_preopen_path);
    if (owns_host_fd && host_fd >= 0) (void)wasm__wasi_host_close(host_fd);
    return WASM_ERR_OOM;
}

WASM_PRIVATE wasm_error_t wasm__wasi_state_ensure(wasm_runtime_t* rt) {
    wasm__wasi_state_t* state;

    if (!rt) return WASM_ERR_MALFORMED;
    if (rt->wasi_state) return WASM_OK;

    state = (wasm__wasi_state_t*)WASM_CALLOC(1u, sizeof(*state));
    if (!state) return WASM_ERR_OOM;

    state->fds[WASM__WASI_FD_STDIN].host_fd = -1;
    state->fds[WASM__WASI_FD_STDOUT].host_fd = -1;
    state->fds[WASM__WASI_FD_STDERR].host_fd = -1;
    wasm__wasi_fd_entry_init_stdio(&state->fds[WASM__WASI_FD_STDIN], wasm__wasi_host_fileno(stdin),
                                   WASM__WASI_FILETYPE_CHARACTER_DEVICE);
    wasm__wasi_fd_entry_init_stdio(&state->fds[WASM__WASI_FD_STDOUT], wasm__wasi_host_fileno(stdout),
                                   WASM__WASI_FILETYPE_CHARACTER_DEVICE);
    wasm__wasi_fd_entry_init_stdio(&state->fds[WASM__WASI_FD_STDERR], wasm__wasi_host_fileno(stderr),
                                   WASM__WASI_FILETYPE_CHARACTER_DEVICE);

    /* No host directory is preopened implicitly. Embedders get stdio and the
     * non-filesystem preview1 helpers, but guest path operations have no
     * directory capability and therefore return BADF. */
    rt->wasi_state = state;
    return WASM_OK;
}

WASM_PRIVATE wasm__wasi_fd_entry_t* wasm__wasi_fd_entry(wasm_runtime_t* rt, uint32_t fd) {
    if (!rt || !rt->wasi_state || fd >= WASM__WASI_MAX_FDS) return NULL;
    if (!rt->wasi_state->fds[fd].used) return NULL;
    return &rt->wasi_state->fds[fd];
}

static int wasm__wasi_read_guest_bytes(wasm_memory_t* memory,
                                       uint64_t mem_size,
                                       uint32_t ptr,
                                       uint32_t len,
                                       uint8_t** out_bytes) {
    uint8_t* copy;

    if (!memory || !out_bytes) return 0;
    *out_bytes = NULL;
    if (!wasm__range_in_bounds_u64((uint64_t)ptr, (uint64_t)len, mem_size)) return 0;
    copy = (uint8_t*)WASM_MALLOC((size_t)len + 1u);
    if (!copy) return 0;
    if (len > 0u) memcpy(copy, memory->data + ptr, len);
    copy[len] = 0u;
    *out_bytes = copy;
    return 1;
}

WASM_PRIVATE int wasm__wasi_read_guest_string(wasm_memory_t* memory,
                                        uint64_t mem_size,
                                        uint32_t ptr,
                                        uint32_t len,
                                        char** out_text) {
    uint8_t* bytes = NULL;

    if (!out_text) return 0;
    *out_text = NULL;
    if (!wasm__wasi_read_guest_bytes(memory, mem_size, ptr, len, &bytes)) return 0;
    *out_text = (char*)bytes;
    return 1;
}

static int wasm__wasi_path_is_separator(char c) {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

WASM_PRIVATE int wasm__wasi_path_walk(const char* path, uint32_t depth, uint32_t* out_depth) {
    const char* p = path;

    if (!path) return 0;
    if (wasm__wasi_path_is_separator(path[0])) return 0;
    while (*p) {
        const char* start = p;
        size_t len;

        while (*p && !wasm__wasi_path_is_separator(*p)) {
#if defined(_WIN32)
            if (*p == ':') return 0;
#endif
            p++;
        }
        len = (size_t)(p - start);
        if (len == 2u && start[0] == '.' && start[1] == '.') {
            if (depth == 0u) return 0;
            depth--;
        } else if (len > 0u && !(len == 1u && start[0] == '.')) {
            depth++;
        }
        while (wasm__wasi_path_is_separator(*p)) p++;
    }
    if (out_depth) *out_depth = depth;
    return 1;
}

WASM_PRIVATE uint32_t wasm__wasi_read_guest_path(wasm_memory_t* memory,
                                           uint64_t mem_size,
                                           uint32_t ptr,
                                           uint32_t len,
                                           char** out_path) {
    char* path = NULL;

    if (!out_path) return WASM__WASI_ERRNO_FAULT;
    *out_path = NULL;
    if (!wasm__wasi_read_guest_string(memory, mem_size, ptr, len, &path)) return WASM__WASI_ERRNO_FAULT;
    if (strlen(path) != (size_t)len || !wasm__wasi_path_walk(path, 0u, NULL)) {
        WASM_FREE(path);
        return WASM__WASI_ERRNO_NOTCAPABLE;
    }
    *out_path = path;
    return WASM__WASI_ERRNO_SUCCESS;
}

WASM_PRIVATE const char* wasm__wasi_entry_host_path(const wasm__wasi_fd_entry_t* entry) {
    if (!entry) return NULL;
    if (entry->host_path) return entry->host_path;
    return entry->preopen_path;
}

WASM_PRIVATE int wasm__wasi_entry_has_host(const wasm__wasi_fd_entry_t* entry) {
    return entry && (entry->host_fd >= 0 || wasm__wasi_entry_host_path(entry) != NULL);
}

WASM_PRIVATE int wasm__wasi_entry_can_path(const wasm__wasi_fd_entry_t* entry) {
    return entry && entry->can_path && wasm__wasi_entry_has_host(entry);
}

WASM_PRIVATE int wasm__wasi_host_read(int fd, void* buf, size_t len, size_t* out_count) {
#if defined(_WIN32)
    int rc = _read(fd, buf, (unsigned int)len);

    if (rc < 0) return 0;
    if (out_count) *out_count = (size_t)rc;
    return 1;
#else
    ssize_t rc = read(fd, buf, len);

    if (rc < 0) return 0;
    if (out_count) *out_count = (size_t)rc;
    return 1;
#endif
}

WASM_PRIVATE int wasm__wasi_host_write(int fd, const void* buf, size_t len, size_t* out_count) {
#if defined(_WIN32)
    int rc = _write(fd, buf, (unsigned int)len);

    if (rc < 0) return 0;
    if (out_count) *out_count = (size_t)rc;
    return 1;
#else
    ssize_t rc = write(fd, buf, len);

    if (rc < 0) return 0;
    if (out_count) *out_count = (size_t)rc;
    return 1;
#endif
}

WASM_PRIVATE int wasm__wasi_host_seek(int fd, int64_t offset, int whence, uint64_t* out_offset) {
#if defined(_WIN32)
    __int64 rc = _lseeki64(fd, (__int64)offset, whence);

    if (rc < 0) return 0;
    if (out_offset) *out_offset = (uint64_t)rc;
    return 1;
#else
    off_t rc = lseek(fd, (off_t)offset, whence);

    if (rc == (off_t)-1) return 0;
    if (out_offset) *out_offset = (uint64_t)rc;
    return 1;
#endif
}

WASM_PRIVATE int wasm__wasi_host_sync(int fd) {
#if defined(_WIN32)
    return _commit(fd) == 0;
#else
    return fsync(fd) == 0;
#endif
}

WASM_PRIVATE int wasm__wasi_host_datasync(int fd) {
#if defined(_WIN32)
    return _commit(fd) == 0;
#elif defined(__APPLE__)
    return fsync(fd) == 0;
#else
    return fdatasync(fd) == 0;
#endif
}

WASM_PRIVATE int wasm__wasi_host_fstat(int fd, wasm__wasi_host_stat_t* st) {
#if defined(_WIN32)
    return _fstat64(fd, st) == 0;
#else
    return fstat(fd, st) == 0;
#endif
}

WASM_PRIVATE int wasm__wasi_host_ftruncate(int fd, uint64_t size) {
#if defined(_WIN32)
    return _chsize_s(fd, size) == 0;
#else
    return ftruncate(fd, (off_t)size) == 0;
#endif
}

WASM_PRIVATE int wasm__wasi_host_pread(int fd, void* buf, size_t len, uint64_t offset, size_t* out_count) {
#if defined(_WIN32)
    uint64_t saved = 0u;
    size_t count = 0u;

    if (!wasm__wasi_host_seek(fd, 0, SEEK_CUR, &saved)) return 0;
    if (!wasm__wasi_host_seek(fd, (int64_t)offset, SEEK_SET, NULL)) return 0;
    if (!wasm__wasi_host_read(fd, buf, len, &count)) {
        (void)wasm__wasi_host_seek(fd, (int64_t)saved, SEEK_SET, NULL);
        return 0;
    }
    (void)wasm__wasi_host_seek(fd, (int64_t)saved, SEEK_SET, NULL);
    if (out_count) *out_count = count;
    return 1;
#else
    ssize_t rc = pread(fd, buf, len, (off_t)offset);

    if (rc < 0) return 0;
    if (out_count) *out_count = (size_t)rc;
    return 1;
#endif
}

WASM_PRIVATE int wasm__wasi_host_pwrite(int fd, const void* buf, size_t len, uint64_t offset, size_t* out_count) {
#if defined(_WIN32)
    uint64_t saved = 0u;
    size_t count = 0u;

    if (!wasm__wasi_host_seek(fd, 0, SEEK_CUR, &saved)) return 0;
    if (!wasm__wasi_host_seek(fd, (int64_t)offset, SEEK_SET, NULL)) return 0;
    if (!wasm__wasi_host_write(fd, buf, len, &count)) {
        (void)wasm__wasi_host_seek(fd, (int64_t)saved, SEEK_SET, NULL);
        return 0;
    }
    (void)wasm__wasi_host_seek(fd, (int64_t)saved, SEEK_SET, NULL);
    if (out_count) *out_count = count;
    return 1;
#else
    ssize_t rc = pwrite(fd, buf, len, (off_t)offset);

    if (rc < 0) return 0;
    if (out_count) *out_count = (size_t)rc;
    return 1;
#endif
}

#endif
#if WASM_ENABLE_PLATFORM
#if defined(_WIN32)
WASM_PRIVATE int wasm__wasi_windows_utf8_to_wide(const char* text, wchar_t** out_wide) {
    int count;
    wchar_t* wide;

    if (!text || !out_wide) return 0;
    *out_wide = NULL;
    count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (count <= 0) return 0;
    wide = (wchar_t*)wasm__malloc_array((size_t)count, sizeof(wchar_t));
    if (!wide) return 0;
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, count) <= 0) {
        WASM_FREE(wide);
        return 0;
    }
    *out_wide = wide;
    return 1;
}

WASM_PRIVATE int wasm__wasi_windows_wide_to_utf8(const wchar_t* text, char** out_text) {
    int count;
    char* utf8;

    if (!text || !out_text) return 0;
    *out_text = NULL;
    count = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (count <= 0) return 0;
    utf8 = (char*)WASM_MALLOC((size_t)count);
    if (!utf8) return 0;
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, count, NULL, NULL) <= 0) {
        WASM_FREE(utf8);
        return 0;
    }
    *out_text = utf8;
    return 1;
}

static void wasm__wasi_windows_normalize_slashes(char* text) {
    char* p;

    if (!text) return;
    for (p = text; *p; ++p) {
        if (*p == '/') *p = '\\';
    }
}

WASM_PRIVATE int wasm__wasi_windows_path_is_absolute(const char* path) {
    if (!path || !path[0]) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
}

WASM_PRIVATE int wasm__wasi_windows_join_path(const char* base, const char* path, char** out_path) {
    size_t base_len;
    size_t path_len;
    char* joined;

    if (!path || !out_path) return 0;
    *out_path = NULL;
    if (wasm__wasi_windows_path_is_absolute(path)) return 0;
    if (!base || !base[0]) return (*out_path = wasm__wasi_strdup(path)) != NULL;

    base_len = strlen(base);
    path_len = strlen(path);
    joined = (char*)WASM_MALLOC(base_len + 1u + path_len + 1u);
    if (!joined) return 0;
    memcpy(joined, base, base_len);
    if (base_len > 0u && joined[base_len - 1u] != '\\' && joined[base_len - 1u] != '/') joined[base_len++] = '\\';
    memcpy(joined + base_len, path, path_len + 1u);
    wasm__wasi_windows_normalize_slashes(joined);
    *out_path = joined;
    return 1;
}

WASM_PRIVATE int wasm__wasi_windows_stat_path(const char* path, wasm__wasi_host_stat_t* st) {
    wchar_t* wide = NULL;
    int ok;

    if (!path || !st) return 0;
    if (!wasm__wasi_windows_utf8_to_wide(path, &wide)) return 0;
    ok = _wstat64(wide, st) == 0;
    WASM_FREE(wide);
    return ok;
}

WASM_PRIVATE int wasm__wasi_windows_open_path(const char* path, int oflags) {
    wchar_t* wide = NULL;
    int fd;

    if (!path) return -1;
    if (!wasm__wasi_windows_utf8_to_wide(path, &wide)) {
        errno = EINVAL;
        return -1;
    }
    fd = _wopen(wide, oflags, _S_IREAD | _S_IWRITE);
    WASM_FREE(wide);
    return fd;
}

WASM_PRIVATE uint32_t wasm__wasi_windows_error_to_wasi(DWORD error_code) {
    switch (error_code) {
        case ERROR_SUCCESS:
            return WASM__WASI_ERRNO_SUCCESS;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
            return WASM__WASI_ERRNO_NOENT;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return WASM__WASI_ERRNO_ACCES;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            return WASM__WASI_ERRNO_EXIST;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return WASM__WASI_ERRNO_NOMEM;
        case ERROR_DIRECTORY:
            return WASM__WASI_ERRNO_NOTDIR;
        case ERROR_DIR_NOT_EMPTY:
            return WASM__WASI_ERRNO_NOTEMPTY;
        case ERROR_INVALID_NAME:
        case ERROR_INVALID_PARAMETER:
            return WASM__WASI_ERRNO_INVAL;
        case ERROR_PRIVILEGE_NOT_HELD:
            return WASM__WASI_ERRNO_PERM;
        case ERROR_NOT_SUPPORTED:
        case ERROR_CALL_NOT_IMPLEMENTED:
            return WASM__WASI_ERRNO_NOSYS;
        default:
            return WASM__WASI_ERRNO_IO;
    }
}

WASM_PRIVATE int wasm__wasi_windows_resolve_path(const wasm__wasi_fd_entry_t* entry,
                                           const char* path,
                                           char** out_path) {
    const char* base_path = wasm__wasi_entry_host_path(entry);

    if (!entry || !path || !out_path || !base_path) return 0;
    return wasm__wasi_windows_join_path(base_path, path, out_path);
}

WASM_PRIVATE uint8_t wasm__wasi_windows_filetype_from_attributes(DWORD attributes) {
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) return WASM__WASI_FILETYPE_SYMBOLIC_LINK;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) return WASM__WASI_FILETYPE_DIRECTORY;
    return WASM__WASI_FILETYPE_REGULAR_FILE;
}

WASM_PRIVATE int wasm__wasi_windows_readlink_path(const char* path, char** out_target) {
    wchar_t* wide_path = NULL;
    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD wide_len;
    wchar_t* wide_target = NULL;
    char* utf8_target = NULL;
    int ok = 0;

    if (!path || !out_target) return 0;
    *out_target = NULL;
    if (!wasm__wasi_windows_utf8_to_wide(path, &wide_path)) return 0;
    handle = CreateFileW(wide_path, 0,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                         NULL);
    if (handle == INVALID_HANDLE_VALUE) goto cleanup;

    wide_len = GetFinalPathNameByHandleW(handle, NULL, 0u, FILE_NAME_NORMALIZED);
    if (wide_len == 0u) goto cleanup;
    wide_target = (wchar_t*)WASM_MALLOC(((size_t)wide_len + 1u) * sizeof(wchar_t));
    if (!wide_target) goto cleanup;
    if (GetFinalPathNameByHandleW(handle, wide_target, wide_len + 1u, FILE_NAME_NORMALIZED) == 0u) goto cleanup;
    if (!wasm__wasi_windows_wide_to_utf8(wide_target, &utf8_target)) goto cleanup;
    if (strncmp(utf8_target, "\\\\?\\UNC\\", 8u) == 0)
        memmove(utf8_target + 1u, utf8_target + 7u, strlen(utf8_target + 7u) + 1u);
    else if (strncmp(utf8_target, "\\\\?\\", 4u) == 0)
        memmove(utf8_target, utf8_target + 4u, strlen(utf8_target + 4u) + 1u);
    *out_target = utf8_target;
    utf8_target = NULL;
    ok = 1;

cleanup:
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    WASM_FREE(wide_path);
    WASM_FREE(wide_target);
    WASM_FREE(utf8_target);
    return ok;
}

#endif
#endif
#if WASM_ENABLE_PLATFORM
static uint64_t wasm__wasi_stat_time_ns(const wasm__wasi_host_stat_t* st, int which) {
    if (!st) return 0u;
    if (which == 0) return (uint64_t)st->st_atime * 1000000000ull;
    if (which == 1) return (uint64_t)st->st_mtime * 1000000000ull;
    return (uint64_t)st->st_ctime * 1000000000ull;
}

WASM_PRIVATE void wasm__wasi_store_filestat(uint8_t* dst, const wasm__wasi_host_stat_t* st) {
    memset(dst, 0, 64u);
    if (!dst || !st) return;
    wasm__store_le64(dst + 0u, (uint64_t)st->st_dev);
    wasm__store_le64(dst + 8u, (uint64_t)st->st_ino);
    dst[16u] = wasm__wasi_filetype_from_mode(st->st_mode);
    wasm__store_le64(dst + 24u, (uint64_t)st->st_nlink);
    wasm__store_le64(dst + 32u, (uint64_t)st->st_size);
    wasm__store_le64(dst + 40u, wasm__wasi_stat_time_ns(st, 0));
    wasm__store_le64(dst + 48u, wasm__wasi_stat_time_ns(st, 1));
    wasm__store_le64(dst + 56u, wasm__wasi_stat_time_ns(st, 2));
}

WASM_PRIVATE int wasm__wasi_entry_stat(const wasm__wasi_fd_entry_t* entry, wasm__wasi_host_stat_t* st) {
    if (!entry || !st) return 0;
    if (entry->host_fd >= 0) return wasm__wasi_host_fstat(entry->host_fd, st);
#if defined(_WIN32)
    {
        const char* host_path = wasm__wasi_entry_host_path(entry);

        if (!host_path) return 0;
        return wasm__wasi_windows_stat_path(host_path, st);
    }
#else
    return 0;
#endif
}

WASM_PRIVATE int wasm__wasi_signal_to_host(uint32_t sig) {
    switch (sig) {
        case 0u:
            return 0;
#ifdef SIGHUP
        case 1u:
            return SIGHUP;
#endif
#ifdef SIGINT
        case 2u:
            return SIGINT;
#endif
#ifdef SIGQUIT
        case 3u:
            return SIGQUIT;
#endif
#ifdef SIGILL
        case 4u:
            return SIGILL;
#endif
#ifdef SIGTRAP
        case 5u:
            return SIGTRAP;
#endif
#ifdef SIGABRT
        case 6u:
            return SIGABRT;
#endif
#ifdef SIGBUS
        case 7u:
            return SIGBUS;
#endif
#ifdef SIGFPE
        case 8u:
            return SIGFPE;
#endif
#ifdef SIGKILL
        case 9u:
            return SIGKILL;
#endif
#ifdef SIGUSR1
        case 10u:
            return SIGUSR1;
#endif
#ifdef SIGSEGV
        case 11u:
            return SIGSEGV;
#endif
#ifdef SIGUSR2
        case 12u:
            return SIGUSR2;
#endif
#ifdef SIGPIPE
        case 13u:
            return SIGPIPE;
#endif
#ifdef SIGALRM
        case 14u:
            return SIGALRM;
#endif
#ifdef SIGTERM
        case 15u:
            return SIGTERM;
#endif
        default:
            return -1;
    }
}

#endif
