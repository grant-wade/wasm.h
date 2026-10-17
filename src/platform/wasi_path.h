#ifndef WASM_PLATFORM_WASI_PATH_H
#define WASM_PLATFORM_WASI_PATH_H

#include "../internal/config.h"

#if WASM_ENABLE_PLATFORM
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
#endif

#endif /* WASM_PLATFORM_WASI_PATH_H */
