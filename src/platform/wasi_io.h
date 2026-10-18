#ifndef WASM_PLATFORM_WASI_IO_H
#define WASM_PLATFORM_WASI_IO_H

#include "../internal/config.h"

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE void wasm__wasi_set_errno(wasm_value_t* results, uint32_t num_results, uint32_t errno_code);

WASM_PRIVATE uint32_t wasm__wasi_errno_from_host(int err_code);

WASM_PRIVATE wasm_error_t wasm__wasi_get_memory0(wasm_module_t* mod,
                                           const char* op_name,
                                           wasm_memory_t** out_memory,
                                           uint64_t* out_mem_size);

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

/* Clock subscriptions sleep until the earliest deadline; fd subscriptions report ready at once. */
WASM_PRIVATE wasm_error_t wasm__wasi_poll_oneoff(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_clock_res_get(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata);
#endif

#endif /* WASM_PLATFORM_WASI_IO_H */
