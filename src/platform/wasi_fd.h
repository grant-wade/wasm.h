#ifndef WASM_PLATFORM_WASI_FD_H
#define WASM_PLATFORM_WASI_FD_H

#include "../internal/config.h"

#if WASM_ENABLE_PLATFORM
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

WASM_PRIVATE wasm_error_t wasm__wasi_sched_yield(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE wasm_error_t wasm__wasi_errno_stub(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata);
#endif

#endif /* WASM_PLATFORM_WASI_FD_H */
