#ifndef WASM_INTERNAL_PLATFORM_H
#define WASM_INTERNAL_PLATFORM_H

#include "common.h"

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

WASM_PRIVATE int wasm__wasi_host_fileno(FILE* stream);

WASM_PRIVATE uint8_t wasm__wasi_filetype_from_mode(int mode);

WASM_PRIVATE void wasm__wasi_fd_entry_reset(wasm__wasi_fd_entry_t* entry);

WASM_PRIVATE void wasm__wasi_state_destroy(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__wasi_fd_entry_init_stdio(wasm__wasi_fd_entry_t* entry,
                                           int host_fd,
                                           uint8_t filetype);

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

WASM_PRIVATE int wasm__wasi_read_guest_bytes(wasm_memory_t* memory,
                                       uint64_t mem_size,
                                       uint32_t ptr,
                                       uint32_t len,
                                       uint8_t** out_bytes);

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

WASM_PRIVATE void wasm__wasi_windows_normalize_slashes(char* text);

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

WASM_PRIVATE uint64_t wasm__wasi_stat_time_ns(const wasm__wasi_host_stat_t* st, int which);

WASM_PRIVATE void wasm__wasi_store_filestat(uint8_t* dst, const wasm__wasi_host_stat_t* st);

WASM_PRIVATE int wasm__wasi_entry_stat(const wasm__wasi_fd_entry_t* entry, wasm__wasi_host_stat_t* st);

WASM_PRIVATE int wasm__wasi_signal_to_host(uint32_t sig);

typedef uint32_t (*wasm__wasi_string_count_fn)(void);
typedef const char* (*wasm__wasi_string_at_fn)(uint32_t index);

WASM_PRIVATE void wasm__wasi_set_errno(wasm_value_t* results, uint32_t num_results, uint32_t errno_code);

WASM_PRIVATE uint32_t wasm__wasi_errno_from_host(int err_code);

WASM_PRIVATE wasm_error_t wasm__wasi_get_memory0(wasm_module_t* mod,
                                           const char* op_name,
                                           wasm_memory_t** out_memory,
                                           uint64_t* out_mem_size);

WASM_PRIVATE uint64_t wasm__wasi_string_list_size(wasm__wasi_string_count_fn count_fn,
                                            wasm__wasi_string_at_fn at_fn);

WASM_PRIVATE uint32_t wasm__wasi_string_list_count_args(void);

WASM_PRIVATE const char* wasm__wasi_string_list_at_args(uint32_t index);

WASM_PRIVATE uint32_t wasm__wasi_string_list_count_env(void);

WASM_PRIVATE const char* wasm__wasi_string_list_at_env(uint32_t index);

WASM_PRIVATE uint32_t wasm__wasi_string_sizes_get(wasm_module_t* mod,
                                            wasm__wasi_string_count_fn count_fn,
                                            wasm__wasi_string_at_fn at_fn,
                                            uint32_t count_ptr,
                                            uint32_t size_ptr,
                                            wasm_value_t* results,
                                            uint32_t num_results);

WASM_PRIVATE uint32_t wasm__wasi_string_list_get(wasm_module_t* mod,
                                           wasm__wasi_string_count_fn count_fn,
                                           wasm__wasi_string_at_fn at_fn,
                                           uint32_t entries_ptr,
                                           uint32_t data_ptr,
                                           wasm_value_t* results,
                                           uint32_t num_results);

WASM_PRIVATE int wasm__wasi_is_stdio_fd(uint32_t fd);

WASM_PRIVATE wasm_error_t wasm__wasi_args_sizes_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_args_get(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_environ_sizes_get(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_write(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_read(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_proc_exit(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_close(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_seek(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_sync(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_environ_get(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_fdstat_get(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_random_get(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_clock_time_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_clock_res_get(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_datasync(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_fdstat_set_flags(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_fdstat_set_rights(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_filestat_get(wasm_module_t* mod,
                                               const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results,
                                               void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_filestat_set_size(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_tell(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_advise(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_allocate(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_proc_raise(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_sched_yield(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_errno_stub(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata);

WASM_PRIVATE int wasm__wasi_dirflags_to_host(uint32_t flags);

WASM_PRIVATE int wasm__wasi_oflags_to_host(uint32_t oflags, uint32_t fdflags);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_prestat_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_prestat_dir_name(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_create_directory(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_filestat_get(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_link(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_open(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_readlink(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_remove_directory(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_rename(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_symlink(wasm_module_t* mod,
                                            const wasm_value_t* args, uint32_t num_args,
                                            wasm_value_t* results, uint32_t num_results,
                                            void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_path_unlink_file(wasm_module_t* mod,
                                                const wasm_value_t* args, uint32_t num_args,
                                                wasm_value_t* results, uint32_t num_results,
                                                void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_renumber(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_pread(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_pwrite(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_fd_readdir(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata);

wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt);

#define WASM__EMSCRIPTEN_MODULE_NAME "env"
#define WASM__EMSCRIPTEN_MEMORY_NAME "memory"

#ifndef ENOSYS
#define ENOSYS 38
#endif

#ifndef ENOTTY
#define ENOTTY 25
#endif

WASM_PRIVATE int32_t wasm__emscripten_errno_from_host(int err_code);

WASM_PRIVATE int32_t wasm__emscripten_neg_errno(int err_code);

WASM_PRIVATE wasm_error_t wasm__emscripten_return_i32(wasm_value_t* results,
                                                uint32_t num_results,
                                                int32_t value);

WASM_PRIVATE int wasm__emscripten_read_guest_string(wasm_memory_t* memory,
                                              uint64_t mem_size,
                                              uint32_t ptr,
                                              char** out_string);

WASM_PRIVATE int wasm__emscripten_store_i32(wasm_memory_t* memory,
                                      uint64_t mem_size,
                                      uint32_t ptr,
                                      int32_t value);

WASM_PRIVATE int wasm__emscripten_store_c_string(wasm_memory_t* memory,
                                           uint64_t mem_size,
                                           uint32_t ptr,
                                           const char* text);

WASM_PRIVATE wasm_error_t wasm__ensure_emscripten_memory_import(wasm_runtime_t* rt,
                                                          uint64_t min_pages,
                                                          uint64_t max_pages,
                                                          int is_64);

WASM_PRIVATE wasm_error_t wasm__emscripten_date_now(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_get_now(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_get_heap_max(wasm_module_t* mod,
                                                  const wasm_value_t* args, uint32_t num_args,
                                                  wasm_value_t* results, uint32_t num_results,
                                                  void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_resize_heap(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_tzset_js(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_localtime_js(wasm_module_t* mod,
                                                  const wasm_value_t* args, uint32_t num_args,
                                                  wasm_value_t* results, uint32_t num_results,
                                                  void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_mmap_js(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_munmap_js(wasm_module_t* mod,
                                               const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results,
                                               void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_faccessat(wasm_module_t* mod,
                                                       const wasm_value_t* args, uint32_t num_args,
                                                       wasm_value_t* results, uint32_t num_results,
                                                       void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_fchmod(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_chmod(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_fchown32(wasm_module_t* mod,
                                                      const wasm_value_t* args, uint32_t num_args,
                                                      wasm_value_t* results, uint32_t num_results,
                                                      void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_fcntl64(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_openat(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_ioctl(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_stat_unsupported(wasm_module_t* mod,
                                                              const wasm_value_t* args, uint32_t num_args,
                                                              wasm_value_t* results, uint32_t num_results,
                                                              void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_ftruncate64(wasm_module_t* mod,
                                                         const wasm_value_t* args, uint32_t num_args,
                                                         wasm_value_t* results, uint32_t num_results,
                                                         void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_getcwd(wasm_module_t* mod,
                                                    const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results,
                                                    void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_mkdirat(wasm_module_t* mod,
                                                     const wasm_value_t* args, uint32_t num_args,
                                                     wasm_value_t* results, uint32_t num_results,
                                                     void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_readlinkat(wasm_module_t* mod,
                                                        const wasm_value_t* args, uint32_t num_args,
                                                        wasm_value_t* results, uint32_t num_results,
                                                        void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_rmdir(wasm_module_t* mod,
                                                   const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results,
                                                   void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_unlinkat(wasm_module_t* mod,
                                                      const wasm_value_t* args, uint32_t num_args,
                                                      wasm_value_t* results, uint32_t num_results,
                                                      void* userdata);

WASM_PRIVATE wasm_error_t wasm__emscripten_syscall_utimensat(wasm_module_t* mod,
                                                       const wasm_value_t* args, uint32_t num_args,
                                                       wasm_value_t* results, uint32_t num_results,
                                                       void* userdata);

wasm_error_t wasm_bind_emscripten_stubs(wasm_runtime_t* rt);

#else

wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt);

wasm_error_t wasm_bind_emscripten_stubs(wasm_runtime_t* rt);

#endif /* WASM_ENABLE_PLATFORM */


#endif /* WASM_INTERNAL_PLATFORM_H */
