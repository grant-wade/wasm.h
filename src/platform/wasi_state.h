#ifndef WASM_PLATFORM_WASI_STATE_H
#define WASM_PLATFORM_WASI_STATE_H

#include "../internal/config.h"
#include "host.h"

#if WASM_ENABLE_PLATFORM
#define WASM__WASI_MODULE_NAME "wasi_snapshot_preview1"
#define WASM__WASI_FD_STDIN 0u
#define WASM__WASI_FD_STDOUT 1u
#define WASM__WASI_FD_STDERR 2u
#define WASM__WASI_FILETYPE_CHARACTER_DEVICE 2u
#define WASM__WASI_FILETYPE_REGULAR_FILE 4u
#define WASM__WASI_WHENCE_SET 0u
#define WASM__WASI_WHENCE_CUR 1u
#define WASM__WASI_WHENCE_END 2u
#define WASM__WASI_CLOCKID_REALTIME 0u
#define WASM__WASI_CLOCKID_MONOTONIC 1u
#define WASM__WASI_CLOCKID_PROCESS_CPUTIME_ID 2u
#define WASM__WASI_CLOCKID_THREAD_CPUTIME_ID 3u
#define WASM__WASI_ERRNO_SUCCESS 0u
#define WASM__WASI_ERRNO_ACCES 2u
#define WASM__WASI_ERRNO_AGAIN 6u
#define WASM__WASI_ERRNO_BADF 8u
#define WASM__WASI_ERRNO_EXIST 20u
#define WASM__WASI_ERRNO_INVAL 28u
#define WASM__WASI_ERRNO_IO 29u
#define WASM__WASI_ERRNO_FAULT 21u
#define WASM__WASI_ERRNO_NOENT 44u
#define WASM__WASI_ERRNO_NOMEM 48u
#define WASM__WASI_ERRNO_NOSPC 51u
#define WASM__WASI_ERRNO_NOSYS 52u
#define WASM__WASI_ERRNO_NOTDIR 54u
#define WASM__WASI_ERRNO_NOTEMPTY 55u
#define WASM__WASI_ERRNO_NOTSUP 58u
#define WASM__WASI_ERRNO_ISDIR 31u
#define WASM__WASI_ERRNO_PERM 63u
#define WASM__WASI_ERRNO_PIPE 64u
#define WASM__WASI_ERRNO_SPIPE 70u
#define WASM__WASI_ERRNO_NOTCAPABLE 76u
#define WASM__WASI_FILETYPE_UNKNOWN 0u
#define WASM__WASI_FILETYPE_BLOCK_DEVICE 1u
#define WASM__WASI_FILETYPE_DIRECTORY 3u
#define WASM__WASI_FILETYPE_SOCKET_DGRAM 5u
#define WASM__WASI_FILETYPE_SOCKET_STREAM 6u
#define WASM__WASI_FILETYPE_SYMBOLIC_LINK 7u
#define WASM__WASI_PREOPENTYPE_DIR 0u
#define WASM__WASI_RIGHTS_ALL UINT64_MAX
#define WASM__WASI_MAX_FDS 64u

typedef struct wasm__wasi_fd_entry_t {
    int used;
    int host_fd;
    int owns_host_fd;
    int is_preopen;
    uint8_t filetype;
    uint16_t fdflags;
    uint64_t rights_base;
    uint64_t rights_inheriting;
    char* host_path;
    char* preopen_path;
} wasm__wasi_fd_entry_t;

struct wasm__wasi_state_t {
    wasm__wasi_fd_entry_t fds[WASM__WASI_MAX_FDS];
};

#if defined(_WIN32)
typedef struct __stat64 wasm__wasi_host_stat_t;
#else
typedef struct stat wasm__wasi_host_stat_t;
#endif

WASM_PRIVATE char* wasm__wasi_strdup(const char* text);

WASM_PRIVATE int wasm__wasi_host_close(int fd);

WASM_PRIVATE uint8_t wasm__wasi_filetype_from_mode(int mode);

WASM_PRIVATE void wasm__wasi_fd_entry_reset(wasm__wasi_fd_entry_t* entry);

WASM_PRIVATE void wasm__wasi_state_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__wasi_state_add_fd(wasm_runtime_t* rt,
                                            int host_fd,
                                            int owns_host_fd,
                                            uint8_t filetype,
                                            uint64_t rights_base,
                                            uint64_t rights_inheriting,
                                            const char* host_path,
                                            const char* preopen_path,
                                            uint32_t* out_fd);

WASM_PRIVATE wasm_error_t wasm__wasi_state_ensure(wasm_runtime_t* rt);

WASM_PRIVATE wasm__wasi_fd_entry_t* wasm__wasi_fd_entry(wasm_runtime_t* rt, uint32_t fd);

WASM_PRIVATE int wasm__wasi_read_guest_path(wasm_memory_t* memory,
                                      uint64_t mem_size,
                                      uint32_t ptr,
                                      uint32_t len,
                                      char** out_path);

WASM_PRIVATE const char* wasm__wasi_entry_host_path(const wasm__wasi_fd_entry_t* entry);

WASM_PRIVATE int wasm__wasi_entry_can_path(const wasm__wasi_fd_entry_t* entry);

WASM_PRIVATE int wasm__wasi_host_read(int fd, void* buf, size_t len, size_t* out_count);

WASM_PRIVATE int wasm__wasi_host_write(int fd, const void* buf, size_t len, size_t* out_count);

WASM_PRIVATE int wasm__wasi_host_seek(int fd, int64_t offset, int whence, uint64_t* out_offset);

WASM_PRIVATE int wasm__wasi_host_sync(int fd);

WASM_PRIVATE int wasm__wasi_host_datasync(int fd);

WASM_PRIVATE int wasm__wasi_host_fstat(int fd, wasm__wasi_host_stat_t* st);

WASM_PRIVATE int wasm__wasi_host_ftruncate(int fd, uint64_t size);

WASM_PRIVATE int wasm__wasi_host_pread(int fd, void* buf, size_t len, uint64_t offset, size_t* out_count);

WASM_PRIVATE int wasm__wasi_host_pwrite(int fd, const void* buf, size_t len, uint64_t offset, size_t* out_count);

#if defined(_WIN32)
WASM_PRIVATE int wasm__wasi_windows_utf8_to_wide(const char* text, wchar_t** out_wide);

WASM_PRIVATE int wasm__wasi_windows_wide_to_utf8(const wchar_t* text, char** out_text);

WASM_PRIVATE int wasm__wasi_windows_path_is_absolute(const char* path);

WASM_PRIVATE int wasm__wasi_windows_join_path(const char* base, const char* path, char** out_path);

WASM_PRIVATE int wasm__wasi_windows_stat_path(const char* path, wasm__wasi_host_stat_t* st);

WASM_PRIVATE int wasm__wasi_windows_open_path(const char* path, int oflags);

WASM_PRIVATE uint32_t wasm__wasi_windows_error_to_wasi(DWORD error_code);

WASM_PRIVATE int wasm__wasi_windows_resolve_path(const wasm__wasi_fd_entry_t* entry,
                                           const char* path,
                                           char** out_path);

WASM_PRIVATE uint8_t wasm__wasi_windows_filetype_from_attributes(DWORD attributes);

WASM_PRIVATE int wasm__wasi_windows_readlink_path(const char* path, char** out_target);

#endif

WASM_PRIVATE void wasm__wasi_store_filestat(uint8_t* dst, const wasm__wasi_host_stat_t* st);

WASM_PRIVATE int wasm__wasi_entry_stat(const wasm__wasi_fd_entry_t* entry, wasm__wasi_host_stat_t* st);

WASM_PRIVATE int wasm__wasi_signal_to_host(uint32_t sig);
#endif

#endif /* WASM_PLATFORM_WASI_STATE_H */
