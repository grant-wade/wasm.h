/* GENERATED FILE - DO NOT EDIT.
   Amalgamated from src/ by tools/amalgamate.c; edit those files and regenerate. */
/*
------------------------------------------------------------------------------
cbuild.h - Minimal, cross-platform, header-only C build system

Copyright (c) 2025-2026 Grant Wade

1. Purpose
----------
A minimal, cross-platform, header-only build system for C projects.
Write your build logic directly in C, enabling full language power
and portability without external tools or scripts.

2. Key Features
---------------
- Targets: executables, static/shared libraries, file-dependency and dummy targets, custom commands
- Build graph and dependencies with incremental rebuilds
  - Uses timestamps plus compile/link signature files to detect flag/env changes
  - GCC/Clang header deps via .d files; MSVC header deps via /showIncludes parsing
- Parallel compilation across CPU cores (-j/--jobs)
- Sources/includes/lib-dirs and libraries; supports wildcards including ** recursive
- Command API: pre/post target commands, argv-based execution, shell command lines, and C callbacks; command dependencies
- Subcommands: register named actions and run them with --run
- Tests: register test programs with labels, skip codes, timeouts, and locks; run them in parallel with --test
- Subprojects: query targets via --manifest, build them on demand, and link as dependencies
- compile_commands.json generation with full arguments array (for clangd, etc.)
- Build configurations: Debug/Release presets, per-target configs, structured knobs (optimization, LTO, sanitizers, etc.)
- Toolchain detection and portability: GCC/Clang/MSVC, platform-specific outputs and SONAME/install_name
- Self-rebuild support to recompile the build executable when its sources change
- Inspection and maintenance: --list, --graph, --deps, --clean
- Embeddable: context-based API with no global state, suitable for library use

3. Basic Usage
--------------
    #define CBUILD_IMPLEMENTATION

    int main(int argc, char** argv) {
        cbuild_context_t* ctx = cbuild_context_new();

        cbuild_set_output_dir(ctx, "build");

        target_t* lib = cbuild_static_library(ctx, "foo");
        cbuild_add_source(ctx, lib, "foo.c");

        target_t* exe = cbuild_executable(ctx, "bar");
        cbuild_add_source(ctx, exe, "bar.c");
        cbuild_add_link_target(ctx, exe, lib);

        int result = cbuild_run(ctx, argc, argv);
        cbuild_context_free(ctx);
        return result;
    }

Typical invocations:
    $ cc build.c -o cbuild            # or 'cl' on Windows
    $ ./cbuild                        # builds all targets
    $ ./cbuild --clean                # cleans build outputs
    $ ./cbuild --version              # prints CBUILD_VERSION
    $ ./cbuild --run bar              # builds bar's target (if any) and runs the subcommand
    $ ./cbuild bar                    # shorthand for --run bar
    $ ./cbuild --list                 # lists targets
    $ ./cbuild --graph                # prints the build graph
    $ ./cbuild --deps=mytarget        # shows reverse dependencies of 'mytarget'
    $ ./cbuild -j 8                   # builds with 8 parallel jobs
    $ ./cbuild --target=mytarget      # builds only the specified target
    $ ./cbuild --compile-commands     # writes compile_commands.json to the output dir
    $ ./cbuild --test                 # builds what the tests need, then runs them

4. Build Configurations
-----------------------
cbuild supports structured build configurations that abstract compiler-specific flags:

    cbuild_context_t* ctx = cbuild_context_new();

    // Use a preset configuration
    config_t* debug = cbuild_config_default_debug(ctx);
    config_t* release = cbuild_config_default_release(ctx);

    // Or create a custom configuration
    config_t* custom = cbuild_config_new(ctx, "Custom");
    cbuild_config_set_opt(ctx, custom, 2);           // -O2 or /O2
    cbuild_config_set_debug(ctx, custom, 1);         // -g or /Zi
    cbuild_config_set_std(ctx, custom, "c11");       // -std=c11 or /std:c11
    cbuild_config_set_warnings(ctx, custom, 2);      // -Wall -Wextra -Wpedantic or /W4
    cbuild_config_set_lto(ctx, custom, 1);           // -flto or /GL + /LTCG
    cbuild_config_enable_sanitizers(ctx, custom, 1); // -fsanitize=address
    cbuild_config_set_output_dir(ctx, custom, "build/custom");

    // Set active configuration (applies to all targets by default)
    cbuild_set_active_config(ctx, release);

    // Or override per-target
    cbuild_target_set_config(ctx, my_debug_tool, debug);

5. Programmatic / Library Use
-----------------------------
For embedding cbuild or using it programmatically without CLI parsing:

    cbuild_context_t* ctx = cbuild_context_new();

    // Configure the build
    cbuild_set_output_dir(ctx, "build");
    cbuild_set_compiler(ctx, "clang");
    cbuild_set_parallelism(ctx, 4);

    // Define targets...
    target_t* exe = cbuild_executable(ctx, "myapp");
    cbuild_add_source(ctx, exe, "main.c");

    // Build programmatically (without CLI)
    int result = cbuild_build(ctx, NULL);        // build all targets
    int result = cbuild_build(ctx, "myapp");     // build specific target

    // Or configure from argv without running
    cbuild_configure_from_argv(ctx, argc, argv);

    // Check errors
    const char* err = cbuild_get_last_error(ctx);
    if (err) fprintf(stderr, "Error: %s\n", err);

    // Reset for reuse (clears targets, keeps allocator)
    cbuild_reset(ctx);

    // Clean up
    cbuild_context_free(ctx);

6. Custom Logging
-----------------
Replace the default logger for integration with other systems:

    void my_logger(void* user_data, cbuild_log_level_t level, const char* msg) {
        MyApp* app = (MyApp*)user_data;
        // Route to your logging system
    }

    cbuild_set_logger(ctx, my_logger, my_app);

7. Integration Notes
--------------------
- Single header: include in your build.c and define CBUILD_IMPLEMENTATION in exactly one translation unit
- Context-based: all state lives in cbuild_context_t, no globals, safe for library embedding
- Subprojects: add with cbuild_add_subproject() and link via cbuild_subproject_get_target()
- Argv-based process execution avoids shell quoting issues on all platforms
- Wildcards supported for sources/includes/libs (including ** recursive)
- Platform headers abstract Windows, macOS, and Linux differences
- Self-rebuild: call cbuild_self_rebuild_if_needed() or use CBUILD_SELF_REBUILD macro

8. License
----------
BSD 3-Clause License

Copyright (c) 2025-2026 Grant Wade
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------
*/

#ifndef CBUILD_H
#define CBUILD_H

/* Public semantic version. Keep this in sync with release tags. */
#define CBUILD_VERSION "v0.1.2"
#define CBUILD_VERSION_MAJOR 0
#define CBUILD_VERSION_MINOR 1
#define CBUILD_VERSION_PATCH 2

#ifdef __cplusplus
extern "C" {
#endif

// *** Type Definitions *** //
typedef struct cbuild_context cbuild_context_t;
typedef struct cbuild_target target_t;
typedef struct cbuild_command command_t;
typedef struct cbuild_subproject subproject_t;
typedef struct cbuild_config config_t;
typedef struct cbuild_test test_t;

typedef void (*cbuild_subcommand_callback)(void* user_data);
typedef int (*cbuild_flag_callback)(const char* value, void* user_data);

typedef enum {
    CBUILD_FLAG_PRE = 0,          /* parse-time actions; set globals, early exits */
    CBUILD_FLAG_BEFORE_BUILD = 1, /* runs after graph is ready, before build */
    CBUILD_FLAG_AFTER_BUILD = 2   /* runs after a successful build */
} cbuild_flag_phase_t;

/* Special return code to signal "handled & exit(0)". */
#define CBUILD_FLAG_EXIT 777

// *** Logging System *** //
typedef enum {
    CBUILD_LOG_INFO = 0,
    CBUILD_LOG_WARNING,
    CBUILD_LOG_ERROR,
    CBUILD_LOG_STEP,
    CBUILD_LOG_STATUS_OK,
    CBUILD_LOG_STATUS_FAIL,
    CBUILD_LOG_VERBOSE
} cbuild_log_level_t;

typedef void (*cbuild_log_fn)(void* user_data, cbuild_log_level_t level, const char* msg);

// *** Context API *** //
cbuild_context_t* cbuild_context_new(void);
void cbuild_context_free(cbuild_context_t* ctx);
void cbuild_set_logger(cbuild_context_t* ctx, cbuild_log_fn callback, void* user_data);
const char* cbuild_get_last_error(cbuild_context_t* ctx);
void cbuild_set_user_data(cbuild_context_t* ctx, void* data);
void* cbuild_get_user_data(cbuild_context_t* ctx);

// *** Main Entrypoint *** //
int cbuild_run(cbuild_context_t* ctx, int argc, char** argv);

// *** Programmatic Build API *** //
int cbuild_build(cbuild_context_t* ctx, const char* target_name);
int cbuild_clean(cbuild_context_t* ctx);
int cbuild_configure_from_argv(cbuild_context_t* ctx, int argc, char** argv);

// *** Reset Build State *** //
void cbuild_reset(cbuild_context_t* ctx);
void cbuild_teardown(cbuild_context_t* ctx);

// *** Target creation functions *** //
target_t* cbuild_executable(cbuild_context_t* ctx, const char* name);
target_t* cbuild_static_library(cbuild_context_t* ctx, const char* name);
target_t* cbuild_shared_library(cbuild_context_t* ctx, const char* name);
target_t* cbuild_dummy_target(cbuild_context_t* ctx, const char* name);
target_t* cbuild_file_dep_target(cbuild_context_t* ctx, const char* name, const char* file_path);

// *** Target sources/includes/links/flags setup *** //
// All functions return 0 on success, -1 on failure (sets last_error)
int cbuild_add_source(cbuild_context_t* ctx, target_t* target, const char* source_file);
int cbuild_add_include_dir(cbuild_context_t* ctx, target_t* target, const char* include_path);
int cbuild_add_library_dir(cbuild_context_t* ctx, target_t* target, const char* lib_dir);
int cbuild_add_link_library(cbuild_context_t* ctx, target_t* target, const char* lib_name);
int cbuild_add_link_target(cbuild_context_t* ctx, target_t* dependant, target_t* dependency);
int cbuild_add_expose_library(cbuild_context_t* ctx, target_t* target, const char* lib_path);
int cbuild_export_symbols(cbuild_context_t* ctx, target_t* target);
int cbuild_add_cflags(cbuild_context_t* ctx, target_t* target, const char* cflags);
int cbuild_add_ldflags(cbuild_context_t* ctx, target_t* target, const char* ldflags);
int cbuild_add_flag(cbuild_context_t* ctx, target_t* target, const char* flag, int value);
int cbuild_add_define(cbuild_context_t* ctx, target_t* target, const char* macro);
int cbuild_add_define_val(cbuild_context_t* ctx, target_t* target, const char* macro, const char* value);

// *** Context Settings *** //
void cbuild_set_output_dir(cbuild_context_t* ctx, const char* dir);
void cbuild_set_parallelism(cbuild_context_t* ctx, int jobs_count);
void cbuild_set_compiler(cbuild_context_t* ctx, const char* compiler_exe);
void cbuild_set_linker(cbuild_context_t* ctx, const char* linker_exe);
void cbuild_set_archiver(cbuild_context_t* ctx, const char* archiver_exe);
void cbuild_target_set_linker(cbuild_context_t* ctx, target_t* t, const char* linker_exe);
void cbuild_set_output_file(cbuild_context_t* ctx, target_t* t, const char* path);
void cbuild_set_soname(cbuild_context_t* ctx, target_t* t, const char* soname);
void cbuild_set_build_type(cbuild_context_t* ctx, const char* build_type);
void cbuild_add_global_cflags(cbuild_context_t* ctx, const char* flags);
void cbuild_add_global_ldflags(cbuild_context_t* ctx, const char* flags);
void cbuild_add_global_define(cbuild_context_t* ctx, const char* macro);
void cbuild_add_global_define_val(cbuild_context_t* ctx, const char* macro, const char* value);
void cbuild_add_global_flag(cbuild_context_t* ctx, const char* flag, int value);
void cbuild_enable_compile_commands(cbuild_context_t* ctx, int enabled);
void cbuild_guess_compiler(cbuild_context_t* ctx);
int cbuild_self_rebuild_if_needed(cbuild_context_t* ctx, int argc, char** argv, const char** sources, int sources_count);
void cbuild_set_verbose(cbuild_context_t* ctx, int verbose);

// *** Build Configuration API *** //
config_t* cbuild_config_new(cbuild_context_t* ctx, const char* name);
config_t* cbuild_config_default_debug(cbuild_context_t* ctx);
config_t* cbuild_config_default_release(cbuild_context_t* ctx);
config_t* cbuild_config_default_wasm32(cbuild_context_t* ctx);

void cbuild_set_active_config(cbuild_context_t* ctx, config_t*);
void cbuild_target_set_config(cbuild_context_t* ctx, target_t*, config_t*);

void cbuild_config_free(cbuild_context_t* ctx, config_t*);
void cbuild_config_add_cflags(cbuild_context_t* ctx, config_t*, const char* flags);
void cbuild_config_add_ldflags(cbuild_context_t* ctx, config_t*, const char* flags);
void cbuild_config_add_define(cbuild_context_t* ctx, config_t*, const char* def);
void cbuild_config_add_include(cbuild_context_t* ctx, config_t*, const char* dir);
void cbuild_config_add_libdir(cbuild_context_t* ctx, config_t*, const char* dir);
void cbuild_config_add_linklib(cbuild_context_t* ctx, config_t*, const char* lib);
void cbuild_config_set_opt(cbuild_context_t* ctx, config_t*, int level);
void cbuild_config_set_debug(cbuild_context_t* ctx, config_t*, int on);
void cbuild_config_set_lto(cbuild_context_t* ctx, config_t*, int mode);
void cbuild_config_set_pic(cbuild_context_t* ctx, config_t*, int on);
void cbuild_config_set_warnings(cbuild_context_t* ctx, config_t*, int mode);
void cbuild_config_set_std(cbuild_context_t* ctx, config_t*, const char* std);
void cbuild_config_set_runtime(cbuild_context_t* ctx, config_t*, const char* runtime);
void cbuild_config_set_output_dir(cbuild_context_t* ctx, config_t*, const char* output_dir);
void cbuild_config_set_freestanding(cbuild_context_t* ctx, config_t*, int on);
void cbuild_config_enable_sanitizers(cbuild_context_t* ctx, config_t*, int mask);
void cbuild_config_disable_sanitizers(cbuild_context_t* ctx, config_t*, int mask);
void cbuild_config_set_compiler(cbuild_context_t* ctx, config_t*, const char* compiler);
void cbuild_config_set_linker(cbuild_context_t* ctx, config_t*, const char* linker);

// *** Command API *** //
int cbuild_run_command(cbuild_context_t* ctx, command_t* cmd);
command_t* cbuild_command(cbuild_context_t* ctx, const char* name, const char* command_line);
command_t* cbuild_command_argv(cbuild_context_t* ctx, const char* name, char** argv, int argc);
command_t* cbuild_command_function(cbuild_context_t* ctx, const char* name,
                                   cbuild_subcommand_callback callback,
                                   void* user_data);
void cbuild_target_add_command(cbuild_context_t* ctx, target_t* target, command_t* cmd);
void cbuild_target_add_post_command(cbuild_context_t* ctx, target_t* target, command_t* cmd);
void cbuild_command_add_dependency(cbuild_context_t* ctx, command_t* cmd, command_t* dependency);

// *** Subproject API *** //
subproject_t* cbuild_add_subproject(cbuild_context_t* ctx, const char* alias, const char* directory,
                                    const char* cbuild_exe);
target_t* cbuild_subproject_get_target(cbuild_context_t* ctx, subproject_t* sub, const char* tgt_name);
void cbuild_register_subcommand(cbuild_context_t* ctx, const char* name, target_t* target,
                                const char* command_line,
                                cbuild_subcommand_callback callback,
                                void* user_data);

// *** Test API *** //
/* Register a test. When target is an executable, the test runs its output with
 * args as the arguments; otherwise args[0] names the program to run. Either
 * way, target is built before the test runs. Exit code 0 passes. */
test_t* cbuild_add_test(cbuild_context_t* ctx, const char* name, target_t* target,
                        const char* const* args, int argc);
/* Build another target before the test runs (a generated input, for example). */
void cbuild_test_add_dependency(cbuild_context_t* ctx, test_t* test, target_t* target);
/* Labels group tests for --test-label. */
void cbuild_test_add_label(cbuild_context_t* ctx, test_t* test, const char* label);
/* Treat this exit code as a skip rather than a failure. */
void cbuild_test_set_skip_code(cbuild_context_t* ctx, test_t* test, int exit_code);
/* Fail the test if it runs longer than this many seconds (0 = no limit). */
void cbuild_test_set_timeout(cbuild_context_t* ctx, test_t* test, int seconds);
/* Tests that share a lock name never run at the same time. */
void cbuild_test_set_lock(cbuild_context_t* ctx, test_t* test, const char* lock);
/* Build what the selected tests need, then run them in parallel. label and
 * pattern (a wildcard on test names) may be NULL to select everything.
 * Returns 0 when every selected test passed or was skipped, -1 otherwise. */
int cbuild_run_tests(cbuild_context_t* ctx, const char* label, const char* pattern);

/* Register a flag handler. */
void cbuild_register_flag(cbuild_context_t* ctx, const char* long_name,
                          char short_name,
                          int takes_value,
                          cbuild_flag_phase_t phase,
                          const char* help,
                          cbuild_flag_callback cb,
                          void* user_data);

/* Convenience registrations that bind outputs directly. */
void cbuild_register_flag_bool(cbuild_context_t* ctx, const char* long_name, char short_name,
                               cbuild_flag_phase_t phase, const char* help,
                               int* out_bool);

void cbuild_register_flag_int(cbuild_context_t* ctx, const char* long_name, char short_name,
                              cbuild_flag_phase_t phase, const char* help,
                              int* out_int);

void cbuild_register_flag_str(cbuild_context_t* ctx, const char* long_name, char short_name,
                              cbuild_flag_phase_t phase, const char* help,
                              const char** out_str);

// *** Argument Parsing Helpers *** //
int cbuild_has_flag(int argc, char** argv, const char* long_name, char short_name);
/* Value of the last --long_name=VALUE, --long_name VALUE, -xVALUE, or -x VALUE
 * before any "--", or NULL. Lets a build read options before defining targets. */
const char* cbuild_get_flag_value(int argc, char** argv, const char* long_name, char short_name);

// *** Process Helpers *** //
/* Run argv[0] with its arguments (no shell) and wait for it. When output is not
 * NULL, stdout and stderr are captured into *output, a malloc'd string (or NULL
 * if the program printed nothing) that the caller frees. Returns the exit
 * status; a program that could not be started reports -1 (127 on Linux). */
int cbuild_spawn(cbuild_context_t* ctx, const char* const* argv, int argc, char** output);

// *** File System and Other Helpers *** //
int cbuild_file_exists(const char* path);
int cbuild_dir_exists(const char* path);
int cbuild_remove_file(const char* path);
int cbuild_remove_dir(const char* path);
int cbuild_get_cwd(char* buf, long size);
int cbuild_match_wildcard(const char* pattern, const char* string);
int cbuild_expand_wildcard(const char* pattern, char*** files, int* file_count);
int cbuild_expand_wildcard_recursive(const char* dir_path, const char* pattern,
                                     char*** files, int* file_count,
                                     int* capacity);

// *** Convenience Macros (require ctx variable in scope) *** //
#define CBUILD_SUBPROJECT(CTX, ALIAS, DIR, EXE) \
    subproject_t* ALIAS = cbuild_add_subproject((CTX), #ALIAS, DIR, EXE)

#define CBUILD_SELF_REBUILD(CTX, ARGC, ARGV, ...)                             \
    do {                                                                      \
        const char* _srcs[] = { __VA_ARGS__ };                                \
        if (cbuild_self_rebuild_if_needed((CTX), (ARGC), (ARGV), _srcs,       \
                                      (int)(sizeof(_srcs) / sizeof(*_srcs))) != 0) \
            return 1;                                                         \
    } while (0)

#define CBUILD_SOURCES(CTX, TGT, ...)                                \
    do {                                                             \
        const char* _a[] = { __VA_ARGS__ };                          \
        for (int _i = 0; _i < (int)(sizeof(_a) / sizeof(*_a)); _i++) \
            cbuild_add_source((CTX), (TGT), _a[_i]);                 \
    } while (0)

#define CBUILD_INCLUDES(CTX, TGT, ...)                               \
    do {                                                             \
        const char* _a[] = { __VA_ARGS__ };                          \
        for (int _i = 0; _i < (int)(sizeof(_a) / sizeof(*_a)); _i++) \
            cbuild_add_include_dir((CTX), (TGT), _a[_i]);            \
    } while (0)

#define CBUILD_LIB_DIRS(CTX, TGT, ...)                               \
    do {                                                             \
        const char* _a[] = { __VA_ARGS__ };                          \
        for (int _i = 0; _i < (int)(sizeof(_a) / sizeof(*_a)); _i++) \
            cbuild_add_library_dir((CTX), (TGT), _a[_i]);            \
    } while (0)

#define CBUILD_LINK_LIBS(CTX, TGT, ...)                              \
    do {                                                             \
        const char* _a[] = { __VA_ARGS__ };                          \
        for (int _i = 0; _i < (int)(sizeof(_a) / sizeof(*_a)); _i++) \
            cbuild_add_link_library((CTX), (TGT), _a[_i]);           \
    } while (0)

#define CBUILD_EXECUTABLE(CTX, NAME, ...)     \
    do {                                      \
        NAME = cbuild_executable((CTX), #NAME); \
        __VA_ARGS__                           \
    } while (0)

#define CBUILD_STATIC_LIBRARY(CTX, NAME, ...)     \
    do {                                          \
        NAME = cbuild_static_library((CTX), #NAME); \
        __VA_ARGS__                               \
    } while (0)

#define CBUILD_SHARED_LIBRARY(CTX, NAME, ...)     \
    do {                                          \
        NAME = cbuild_shared_library((CTX), #NAME); \
        __VA_ARGS__                               \
    } while (0)

#define CBUILD_DEFINES(CTX, TGT, ...)                                      \
    do {                                                                   \
        const char* _defs[] = { __VA_ARGS__ };                             \
        for (int _i = 0; _i < (int)(sizeof(_defs) / sizeof(*_defs)); _i++) \
            cbuild_add_define((CTX), (TGT), _defs[_i]);                    \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* CBUILD_H */

/* ---------------------------------------------------------------------- */
/* Implementation below (define CBUILD_IMPLEMENTATION in one source file) */
/* ---------------------------------------------------------------------- */
#ifdef CBUILD_IMPLEMENTATION

/* ---- src/cbuild_internal.h ---- */

/* cbuild_internal.h - platform setup, private types, and helpers shared by the
   cbuild sources. Not part of the public API. */
#ifndef CBUILD_INTERNAL_H
#define CBUILD_INTERNAL_H

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <ctype.h>
#include <errno.h>
#include <limits.h>  // for PATH_MAX
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>   // _mkdir
#include <io.h>       // _access or _stat
#include <process.h>  // _beginthreadex, _spawn
#include <windows.h>  // Windows API for threads, etc.
#define stat _stat
#define unlink _unlink
#define rmdir _rmdir
#define getcwd _getcwd
#define chdir _chdir
#define PATH_MAX MAX_PATH
/* POSIX compatibility macros for Windows */
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#define strtok_r strtok_s
#define strcasecmp _stricmp
#define strdup _strdup
#define isatty _isatty
#else
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>  // _NSGetExecutablePath
/* Avoid <sys/sysctl.h>: strict POSIX feature modes hide BSD u_int types
 * required by that header in recent macOS SDKs. */
extern int sysctlbyname(const char* name, void* oldp, size_t* oldlenp,
                        void* newp, size_t newlen);
#endif
#include <pthread.h>
#include <signal.h>
#include <strings.h>
#include <unistd.h>

extern char** environ; /* for posix_spawnp */
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096  // reasonable default for most systems
#endif

#define CBUILD_EXE_SUFFIX ".exe"

#ifndef _WIN32
#define CBUILD_COLOR_RESET "\033[0m"
#define CBUILD_COLOR_BOLD "\033[1m"
#define CBUILD_COLOR_GREEN "\033[32m"
#define CBUILD_COLOR_YELLOW "\033[33m"
#define CBUILD_COLOR_BLUE "\033[34m"
#define CBUILD_COLOR_MAGENTA "\033[35m"
#define CBUILD_COLOR_RED "\033[31m"
#define CBUILD_OBJ_EXT ".o"
#else
#define CBUILD_COLOR_RESET ""
#define CBUILD_COLOR_BOLD ""
#define CBUILD_COLOR_GREEN ""
#define CBUILD_COLOR_YELLOW ""
#define CBUILD_COLOR_BLUE ""
#define CBUILD_COLOR_MAGENTA ""
#define CBUILD_COLOR_RED ""
#define CBUILD_OBJ_EXT ".obj"
#endif


/* Helpers shared between source files have external linkage when src/ is
   built as separate translation units, and stay private to the including
   translation unit in the amalgamated single-header build. */
#ifdef CBUILD_IMPLEMENTATION
#define CBUILD_INTERNAL static
#else
#define CBUILD_INTERNAL
#endif

/* Modification time in the finest unit the platform reports (100ns ticks on
 * Windows, nanoseconds elsewhere). Only meaningful for comparing two files. */
typedef unsigned long long cbuild_mtime_t;

/* --- Data Structures for Build Targets, Commands, and Build Config --- */

typedef enum {
    TARGET_EXECUTABLE,
    TARGET_STATIC_LIB,
    TARGET_SHARED_LIB,
    TARGET_COMMAND,
    TARGET_DUMMY,
    TARGET_FILE_DEP
} cbuild_target_type;

typedef enum {
    CBUILD_CC_GCC_CLANG,
    CBUILD_CC_MSVC
} cbuild_cc_kind_t;

/* Global list of subcommands */
typedef struct cbuild_subcommand {
    char* name;
    target_t* target;
    char* command_line;
    cbuild_subcommand_callback callback;
    void* user_data;
} cbuild_subcommand_t;

typedef struct {
    char* directory;
    char* command;
    char** arguments;
    int argc;
    char* file;
} compile_commands_entry_t;

typedef struct cbuild_flag_handler {
    char* long_name;
    char short_name;
    int takes_value;
    cbuild_flag_phase_t phase;
    char* help;
    cbuild_flag_callback cb;
    void* user_data;
    /* direct bindings for convenience variants */
    int* bind_bool;
    int* bind_int;
    const char** bind_str;
    char* allocated_str;  /* owned copy of string value, freed on reset */
} cbuild_flag_handler_t;

typedef struct {
    target_t* target;
    int source_index;
} compile_job_t;

typedef struct cbuild_subproject_target {
    char* name;                          // logical name (e.g. "zlib")
    char* type;                          // "static_lib", "shared_lib", "executable"
    char* output_path;                   // relative to subproject directory
    struct cbuild_target* proxy_target;  // created on demand
} cbuild_subproject_target_t;

struct cbuild_subproject {
    char* alias;
    char* directory;
    char* cbuild_exe;
    command_t* build_cmd;
    int manifest_loaded;
    cbuild_subproject_target_t* targets;
    int target_count, target_cap;
};

/* The main context structure that holds all build state */
struct cbuild_context {
    /* Compiler kind */
    cbuild_cc_kind_t cc_kind;

    /* Logging callback and user data */
    cbuild_log_fn log_fn;
    void* log_user_data;

    /* Target list */
    target_t** targets;
    int target_count;
    int target_cap;

    /* Command list */
    command_t** commands;
    int command_count;
    int command_cap;

    /* Build settings */
    char* output_dir;
    int parallel_jobs;
    char* cc;
    char* ar;
    char* ld;
    char** global_cflags;
    int global_cflag_count;
    int global_cflag_cap;
    char** global_ldflags;
    int global_ldflag_count;
    int global_ldflag_cap;
    int verbose;
    char** target_filters;
    int target_filter_count;
    int target_filter_cap;

    /* Build configurations */
    config_t* default_config;
    config_t* active_config;
    target_t** cfg_targets;
    config_t** cfg_values;
    int cfg_count;
    int cfg_cap;

    /* All configs created via cbuild_config_new (for automatic cleanup) */
    config_t** all_configs;
    int all_configs_count;
    int all_configs_cap;

    /* DFS state */
    int* visited;
    int* in_stack;

    /* Global defines */
    char** global_defines;
    int global_def_count;
    int global_def_cap;

    /* Subcommands */
    cbuild_subcommand_t** subcommands;
    int subcommand_count;
    int subcommand_cap;

    /* Compile commands generation */
    int generate_compile_commands;
    compile_commands_entry_t* cc_entries;
    int cc_count;
    int cc_cap;

    /* Flag handlers */
    cbuild_flag_handler_t* flag_handlers;
    int flag_count;
    int flag_cap;

    /* Help display */
    const char* argv0_for_help;

    /* Job queue for parallel compilation */
    compile_job_t* job_queue;
    int job_count;
    int job_capacity;
    int jobs_completed;
    int compiled_count;

    /* Threading primitives */
#ifdef _WIN32
    HANDLE* threads;
    CRITICAL_SECTION queue_mutex;
#else
    pthread_t* threads;
    pthread_mutex_t queue_mutex;
#endif

    /* Build error flag */
    int build_error;

    /* Last error message for error propagation */
    char last_error[512];

    /* Subprojects */
    subproject_t** subprojects;
    int subproject_count;
    int subproject_cap;

    /* Run subcommand name */
    const char* run_subcmd;

    /* Tests and the --test selection */
    test_t** tests;
    int test_count;
    int test_cap;
    int run_tests;
    int list_tests;
    int test_timeout;  /* default seconds for tests without their own limit */
    char** test_labels;
    int test_label_count, test_label_cap;
    char** test_filters;
    int test_filter_count, test_filter_cap;
    char** test_excludes;
    int test_exclude_count, test_exclude_cap;

    /* User data for embedders */
    void* user_data;
};

struct cbuild_command {
    char* name;
    char* command_line;
    char** argv;
    int argc;
    cbuild_subcommand_callback callback;
    void* user_data;
    command_t** dependencies;
    int dep_count, dep_cap;
    int executed;
    int in_progress;  // set while running, to detect dependency cycles
    int result;
};

struct cbuild_config {
    const char* name;

    char** cflags;
    int ncflags;
    int cflags_cap;
    char** ldflags;
    int nldflags;
    int ldflags_cap;
    char** defines;
    int ndefines;
    int defines_cap;
    char** includes;
    int nincludes;
    int includes_cap;
    char** libdirs;
    int nlibdirs;
    int libdirs_cap;
    char** linklibs;
    int nlinklibs;
    int linklibs_cap;

    // structured knobs (portable intent → flags later)
    int opt_level;        // 0..3, or -1 = leave alone
    int debug_symbols;    // bool
    int lto;              // 0=off, 1=on, 2=thin
    int pic;              // bool
    int warnings;         // 0=default, 1=all, 2=pedantic
    int freestanding;     // bool
    const char* output_dir;  // custom build dir for this config
    const char* std;      // "c11","c17","gnu11", etc.
    const char* runtime;  // "static","dynamic" (where it makes sense)
    int sanitize;         // bitmask: ASAN|UBSAN|TSAN|… (0 none)

    // per-config tool overrides
    const char* compiler; // Override global g_cc
    const char* linker;   // Override global g_ld
};

/* Structure representing a build target (executable or library) */
struct cbuild_target {
    cbuild_target_type type;
    char* name;      // base name of target
    char** sources;  // array of source file paths
    int sources_count;
    int sources_cap;
    char** include_dirs;  // array of include directory paths
    int include_count;
    int include_cap;
    char** lib_dirs;  // library directories for linking
    int lib_dir_count;
    int lib_dir_cap;
    char** link_libs;  // external libraries to link (names or paths)
    int link_lib_count;
    int link_lib_cap;
    char** exposed_libs;
    int exposed_lib_count;
    int exposed_lib_cap;
    target_t** dependencies;  // other targets this target depends on (to link against)
    int dep_count;
    int dep_cap;
    char** cflags;  // compile flags tokens specific to this target
    int cflag_count;
    int cflag_cap;
    char** ldflags;  // linker flags tokens specific to this target
    int ldflag_count;
    int ldflag_cap;
    char* output_file;     // path to final output (exe, .a, .dll/.so)
    int output_file_explicit;  // set by cbuild_set_output_file; never re-derived
    char* obj_dir;         // directory for this target's object files (and .d files)
    command_t** commands;  // commands to run before building this target
    int cmd_count, cmd_cap;
    command_t** post_commands;  // commands to run after building this target
    int post_cmd_count, post_cmd_cap;
    char** defines;
    int define_count;
    int define_cap;
    char* soname;  // SONAME (Linux) or install_name (macOS) for shared libraries
    int config_applied;
    int external;  // output is produced by a subproject, not linked by this context
    int rebuilt;   // output was (re)produced during the current build pass

    // per-target tool overrides (from config or explicit)
    char* compiler;        // Override global g_cc for this target
    char* linker;          // Override global g_ld for this target
    int cc_kind_override;  // -1 = use global g_cc_kind, else cbuild_cc_kind_t
};

struct cbuild_test {
    char* name;
    target_t* target;  /* built first; when an executable, it is the program */
    char** args;
    int argc;
    target_t** deps;
    int dep_count, dep_cap;
    char** labels;
    int label_count, label_cap;
    char* lock;
    int skip_code;  /* -1 when no exit code means "skipped" */
    int timeout;    /* seconds, 0 = no limit */
};

#define CBUILD_MAX(a, b) ((a) > (b) ? (a) : (b))

/* --- Process Spawning API (for subproject builds) --- */
typedef struct {
    char** args;
    int count;
    int capacity;
} cbuild_argv_t;

/* build.c */
CBUILD_INTERNAL void dfs_build_func(cbuild_context_t* ctx, target_t* t, int* error_flag_ptr);
CBUILD_INTERNAL void cbuild__resolve_target_paths(cbuild_context_t* ctx);

/* compile.c */
CBUILD_INTERNAL int process_compile_jobs_parallel(cbuild_context_t* ctx, target_t* target, int* error_flag);
CBUILD_INTERNAL void collect_compile_commands_for_target(cbuild_context_t* ctx, target_t* t);

/* config.c */
CBUILD_INTERNAL void cbuild__apply_config_if_needed(cbuild_context_t* ctx, target_t* t);

/* context.c */
CBUILD_INTERNAL void cbuild_init(cbuild_context_t* ctx);

/* flags.c */
CBUILD_INTERNAL void cbuild__print_help(cbuild_context_t* ctx);
CBUILD_INTERNAL int cbuild__positional_args(cbuild_context_t* ctx, int argc, char** argv, int* first);
CBUILD_INTERNAL int cbuild_dispatch_flags_strict(cbuild_context_t* ctx, cbuild_flag_phase_t phase,
                                                 int* argc, char*** argvp);

/* fs.c */
CBUILD_INTERNAL char* cbuild__normalize_path(const char* path);
CBUILD_INTERNAL int cbuild__mtime(const char* path, cbuild_mtime_t* out);
CBUILD_INTERNAL char* cbuild__join_path(const char* a, const char* b);
CBUILD_INTERNAL int cbuild__is_library_path(const char* path);
CBUILD_INTERNAL void get_dir_from_path(const char* path, char* out_dir, size_t out_size);
CBUILD_INTERNAL int ensure_dir_exists(const char* path);
CBUILD_INTERNAL int cbuild__strcmp_wrapper(const void* a, const void* b);
CBUILD_INTERNAL void remove_file(cbuild_context_t* ctx, const char* path);
CBUILD_INTERNAL void cbuild__clean_dir(cbuild_context_t* ctx, const char* path);

/* log.c */
CBUILD_INTERNAL void cbuild__log(cbuild_context_t* ctx, cbuild_log_level_t level, const char* fmt, ...);
CBUILD_INTERNAL void cbuild__log_step(cbuild_context_t* ctx, const char* label, const char* color, const char* fmt, ...);
CBUILD_INTERNAL void cbuild__log_status(cbuild_context_t* ctx, int ok, const char* fmt, ...);
CBUILD_INTERNAL void cbuild__set_error(cbuild_context_t* ctx, const char* fmt, ...);

/* process.c */
CBUILD_INTERNAL void cbuild_argv_init(cbuild_argv_t* argv);
CBUILD_INTERNAL void cbuild_argv_free(cbuild_argv_t* argv);
CBUILD_INTERNAL int cbuild_argv_append(cbuild_argv_t* argv, const char* arg);
CBUILD_INTERNAL char* cbuild_argv_to_cmdline(cbuild_argv_t* argv);
CBUILD_INTERNAL int cbuild_spawn_process(cbuild_context_t* ctx, cbuild_argv_t* argv, int capture_output, char** captured_output);
CBUILD_INTERNAL int run_command(cbuild_context_t* ctx, const char* cmd, int capture_out,
                                char** captured_output);
CBUILD_INTERNAL void cbuild__argv_append_prefixed(cbuild_argv_t* argv, const char* prefix, const char* value);
CBUILD_INTERNAL void cbuild_argv_append_flags(cbuild_argv_t* argv, const char* flags);

/* target.c */
CBUILD_INTERNAL int cbuild__add_define_to_list(cbuild_context_t* ctx, char*** arr, int* count, int* cap,
                                                const char* macro,
                                                const char* value_optional);

/* test.c */
CBUILD_INTERNAL int cbuild__run_selected_tests(cbuild_context_t* ctx);
CBUILD_INTERNAL void cbuild__list_tests(cbuild_context_t* ctx);
CBUILD_INTERNAL void cbuild__free_tests(cbuild_context_t* ctx);

/* toolchain.c */
CBUILD_INTERNAL cbuild_cc_kind_t detect_cc_kind(const char* exe);
CBUILD_INTERNAL cbuild_cc_kind_t detect_ar_kind(const char* ar);
CBUILD_INTERNAL const char* cbuild_target_compiler(cbuild_context_t* ctx, target_t* t);
CBUILD_INTERNAL cbuild_cc_kind_t cbuild_target_cc_kind(cbuild_context_t* ctx, target_t* t);
CBUILD_INTERNAL const char* cbuild_target_linker(cbuild_context_t* ctx, target_t* t);

/* util.c */
CBUILD_INTERNAL void cbuild__trim(char* s);
CBUILD_INTERNAL int ensure_capacity_charpp(cbuild_context_t* ctx, char*** arr, int* count, int* capacity);
CBUILD_INTERNAL int append_format(char** dst, const char* fmt, ...);
CBUILD_INTERNAL int cbuild__has_suffix(const char* s, const char* suf);
CBUILD_INTERNAL unsigned cbuild__hash_path(const char* s);
CBUILD_INTERNAL void fprint_json_string(FILE* f, const char* s);

#endif /* CBUILD_INTERNAL_H */

/* ---- src/build.c ---- */

/* build.c - target graph traversal, clean, and the per-target build and link steps. */


static void build_target(cbuild_context_t* ctx, target_t* t, int* error_flag);

static void dfs_command_func(cbuild_context_t* ctx, command_t* cmd, int* error_flag_ptr) {
    if (!cmd || *error_flag_ptr)
        return;
    if (cmd->executed)
        return;
    /* cbuild_run_command runs dependencies first and rejects cycles */
    if (cbuild_run_command(ctx, cmd) != 0) {
        *error_flag_ptr = 1;
    }
}

CBUILD_INTERNAL void dfs_build_func(cbuild_context_t* ctx, target_t* t, int* error_flag_ptr) {
    int ti = -1;
    for (int j = 0; j < ctx->target_count; ++j) {
        if (ctx->targets[j] == t) {
            ti = j;
            break;
        }
    }
    if (ti == -1)
        return;
    if (*error_flag_ptr)
        return;
    if (ctx->in_stack[ti]) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: circular dependency involving %s", t->name);
        *error_flag_ptr = 1;
        return;
    }
    if (ctx->visited[ti])
        return;
    ctx->in_stack[ti] = 1;

    if (t->type == TARGET_FILE_DEP) {
        /* Its commands only run when the file is stale, so unlike other targets
         * the dependencies (e.g. a generator tool) come first. */
        for (int di = 0; di < t->dep_count; ++di) {
            dfs_build_func(ctx, t->dependencies[di], error_flag_ptr);
            if (*error_flag_ptr) {
                ctx->in_stack[ti] = 0;
                return;
            }
        }
        build_target(ctx, t, error_flag_ptr);
        ctx->visited[ti] = 1;
        ctx->in_stack[ti] = 0;
        return;
    }

    for (int ci = 0; ci < t->cmd_count; ++ci) {
        dfs_command_func(ctx, t->commands[ci], error_flag_ptr);
        if (*error_flag_ptr) {
            ctx->in_stack[ti] = 0;
            return;
        }
    }

    for (int di = 0; di < t->dep_count; ++di) {
        dfs_build_func(ctx, t->dependencies[di], error_flag_ptr);
        if (*error_flag_ptr) {
            ctx->in_stack[ti] = 0;
            return;
        }
    }
    build_target(ctx, t, error_flag_ptr);
    for (int pci = 0; pci < t->post_cmd_count; ++pci) {
        dfs_command_func(ctx, t->post_commands[pci], error_flag_ptr);
        if (*error_flag_ptr) {
            ctx->in_stack[ti] = 0;
            return;
        }
    }
    ctx->visited[ti] = 1;
    ctx->in_stack[ti] = 0;
}

/* Recalculate target output paths based on per-target or active config.
 * Called after PRE flags are processed so config changes take effect.
 * Also resets config_applied flag so the full config (flags, optimizations, etc.)
 * gets applied with the correct active config. */
CBUILD_INTERNAL void cbuild__resolve_target_paths(cbuild_context_t* ctx) {
    const char *default_output_dir = ctx->active_config ? (ctx->active_config->output_dir ? ctx->active_config->output_dir : ctx->output_dir) : ctx->output_dir;

    for (int i = 0; i < ctx->target_count; ++i) {
        target_t* t = ctx->targets[i];

        /* Every build pass starts here, so this is also where per-pass state resets. */
        t->rebuilt = 0;

        /* Skip targets whose paths must not be derived from this context. */
        if (t->external || t->type == TARGET_COMMAND || t->type == TARGET_DUMMY || t->type == TARGET_FILE_DEP) {
            continue;
        }

        /* Look up per-target config to get its output_dir if set */
        const char *output_dir = default_output_dir;
        for (int j = 0; j < ctx->cfg_count; ++j) {
            if (ctx->cfg_targets[j] == t && ctx->cfg_values[j] && ctx->cfg_values[j]->output_dir) {
                output_dir = ctx->cfg_values[j]->output_dir;
                break;
            }
        }

        /* Recalculate obj_dir */
        if (t->obj_dir) free(t->obj_dir);
        t->obj_dir = NULL;
        append_format(&t->obj_dir, "%s/obj_%s", output_dir, t->name);

        /* A path chosen with cbuild_set_output_file is not ours to re-derive. */
        if (t->output_file_explicit) continue;

        if (t->output_file) {
            free(t->output_file);
            t->output_file = NULL;
        }

        /* Recalculate output_file based on type */
        char *out = NULL;
        if (t->type == TARGET_EXECUTABLE) {
#ifdef _WIN32
            append_format(&out, "%s/%s.exe", output_dir, t->name);
#else
            append_format(&out, "%s/%s", output_dir, t->name);
#endif
        } else if (t->type == TARGET_STATIC_LIB) {
#ifdef _WIN32
            append_format(&out, "%s/%s.lib", output_dir, t->name);
#else
            append_format(&out, "%s/lib%s.a", output_dir, t->name);
#endif
        } else if (t->type == TARGET_SHARED_LIB) {
#ifdef _WIN32
            append_format(&out, "%s/%s.dll", output_dir, t->name);
#elif __APPLE__
            append_format(&out, "%s/lib%s.dylib", output_dir, t->name);
#else
            append_format(&out, "%s/lib%s.so", output_dir, t->name);
#endif
        }

        t->output_file = out;
    }
}

int cbuild_clean(cbuild_context_t* ctx) {
    cbuild_init(ctx);
    cbuild__resolve_target_paths(ctx);

    cbuild__log_step(ctx, "CLEAN", CBUILD_COLOR_YELLOW, "Cleaning build outputs...");
    for (int i = 0; i < ctx->subproject_count; ++i) {
        subproject_t* sub = ctx->subprojects[i];
        cbuild__log_step(ctx, "CLEAN", CBUILD_COLOR_YELLOW, "Cleaning subproject: %s", sub->alias);
        char old_cwd[PATH_MAX];
        if (cbuild_get_cwd(old_cwd, sizeof(old_cwd)) == 0) {
            if (chdir(sub->directory) == 0) {
                cbuild_argv_t avv;
                cbuild_argv_init(&avv);
                cbuild_argv_append(&avv, sub->cbuild_exe);
                cbuild_argv_append(&avv, "--clean");
                (void)cbuild_spawn_process(ctx, &avv, 0, NULL);
                cbuild_argv_free(&avv);
                chdir(old_cwd);
            }
        }
    }
    for (int i = 0; i < ctx->target_count; ++i) {
        target_t* t = ctx->targets[i];
        if (t->external) continue; /* cleaned by its owning subproject */
        /* A file-dep target without commands names a file cbuild did not create. */
        if (t->type == TARGET_FILE_DEP && t->cmd_count == 0) continue;
        if (t->obj_dir) cbuild__clean_dir(ctx, t->obj_dir);
        if (t->output_file) {
            remove_file(ctx, t->output_file);
            if (t->type != TARGET_FILE_DEP) {
                char* link_sig = NULL;
                if (append_format(&link_sig, "%s.link.sig", t->output_file) == 0) remove_file(ctx, link_sig);
                free(link_sig);
            }
        }
    }
    if (ctx->active_config && ctx->active_config->output_dir) {
        cbuild__clean_dir(ctx, ctx->active_config->output_dir);
    }
    cbuild__clean_dir(ctx, ctx->output_dir);
    cbuild__log_status(ctx, 1, "Clean complete.");
    return 0;
}

int cbuild_build(cbuild_context_t* ctx, const char* target_name) {
    cbuild_init(ctx);
    cbuild__resolve_target_paths(ctx);

    if (ctx->cc_entries) {
        for (int i = 0; i < ctx->cc_count; ++i) {
            free(ctx->cc_entries[i].directory);
            free(ctx->cc_entries[i].command);
            free(ctx->cc_entries[i].file);
            if (ctx->cc_entries[i].arguments) {
                for (int j = 0; j < ctx->cc_entries[i].argc; ++j) {
                    free(ctx->cc_entries[i].arguments[j]);
                }
                free(ctx->cc_entries[i].arguments);
            }
        }
        free(ctx->cc_entries);
        ctx->cc_entries = NULL;
    }
    ctx->cc_count = 0;
    ctx->cc_cap = 0;

    if (ctx->job_queue) {
        free(ctx->job_queue);
        ctx->job_queue = NULL;
        ctx->job_capacity = 0;
    }

    if (ctx->generate_compile_commands) {
        for (int i = 0; i < ctx->target_count; ++i) {
            collect_compile_commands_for_target(ctx, ctx->targets[i]);
        }
    }

    int error_flag = 0;
    if (ctx->visited) free(ctx->visited);
    if (ctx->in_stack) free(ctx->in_stack);
    ctx->visited = calloc(ctx->target_count, sizeof(int));
    ctx->in_stack = calloc(ctx->target_count, sizeof(int));

    if (target_name) {
        target_t* target_to_build = NULL;
        for (int i = 0; i < ctx->target_count; ++i) {
            if (ctx->targets[i]->name && strcmp(ctx->targets[i]->name, target_name) == 0) {
                target_to_build = ctx->targets[i];
                break;
            }
        }
        if (!target_to_build) {
            cbuild__set_error(ctx, "Target '%s' not found", target_name);
            cbuild__log(ctx, CBUILD_LOG_ERROR, "Target '%s' not found", target_name);
            free(ctx->visited);
            free(ctx->in_stack);
            ctx->visited = NULL;
            ctx->in_stack = NULL;
            return -1;
        }
        dfs_build_func(ctx, target_to_build, &error_flag);
    } else {
        int* indegree = calloc(ctx->target_count, sizeof(int));
        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            for (int d = 0; d < t->dep_count; ++d) {
                target_t* dep = t->dependencies[d];
                for (int j = 0; j < ctx->target_count; ++j) {
                    if (ctx->targets[j] == dep) {
                        indegree[j]++;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            if (t->type == TARGET_FILE_DEP && indegree[i] == 0) continue;
            if (!ctx->visited[i]) {
                dfs_build_func(ctx, t, &error_flag);
                if (error_flag) break;
            }
        }
        free(indegree);
    }

    free(ctx->visited);
    ctx->visited = NULL;
    free(ctx->in_stack);
    ctx->in_stack = NULL;

    if (!error_flag) {
        if (ctx->generate_compile_commands) {
            const char* output_dir = ctx->active_config ? (ctx->active_config->output_dir ? ctx->active_config->output_dir : ctx->output_dir) : ctx->output_dir;
            ensure_dir_exists(output_dir);
            char path[1024];
            snprintf(path, sizeof(path), "%s/compile_commands.json", output_dir);
            FILE* f = fopen(path, "w");
            if (f) {
                fprintf(f, "[\n");
                for (int i = 0; i < ctx->cc_count; i++) {
                    fprintf(f, "  {\n    \"directory\": ");
                    fprint_json_string(f, ctx->cc_entries[i].directory);
                    fprintf(f, ",\n    \"arguments\": [");
                    for (int j = 0; j < ctx->cc_entries[i].argc; ++j) {
                        if (j > 0) fprintf(f, ", ");
                        fprint_json_string(f, ctx->cc_entries[i].arguments[j]);
                    }
                    fprintf(f, "],\n    \"file\": ");
                    fprint_json_string(f, ctx->cc_entries[i].file);
                    fprintf(f, "\n  }%s\n", (i + 1 < ctx->cc_count) ? "," : "");
                }
                fprintf(f, "]\n");
                fclose(f);
            }
        }
        cbuild__log_status(ctx, 1, "Build succeeded.");
        return 0;
    } else {
        cbuild__log_status(ctx, 0, "Build failed.");
        return -1;
    }
}

/* Append sorted copies of items so the command, and so its signature, is stable. */
static void cbuild__argv_append_sorted(cbuild_argv_t* argv, char** items, int count, const char* prefix) {
    if (count <= 0) return;
    char** tmp = (char**)malloc(sizeof(char*) * count);
    if (!tmp) return;
    memcpy(tmp, items, sizeof(char*) * count);
    qsort(tmp, count, sizeof(char*), cbuild__strcmp_wrapper);
    for (int i = 0; i < count; ++i) {
        if (prefix) {
            cbuild__argv_append_prefixed(argv, prefix, tmp[i]);
        } else {
            cbuild_argv_append(argv, tmp[i]);
        }
    }
    free(tmp);
}

/* Build the archiver or linker argv for a target. The link step and its rebuild
 * signature both go through here so they cannot drift. obj_files must be sorted. */
static void cbuild__link_argv(cbuild_context_t* ctx, target_t* t, char** obj_files, int obj_count,
                              cbuild_argv_t* argv) {
    if (t->type == TARGET_STATIC_LIB) {
        /* Use append_flags to handle archivers with spaces (e.g., "zig ar") */
        cbuild_argv_append_flags(argv, ctx->ar);
        if (detect_ar_kind(ctx->ar) == CBUILD_CC_MSVC) {
            cbuild__argv_append_prefixed(argv, "/OUT:", t->output_file);
        } else {
            cbuild_argv_append(argv, "rcs");
            cbuild_argv_append(argv, t->output_file);
        }
        for (int i = 0; i < obj_count; ++i)
            cbuild_argv_append(argv, obj_files[i]);
        return;
    }

    int msvc = cbuild_target_cc_kind(ctx, t) == CBUILD_CC_MSVC;

    /* Use append_flags to handle linkers with spaces (e.g., "zig cc -target ...") */
    cbuild_argv_append_flags(argv, cbuild_target_linker(ctx, t));
    if (msvc) {
        cbuild_argv_append(argv, "/nologo");
        cbuild__argv_append_prefixed(argv, "/Fe:", t->output_file);
    } else {
        cbuild_argv_append(argv, "-o");
        cbuild_argv_append(argv, t->output_file);
    }

    for (int i = 0; i < obj_count; ++i)
        cbuild_argv_append(argv, obj_files[i]);

    cbuild__argv_append_sorted(argv, t->lib_dirs, t->lib_dir_count, msvc ? "/LIBPATH:" : "-L");

    if (t->exposed_lib_count > 0) {
        // Export dynamic symbols for plugin/dlopen use
        if (!msvc) {
#if defined(__linux__) || defined(__unix__) && !defined(__APPLE__)
            cbuild_argv_append(argv, "-Wl,-E");
#endif
        }
    }
    cbuild__argv_append_sorted(argv, t->exposed_libs, t->exposed_lib_count, NULL);

    /* Our own libraries come before system libraries, so single-pass linkers
     * can resolve their references (for example to libm) further along. */
    for (int i = 0; i < t->dep_count; ++i) {
        target_t* dep = t->dependencies[i];
        if (dep->type == TARGET_STATIC_LIB) {
            cbuild_argv_append(argv, dep->output_file);
        } else if (dep->type == TARGET_SHARED_LIB) {
#ifdef _WIN32
            /* On Windows, link against .lib import library, not .dll */
            char lib_path[1024];
            strncpy(lib_path, dep->output_file, sizeof(lib_path) - 1);
            lib_path[sizeof(lib_path) - 1] = '\0';
            size_t len = strlen(lib_path);
            if (len > 4 && strcmp(lib_path + len - 4, ".dll") == 0) {
                strcpy(lib_path + len - 4, ".lib");
            }
            cbuild_argv_append(argv, lib_path);
#else
            cbuild_argv_append(argv, dep->output_file);
#endif
        }
        // FILE_DEP is linkable iff its output looks like a library path.
        else if (dep->type == TARGET_FILE_DEP && dep->output_file &&
                 cbuild__is_library_path(dep->output_file)) {
            cbuild_argv_append(argv, dep->output_file);
        }
    }

    for (int i = 0; i < t->link_lib_count; ++i) {
        const char* lib = t->link_libs[i];
        if (msvc) {
            /* Skip 'm' (math) library on MSVC - it's part of the CRT */
            if (strcmp(lib, "m") == 0) continue;
            if (strchr(lib, '\\') || strchr(lib, ':')) {
                cbuild_argv_append(argv, lib);
            } else {
                char lib_arg[512];
                snprintf(lib_arg, sizeof(lib_arg), "%s.lib", lib);
                cbuild_argv_append(argv, lib_arg);
            }
        } else if (strchr(lib, '/')) {
            cbuild_argv_append(argv, lib);
        } else {
            cbuild__argv_append_prefixed(argv, "-l", lib);
        }
    }

    for (int i = 0; i < t->ldflag_count; ++i)
        cbuild_argv_append(argv, t->ldflags[i]);
    for (int i = 0; i < ctx->global_ldflag_count; ++i)
        cbuild_argv_append(argv, ctx->global_ldflags[i]);

    if (t->type == TARGET_SHARED_LIB) {
        if (msvc) {
            cbuild_argv_append(argv, "/LD");
        } else {
#ifdef __APPLE__
            cbuild_argv_append(argv, "-dynamiclib");
            if (t->soname) {
                cbuild_argv_append(argv, "-install_name");
                cbuild__argv_append_prefixed(argv, "@rpath/", t->soname);
            }
#else
            cbuild_argv_append(argv, "-shared");
            if (t->soname) cbuild__argv_append_prefixed(argv, "-Wl,-soname,", t->soname);
#endif
        }
    }
}

/* Everything that must stay the same for a linked output to be reusable. */
static char* cbuild__link_signature(cbuild_argv_t* argv) {
    char* sig = NULL;
    for (int i = 0; i < argv->count; ++i) {
        append_format(&sig, "%s\n", argv->args[i]);
    }
    const char* ev = getenv("LDFLAGS");
    if (ev) append_format(&sig, "ENV:LDFLAGS=%s\n", ev);
    return sig;
}

static void build_target(cbuild_context_t* ctx, target_t* t, int* error_flag) {
    if (t->external) {
        if (!cbuild_file_exists(t->output_file)) {
            cbuild__log_status(ctx, 0, "Subproject output '%s' was not produced", t->output_file);
            *error_flag = 1;
        }
        return;
    }

    /* Compute and apply effective build configuration (defaults → active → target) */
    cbuild__apply_config_if_needed(ctx, t);

    if (t->type == TARGET_DUMMY) {
        cbuild__log_step(ctx, "DUMMY", CBUILD_COLOR_MAGENTA, "%s", t->name);
        return;
    }

    if (t->type == TARGET_FILE_DEP) {
        int should_update = 0;
        cbuild_mtime_t out_mtime = 0;

        if (cbuild__mtime(t->output_file, &out_mtime) != 0) {
            should_update = 1;  // output missing
        } else {
            for (int i = 0; i < t->sources_count; ++i) {
                cbuild_mtime_t src_mtime;
                if (cbuild__mtime(t->sources[i], &src_mtime) == 0 &&
                    src_mtime > out_mtime) {
                    should_update = 1;  // source newer than output
                    break;
                }
            }
            for (int i = 0; i < t->dep_count && !should_update; ++i) {
                target_t* dep = t->dependencies[i];
                cbuild_mtime_t dep_mtime;
                /* dep->rebuilt catches a dependency rebuilt within the same
                 * timestamp tick as the existing output */
                if (dep->rebuilt || (dep->output_file && cbuild__mtime(dep->output_file, &dep_mtime) == 0 &&
                                     dep_mtime > out_mtime)) {
                    should_update = 1;  // dependency output newer than output
                }
            }
        }

        if (should_update) {
            for (int i = 0; i < t->cmd_count; ++i) {
                if (cbuild_run_command(ctx, t->commands[i]) != 0) {
                    *error_flag = 1;
                    return;
                }
            }
            if (!cbuild_file_exists(t->output_file)) {
                cbuild__log_status(ctx, 0, "Required file '%s' missing after commands in target '%s'", t->output_file, t->name);
                *error_flag = 1;
                return;
            }
            cbuild__log_step(ctx, "FILE_DEP", CBUILD_COLOR_GREEN, "%s (updated)", t->output_file);
            t->rebuilt = 1;
        }
        return;
    }

    int compiled_sources = process_compile_jobs_parallel(ctx, t, error_flag);
    if (*error_flag) return;

    int obj_count = t->sources_count;
    char** obj_files = (char**)calloc(obj_count, sizeof(char*));
    for (int i = 0; i < obj_count; ++i) {
        const char* src = t->sources[i];
        const char* slash = strrchr(src, '/');
        const char* bslash = strrchr(src, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
        const char* base = slash ? slash + 1 : src;
        char* dot = strrchr(base, '.');
        size_t len = dot ? (size_t)(dot - base) : strlen(base);
        unsigned h = cbuild__hash_path(src);
        char objname[512];
        snprintf(objname, sizeof(objname), "%s/%.*s-%08x" CBUILD_OBJ_EXT, t->obj_dir, (int)len, base, h);
        obj_files[i] = strdup(objname);
    }
    /* Sort object files to stabilize build results */
    qsort(obj_files, obj_count, sizeof(char*), cbuild__strcmp_wrapper);

    int needs_link = compiled_sources > 0;
    cbuild_mtime_t out_mtime;
    if (cbuild__mtime(t->output_file, &out_mtime) != 0) {
        needs_link = 1;
    } else {
        for (int i = 0; i < obj_count; ++i) {
            cbuild_mtime_t obj_mtime;
            if (cbuild__mtime(obj_files[i], &obj_mtime) != 0 ||
                obj_mtime > out_mtime) {
                needs_link = 1;
                break;
            }
        }
        if (!needs_link) {
            for (int i = 0; i < t->dep_count; ++i) {
                target_t* dep = t->dependencies[i];
                /* A dependency rebuilt in this pass may share a timestamp tick
                 * with the existing output, so do not rely on mtime alone. */
                if (dep->rebuilt) {
                    needs_link = 1;
                    break;
                }
                if (dep->output_file) {
                    cbuild_mtime_t dep_mtime;
                    if (cbuild__mtime(dep->output_file, &dep_mtime) == 0 && dep_mtime > out_mtime) {
                        needs_link = 1;
                        break;
                    }
                }
            }
        }
    }

    cbuild_argv_t argv;
    cbuild_argv_init(&argv);
    cbuild__link_argv(ctx, t, obj_files, obj_count, &argv);
    char* link_sig = cbuild__link_signature(&argv);
    char link_sig_path[1024];
    snprintf(link_sig_path, sizeof(link_sig_path), "%s.link.sig", t->output_file);

    /* If timestamps say "up-to-date", also verify the link command signature */
    if (!needs_link) {
        FILE* lf = fopen(link_sig_path, "rb");
        if (!lf) {
            needs_link = 1;
        } else {
            fseek(lf, 0, SEEK_END);
            long lsz = ftell(lf);
            fseek(lf, 0, SEEK_SET);
            char* prev = (lsz >= 0) ? (char*)malloc((size_t)lsz + 1) : NULL;
            if (!prev) {
                needs_link = 1;
            } else {
                size_t rd = fread(prev, 1, (size_t)lsz, lf);
                prev[rd] = '\0';
                if (strcmp(prev, link_sig ? link_sig : "") != 0) needs_link = 1;
                free(prev);
            }
            fclose(lf);
        }
    }

    if (needs_link) {
        char output_dir[1024];
        get_dir_from_path(t->output_file, output_dir, sizeof(output_dir));
        ensure_dir_exists(output_dir);

        cbuild__log_step(ctx, "LINK", CBUILD_COLOR_YELLOW, "%s", t->output_file);

        // Print command in verbose mode before executing
        if (ctx->verbose) {
            char cmd_buf[4096];
            int bpos = 0;
            for (int i = 0; i < argv.count && bpos < (int)sizeof(cmd_buf) - 1; ++i) {
                if (i > 0) cmd_buf[bpos++] = ' ';
                if (strchr(argv.args[i], ' ')) {
                    bpos += snprintf(cmd_buf + bpos, sizeof(cmd_buf) - bpos, "'%s'", argv.args[i]);
                } else {
                    bpos += snprintf(cmd_buf + bpos, sizeof(cmd_buf) - bpos, "%s", argv.args[i]);
                }
            }
            cmd_buf[bpos] = '\0';
            cbuild__log(ctx, CBUILD_LOG_VERBOSE, "%s", cmd_buf);
        }

        char* output = NULL;
        int rc = cbuild_spawn_process(ctx, &argv, 1, &output);

        if (output && rc != 0) {
            fwrite(output, 1, strlen(output), stderr);
        }
        if (output)
            free(output);

        if (rc != 0) {
            cbuild__log_status(ctx, 0, "Linking failed for %s", t->output_file);
            *error_flag = 1;
        } else {
            t->rebuilt = 1;
            /* On successful link, write signature of the link command */
            FILE* lf = fopen(link_sig_path, "wb");
            if (lf) {
                fwrite(link_sig ? link_sig : "", 1, link_sig ? strlen(link_sig) : 0, lf);
                fclose(lf);
            }
        }
    }

    free(link_sig);
    cbuild_argv_free(&argv);
    for (int i = 0; i < obj_count; ++i)
        free(obj_files[i]);
    free(obj_files);
}

/* ---- src/cli.c ---- */

/* cli.c - built-in command-line flags and the cbuild_run entry point. */


/* ---------- Flag callback helpers ---------- */

static int cbuild__flag_on_verbose(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    ctx->verbose = 1;
    return 0;
}

static int cbuild__flag_on_target(const char* v, void* u) {
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    if (!v || !*v) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: --target requires a name");
        return 1;
    }
    ensure_capacity_charpp(ctx, &ctx->target_filters, &ctx->target_filter_count, &ctx->target_filter_cap);
    ctx->target_filters[ctx->target_filter_count++] = strdup(v);
    return 0;
}

static int cbuild__flag_on_compile_commands(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    cbuild_enable_compile_commands(ctx, 1);
    return 0;
}

static int cbuild__flag_on_list(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    char buf[8192];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "Available targets:");
    for (int i = 0; i < ctx->target_count; ++i) {
        target_t* t = ctx->targets[i];
        if (!t->name) continue;
        const char* type_str = "unknown";
        switch (t->type) {
            case TARGET_EXECUTABLE:
                type_str = "executable";
                break;
            case TARGET_STATIC_LIB:
                type_str = "static_lib";
                break;
            case TARGET_SHARED_LIB:
                type_str = "shared_lib";
                break;
            case TARGET_COMMAND:
                type_str = "command";
                break;
            case TARGET_DUMMY:
                type_str = "dummy";
                break;
            case TARGET_FILE_DEP:
                type_str = "file_dep";
                break;
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "\n  %-20s  [%s]", t->name, type_str);
        if (t->output_file) pos += snprintf(buf + pos, sizeof(buf) - pos, "  -> %s", t->output_file);
    }
    cbuild__log(ctx, CBUILD_LOG_INFO, "%s", buf);
    return CBUILD_FLAG_EXIT;
}

/* Pretty-print build graph */
static const char* cbuild__type_str(cbuild_target_type tt) {
    switch (tt) {
        case TARGET_EXECUTABLE:
            return "executable";
        case TARGET_STATIC_LIB:
            return "static_lib";
        case TARGET_SHARED_LIB:
            return "shared_lib";
        case TARGET_COMMAND:
            return "command";
        case TARGET_DUMMY:
            return "dummy";
        case TARGET_FILE_DEP:
            return "file_dep";
        default:
            return "unknown";
    }
}

static int cbuild__idx_of_target(cbuild_context_t* ctx, target_t* t) {
    for (int i = 0; i < ctx->target_count; ++i) {
        if (ctx->targets[i] == t) return i;
    }
    return -1;
}

static void cbuild__print_graph_rec(cbuild_context_t* ctx, target_t* t, int* visiting, int depth) {
    int ti = cbuild__idx_of_target(ctx, t);
    if (ti >= 0) {
        if (visiting[ti]) {
            const char* cyc_name = t->name ? t->name : (t->output_file ? t->output_file : "(unnamed)");
            cbuild__log(ctx, CBUILD_LOG_INFO, "%*s-> (cycle to %s)", depth * 2, "", cyc_name);
            return;
        }
        visiting[ti] = 1;
    }

    for (int i = 0; i < t->dep_count; ++i) {
        target_t* dep = t->dependencies[i];
        const char* dep_name = dep->name ? dep->name : (dep->output_file ? dep->output_file : "(unnamed)");
        cbuild__log(ctx, CBUILD_LOG_INFO, "%*s-> %s [%s]", depth * 2, "", dep_name, cbuild__type_str(dep->type));
        cbuild__print_graph_rec(ctx, dep, visiting, depth + 1);
    }

    if (ti >= 0) visiting[ti] = 0;
}

static int cbuild__flag_on_graph(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;

    cbuild__log(ctx, CBUILD_LOG_INFO, "Build graph:");
    if (ctx->target_count == 0) {
        cbuild__log(ctx, CBUILD_LOG_INFO, "  (no targets)");
        return CBUILD_FLAG_EXIT;
    }

    int* indegree = (int*)calloc(ctx->target_count, sizeof(int));
    if (indegree) {
        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            for (int d = 0; d < t->dep_count; ++d) {
                target_t* dep = t->dependencies[d];
                for (int j = 0; j < ctx->target_count; ++j) {
                    if (ctx->targets[j] == dep) {
                        indegree[j]++;
                        break;
                    }
                }
            }
        }
    }

    int* visiting = (int*)calloc(ctx->target_count, sizeof(int));
    if (visiting) {
        int roots = 0;
        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            /* Skip orphan FILE_DEPs unless something depends on them */
            if (t->type == TARGET_FILE_DEP && indegree && indegree[i] == 0) continue;

            if (!indegree || indegree[i] == 0) {
                const char* name = t->name ? t->name : (t->output_file ? t->output_file : "(unnamed)");
                cbuild__log(ctx, CBUILD_LOG_INFO, "%s [%s]", name, cbuild__type_str(t->type));
                cbuild__print_graph_rec(ctx, t, visiting, 1);
                roots++;
            }
        }
        /* If no roots (cycle-only graph), print all once */
        if (roots == 0) {
            for (int i = 0; i < ctx->target_count; ++i) {
                target_t* t = ctx->targets[i];
                const char* name = t->name ? t->name : (t->output_file ? t->output_file : "(unnamed)");
                cbuild__log(ctx, CBUILD_LOG_INFO, "%s [%s]", name, cbuild__type_str(t->type));
                cbuild__print_graph_rec(ctx, t, visiting, 1);
            }
        }
        free(visiting);
    }
    if (indegree) free(indegree);

    return CBUILD_FLAG_EXIT;
}

/* Reverse dependency (consumers) graph */
static int cbuild__find_target_by_name(cbuild_context_t* ctx, const char* name) {
    if (!name) return -1;
    for (int i = 0; i < ctx->target_count; ++i) {
        target_t* t = ctx->targets[i];
        if (t->name && strcmp(t->name, name) == 0) return i;
    }
    return -1;
}

static void cbuild__print_consumers_rec(cbuild_context_t* ctx, int tgt_idx, int* visiting, int depth) {
    for (int i = 0; i < ctx->target_count; ++i) {
        target_t* t = ctx->targets[i];
        int depends = 0;
        for (int d = 0; d < t->dep_count; ++d) {
            if (t->dependencies[d] == ctx->targets[tgt_idx]) {
                depends = 1;
                break;
            }
        }
        if (!depends) continue;

        if (visiting[i]) {
            const char* cyc_name = t->name ? t->name : (t->output_file ? t->output_file : "(unnamed)");
            cbuild__log(ctx, CBUILD_LOG_INFO, "%*s-> (cycle to %s)", depth * 2, "", cyc_name);
            continue;
        }
        const char* name = t->name ? t->name : (t->output_file ? t->output_file : "(unnamed)");
        cbuild__log(ctx, CBUILD_LOG_INFO, "%*s-> %s [%s]", depth * 2, "", name, cbuild__type_str(t->type));
        visiting[i] = 1;
        cbuild__print_consumers_rec(ctx, i, visiting, depth + 1);
        visiting[i] = 0;
    }
}

static int cbuild__flag_on_deps(const char* v, void* u) {
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    if (!v || !*v) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: --deps requires a target name");
        return 1;
    }
    int idx = cbuild__find_target_by_name(ctx, v);
    if (idx < 0) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: target '%s' not found", v);
        return 1;
    }
    target_t* root = ctx->targets[idx];
    const char* name = root->name ? root->name : (root->output_file ? root->output_file : "(unnamed)");
    cbuild__log(ctx, CBUILD_LOG_INFO, "%s [%s]", name, cbuild__type_str(root->type));
    int* visiting = (int*)calloc(ctx->target_count, sizeof(int));
    if (visiting) {
        cbuild__print_consumers_rec(ctx, idx, visiting, 1);
        free(visiting);
    }
    return CBUILD_FLAG_EXIT;
}

static int cbuild__flag_on_manifest(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    for (int i = 0; i < ctx->target_count; ++i) {
        target_t* t = ctx->targets[i];
        if (!t->output_file || !t->name) continue;
        const char* type = NULL;
        switch (t->type) {
            case TARGET_STATIC_LIB:
                type = "static_lib";
                break;
            case TARGET_SHARED_LIB:
                type = "shared_lib";
                break;
            case TARGET_EXECUTABLE:
                type = "executable";
                break;
            default:
                continue;
        }
        cbuild__log(ctx, CBUILD_LOG_INFO, "%s %s %s", type, t->name, t->output_file);
    }
    return CBUILD_FLAG_EXIT;
}

static int cbuild__flag_on_clean(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    cbuild_clean(ctx);
    return CBUILD_FLAG_EXIT;
}

static int cbuild__flag_on_run(const char* v, void* u) {
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    if (!v || !*v) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: --run needs a name");
        return 1;
    }
    ctx->run_subcmd = v;
    return 0;
}

static int cbuild__flag_on_test(const char* v, void* u) {
    (void)v;
    ((cbuild_context_t*)u)->run_tests = 1;
    return 0;
}

static int cbuild__flag_on_list_tests(const char* v, void* u) {
    (void)v;
    ((cbuild_context_t*)u)->list_tests = 1;
    return 0;
}

static int cbuild__append_test_selection(cbuild_context_t* ctx, const char* flag, const char* v,
                                         char*** list, int* count, int* cap) {
    if (!v || !*v) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: --%s requires a value", flag);
        return 1;
    }
    ensure_capacity_charpp(ctx, list, count, cap);
    (*list)[(*count)++] = strdup(v);
    return 0;
}

static int cbuild__flag_on_test_label(const char* v, void* u) {
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    return cbuild__append_test_selection(ctx, "test-label", v, &ctx->test_labels, &ctx->test_label_count,
                                         &ctx->test_label_cap);
}

static int cbuild__flag_on_test_filter(const char* v, void* u) {
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    return cbuild__append_test_selection(ctx, "test-filter", v, &ctx->test_filters, &ctx->test_filter_count,
                                         &ctx->test_filter_cap);
}

static int cbuild__flag_on_test_exclude(const char* v, void* u) {
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    return cbuild__append_test_selection(ctx, "test-exclude", v, &ctx->test_excludes, &ctx->test_exclude_count,
                                         &ctx->test_exclude_cap);
}

static int cbuild__flag_on_version(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    cbuild__log(ctx, CBUILD_LOG_INFO, "%s", CBUILD_VERSION);
    return CBUILD_FLAG_EXIT;
}

static int cbuild__flag_on_help(const char* v, void* u) {
    (void)v;
    cbuild_context_t* ctx = (cbuild_context_t*)u;
    cbuild__print_help(ctx);
    return CBUILD_FLAG_EXIT;
}

int cbuild_configure_from_argv(cbuild_context_t* ctx, int argc, char** argv) {
    cbuild_init(ctx);
    ctx->argv0_for_help = (argv && argv[0]) ? argv[0] : "cbuild";

    cbuild_register_flag(ctx, "verbose", 'v', 0, CBUILD_FLAG_PRE, "Verbose output", cbuild__flag_on_verbose, ctx);
    cbuild_register_flag_int(ctx, "jobs", 'j', CBUILD_FLAG_PRE,
                             "Number of parallel compile and test jobs", &ctx->parallel_jobs);
    cbuild_register_flag(ctx, "target", 't', 1, CBUILD_FLAG_PRE,
                         "Build only the specified target", cbuild__flag_on_target, ctx);
    cbuild_register_flag(ctx, "compile-commands", 0, 0, CBUILD_FLAG_PRE,
                         "Emit compile_commands.json into the output dir", cbuild__flag_on_compile_commands, ctx);
    cbuild_register_flag(ctx, "version", 0, 0, CBUILD_FLAG_PRE, "Show version and exit", cbuild__flag_on_version, ctx);
    cbuild_register_flag(ctx, "help", 'h', 0, CBUILD_FLAG_PRE, "Show help and exit", cbuild__flag_on_help, ctx);

    int rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_PRE, &argc, &argv);
    if (rc == CBUILD_FLAG_EXIT) return CBUILD_FLAG_EXIT;
    if (rc) return rc;

    int first = -1;
    if (cbuild__positional_args(ctx, argc, argv, &first) > 0) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: unexpected argument '%s'", argv[first]);
        return 1;
    }

    return 0;
}

int cbuild_run(cbuild_context_t* ctx, int argc, char** argv) {
    cbuild_init(ctx);

    ctx->argv0_for_help = (argv && argv[0]) ? argv[0] : "cbuild";

    cbuild_register_flag(ctx, "verbose", 'v', 0, CBUILD_FLAG_PRE, "Verbose output", cbuild__flag_on_verbose, ctx);
    cbuild_register_flag_int(ctx, "jobs", 'j', CBUILD_FLAG_PRE,
                             "Number of parallel compile and test jobs", &ctx->parallel_jobs);
    cbuild_register_flag(ctx, "target", 't', 1, CBUILD_FLAG_PRE,
                         "Build only the specified target", cbuild__flag_on_target, ctx);
    cbuild_register_flag(ctx, "compile-commands", 0, 0, CBUILD_FLAG_PRE,
                         "Emit compile_commands.json into the output dir", cbuild__flag_on_compile_commands, ctx);
    cbuild_register_flag(ctx, "list", 'l', 0, CBUILD_FLAG_PRE,
                         "List targets and exit", cbuild__flag_on_list, ctx);
    cbuild_register_flag(ctx, "graph", 0, 0, CBUILD_FLAG_PRE,
                         "Pretty print build graph and exit", cbuild__flag_on_graph, ctx);
    cbuild_register_flag(ctx, "deps", 0, 1, CBUILD_FLAG_PRE,
                         "Print reverse dependency (consumers) graph for the given target and exit", cbuild__flag_on_deps, ctx);
    cbuild_register_flag(ctx, "manifest", 0, 0, CBUILD_FLAG_PRE,
                         "Print subproject manifest format and exit", cbuild__flag_on_manifest, ctx);
    cbuild_register_flag(ctx, "clean", 0, 0, CBUILD_FLAG_PRE,
                         "Remove build outputs and exit", cbuild__flag_on_clean, ctx);
    cbuild_register_flag(ctx, "run", 'r', 1, CBUILD_FLAG_PRE,
                         "Run a registered subcommand after building its target", cbuild__flag_on_run, ctx);
    cbuild_register_flag(ctx, "test", 0, 0, CBUILD_FLAG_PRE,
                         "Build what the tests need, then run them", cbuild__flag_on_test, ctx);
    cbuild_register_flag(ctx, "list-tests", 0, 0, CBUILD_FLAG_PRE,
                         "List the selected tests and exit", cbuild__flag_on_list_tests, ctx);
    cbuild_register_flag(ctx, "test-label", 0, 1, CBUILD_FLAG_PRE,
                         "Only tests with this label (repeatable)", cbuild__flag_on_test_label, ctx);
    cbuild_register_flag(ctx, "test-filter", 0, 1, CBUILD_FLAG_PRE,
                         "Only tests whose name matches this wildcard (repeatable)", cbuild__flag_on_test_filter, ctx);
    cbuild_register_flag(ctx, "test-exclude", 0, 1, CBUILD_FLAG_PRE,
                         "Skip tests whose name matches this wildcard (repeatable)", cbuild__flag_on_test_exclude, ctx);
    cbuild_register_flag_int(ctx, "test-timeout", 0, CBUILD_FLAG_PRE,
                             "Seconds before a test without its own limit fails", &ctx->test_timeout);
    cbuild_register_flag(ctx, "version", 0, 0, CBUILD_FLAG_PRE, "Show version and exit", cbuild__flag_on_version, ctx);
    cbuild_register_flag(ctx, "help", 'h', 0, CBUILD_FLAG_PRE, "Show help and exit", cbuild__flag_on_help, ctx);

    int rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_PRE, &argc, &argv);
    if (rc == CBUILD_FLAG_EXIT) return 0;
    if (rc) return rc;

    cbuild__resolve_target_paths(ctx);

    /* A single bare argument naming a registered subcommand is shorthand for --run=NAME. */
    int first = -1;
    int positional = cbuild__positional_args(ctx, argc, argv, &first);
    if (positional == 1 && !ctx->run_subcmd) {
        for (int i = 0; i < ctx->subcommand_count; i++) {
            if (strcmp(ctx->subcommands[i]->name, argv[first]) == 0) {
                ctx->run_subcmd = argv[first];
                for (int k = first; k < argc; ++k) argv[k] = argv[k + 1];
                argc--;
                positional = 0;
                break;
            }
        }
    }

    if (positional > 0) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: unexpected argument '%s'", argv[first]);
        cbuild__log(ctx, CBUILD_LOG_INFO, "hint: use flags or a subcommand name (try --help)");
        return 1;
    }

    if (ctx->list_tests) {
        cbuild__list_tests(ctx);
        return 0;
    }

    if (ctx->run_tests && !ctx->run_subcmd) {
        rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_BEFORE_BUILD, &argc, &argv);
        if (rc == CBUILD_FLAG_EXIT) return 0;
        if (rc) return rc;
        if (cbuild__run_selected_tests(ctx) != 0) return 1;
        rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_AFTER_BUILD, &argc, &argv);
        if (rc == CBUILD_FLAG_EXIT) return 0;
        return rc;
    }

    if (ctx->run_subcmd) {
        int error_flag = 0;

        if (ctx->visited) free(ctx->visited);
        if (ctx->in_stack) free(ctx->in_stack);
        ctx->visited = calloc(ctx->target_count, sizeof(int));
        ctx->in_stack = calloc(ctx->target_count, sizeof(int));

        cbuild_subcommand_t* match = NULL;
        for (int i = 0; i < ctx->subcommand_count; ++i) {
            if (strcmp(ctx->subcommands[i]->name, ctx->run_subcmd) == 0) {
                match = ctx->subcommands[i];
                break;
            }
        }
        if (!match) {
            cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: unknown subcommand '%s'", ctx->run_subcmd);
            free(ctx->visited);
            ctx->visited = NULL;
            free(ctx->in_stack);
            ctx->in_stack = NULL;
            return 1;
        }

        rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_BEFORE_BUILD, &argc, &argv);
        if (rc == CBUILD_FLAG_EXIT) {
            free(ctx->visited);
            free(ctx->in_stack);
            return 0;
        }
        if (rc) {
            free(ctx->visited);
            free(ctx->in_stack);
            return rc;
        }

        dfs_build_func(ctx, match->target, &error_flag);

        free(ctx->visited);
        ctx->visited = NULL;
        free(ctx->in_stack);
        ctx->in_stack = NULL;

        if (error_flag) {
            cbuild__log_status(ctx, 0, "Build failed.");
            return 1;
        }

        int sub_rc = 0;
        if (match->command_line) {
            cbuild__log_step(ctx, "SUBCMD", CBUILD_COLOR_BLUE, "Running '%s': %s", match->name, match->command_line);
            sub_rc = run_command(ctx, match->command_line, 0, NULL);
        } else if (match->callback) {
            cbuild__log_step(ctx, "SUBCMD", CBUILD_COLOR_BLUE, "Running '%s' (callback)...", match->name);
            match->callback(match->user_data);
        }

        if (sub_rc == 0) {
            rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_AFTER_BUILD, &argc, &argv);
            if (rc == CBUILD_FLAG_EXIT) return 0;
            if (rc) return rc;
        }
        return sub_rc;
    }

    if (ctx->cc_entries) {
        for (int i = 0; i < ctx->cc_count; ++i) {
            free(ctx->cc_entries[i].directory);
            free(ctx->cc_entries[i].command);
            free(ctx->cc_entries[i].file);
            if (ctx->cc_entries[i].arguments) {
                for (int j = 0; j < ctx->cc_entries[i].argc; ++j) {
                    free(ctx->cc_entries[i].arguments[j]);
                }
                free(ctx->cc_entries[i].arguments);
            }
        }
        free(ctx->cc_entries);
        ctx->cc_entries = NULL;
    }
    ctx->cc_count = 0;
    ctx->cc_cap = 0;

    if (ctx->job_queue) {
        free(ctx->job_queue);
        ctx->job_queue = NULL;
        ctx->job_capacity = 0;
    }

    int error_flag = 0;
    if (ctx->visited) free(ctx->visited);
    if (ctx->in_stack) free(ctx->in_stack);
    ctx->visited = calloc(ctx->target_count, sizeof(int));
    ctx->in_stack = calloc(ctx->target_count, sizeof(int));

    rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_BEFORE_BUILD, &argc, &argv);
    if (rc == CBUILD_FLAG_EXIT) {
        free(ctx->visited);
        free(ctx->in_stack);
        return 0;
    }
    if (rc) {
        free(ctx->visited);
        free(ctx->in_stack);
        return rc;
    }

    /* Collected after BEFORE_BUILD flags so the entries match what gets compiled */
    if (ctx->generate_compile_commands) {
        for (int i = 0; i < ctx->target_count; ++i) {
            collect_compile_commands_for_target(ctx, ctx->targets[i]);
        }
    }

    if (ctx->target_filter_count > 0) {
        for (int f = 0; f < ctx->target_filter_count; ++f) {
            target_t* target_to_build = NULL;
            for (int i = 0; i < ctx->target_count; ++i) {
                if (ctx->targets[i]->name && strcmp(ctx->targets[i]->name, ctx->target_filters[f]) == 0) {
                    target_to_build = ctx->targets[i];
                    break;
                }
            }
            if (!target_to_build) {
                cbuild__log(ctx, CBUILD_LOG_ERROR, "Target '%s' not found", ctx->target_filters[f]);
                cbuild__log(ctx, CBUILD_LOG_INFO, "Use --list to see available targets");
                free(ctx->visited);
                free(ctx->in_stack);
                return 1;
            }
            dfs_build_func(ctx, target_to_build, &error_flag);
            if (error_flag) break;
        }
    } else {
        int* indegree = calloc(ctx->target_count, sizeof(int));
        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            for (int d = 0; d < t->dep_count; ++d) {
                target_t* dep = t->dependencies[d];
                for (int j = 0; j < ctx->target_count; ++j) {
                    if (ctx->targets[j] == dep) {
                        indegree[j]++;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            if (t->type == TARGET_FILE_DEP && indegree[i] == 0) continue;
            if (!ctx->visited[i]) {
                dfs_build_func(ctx, t, &error_flag);
                if (error_flag) break;
            }
        }
        free(indegree);
    }
    free(ctx->visited);
    ctx->visited = NULL;
    free(ctx->in_stack);
    ctx->in_stack = NULL;

    if (!error_flag) {
        rc = cbuild_dispatch_flags_strict(ctx, CBUILD_FLAG_AFTER_BUILD, &argc, &argv);
        if (rc == CBUILD_FLAG_EXIT) return 0;
        if (rc) return rc;

        if (ctx->generate_compile_commands) {
            const char* output_dir = ctx->active_config ? (ctx->active_config->output_dir ? ctx->active_config->output_dir : ctx->output_dir) : ctx->output_dir;
            ensure_dir_exists(output_dir);
            char path[1024];
            snprintf(path, sizeof(path), "%s/compile_commands.json", output_dir);
            FILE* f = fopen(path, "w");
            if (f) {
                fprintf(f, "[\n");
                for (int i = 0; i < ctx->cc_count; i++) {
                    fprintf(f, "  {\n    \"directory\": ");
                    fprint_json_string(f, ctx->cc_entries[i].directory);
                    fprintf(f, ",\n    \"arguments\": [");
                    for (int j = 0; j < ctx->cc_entries[i].argc; ++j) {
                        if (j > 0) fprintf(f, ", ");
                        fprint_json_string(f, ctx->cc_entries[i].arguments[j]);
                    }
                    fprintf(f, "],\n    \"file\": ");
                    fprint_json_string(f, ctx->cc_entries[i].file);
                    fprintf(f, "\n  }%s\n", (i + 1 < ctx->cc_count) ? "," : "");
                }
                fprintf(f, "]\n");
                fclose(f);
            }
        }
        cbuild__log_status(ctx, 1, "Build succeeded.");
        return 0;
    } else {
        cbuild__log_status(ctx, 0, "Build failed.");
        return 1;
    }
}

/* ---- src/command.c ---- */

/* command.c - custom commands and subcommands. */


command_t* cbuild_command(cbuild_context_t* ctx, const char* name, const char* command_line) {
    command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
    cmd->name = strdup(name);
    cmd->command_line = command_line ? strdup(command_line) : NULL;
    cmd->argv = NULL;
    cmd->argc = 0;
    ensure_capacity_charpp(ctx, (char***)&ctx->commands, &ctx->command_count,
                           &ctx->command_cap);
    ctx->commands[ctx->command_count++] = cmd;
    return cmd;
}

command_t* cbuild_command_argv(cbuild_context_t* ctx, const char* name, char** argv, int argc) {
    if (!name || !argv || argc <= 0)
        return NULL;

    command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
    cmd->name = strdup(name);
    cmd->command_line = NULL;
    cmd->argc = argc;
    cmd->argv = (char**)calloc(argc + 1, sizeof(char*));
    for (int i = 0; i < argc; ++i) {
        cmd->argv[i] = strdup(argv[i]);
    }
    cmd->argv[argc] = NULL; /* NULL-terminate for execvp */

    ensure_capacity_charpp(ctx, (char***)&ctx->commands, &ctx->command_count,
                           &ctx->command_cap);
    ctx->commands[ctx->command_count++] = cmd;

    return cmd;
}

void cbuild_target_add_command(cbuild_context_t* ctx, target_t* target, command_t* cmd) {
    (void)ctx;
    if (!target || !cmd)
        return;
    ensure_capacity_charpp(ctx, (char***)&target->commands, &target->cmd_count,
                           &target->cmd_cap);
    target->commands[target->cmd_count++] = cmd;
}

void cbuild_target_add_post_command(cbuild_context_t* ctx, target_t* target, command_t* cmd) {
    if (!target || !cmd)
        return;
    ensure_capacity_charpp(ctx, (char***)&target->post_commands,
                           &target->post_cmd_count, &target->post_cmd_cap);
    target->post_commands[target->post_cmd_count++] = cmd;
}

void cbuild_command_add_dependency(cbuild_context_t* ctx, command_t* cmd, command_t* dependency) {
    if (!cmd || !dependency)
        return;
    ensure_capacity_charpp(ctx, (char***)&cmd->dependencies, &cmd->dep_count,
                           &cmd->dep_cap);
    cmd->dependencies[cmd->dep_count++] = dependency;
}

int cbuild_run_command(cbuild_context_t* ctx, command_t* cmd) {
    if (!cmd)
        return -1;
    if (cmd->executed)
        return cmd->result;
    if (cmd->in_progress) {
        cbuild__set_error(ctx, "Circular command dependency involving '%s'", cmd->name);
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: circular command dependency involving %s", cmd->name);
        return -1;
    }
    cmd->in_progress = 1;
    for (int i = 0; i < cmd->dep_count; ++i) {
        int rc = cbuild_run_command(ctx, cmd->dependencies[i]);
        if (rc != 0) {
            cmd->in_progress = 0;
            return rc;
        }
    }

    cbuild__log_step(ctx, "COMMAND", CBUILD_COLOR_MAGENTA, "%s", cmd->name);

    int rc = 0;

    if (cmd->callback) {
        cmd->callback(cmd->user_data);
        rc = 0;
    } else if (cmd->argv && cmd->argc > 0) {
        /* Argv-based command: shell-free execution */
        cbuild_argv_t argv;
        cbuild_argv_init(&argv);
        for (int i = 0; i < cmd->argc; ++i) {
            cbuild_argv_append(&argv, cmd->argv[i]);
        }
        rc = cbuild_spawn_process(ctx, &argv, 0, NULL);
        cbuild_argv_free(&argv);
    } else if (cmd->command_line) {
        /* Note: user-defined command lines may use shell syntax, so we keep run_command here */
        rc = run_command(ctx, cmd->command_line, 0, NULL);
    } else {
        cbuild__log_status(ctx, 0, "No command or callback in command: %s", cmd->name);
        rc = -1;
    }

    cmd->executed = 1;
    cmd->in_progress = 0;
    cmd->result = rc;

    if (rc != 0) {
        cbuild__log_status(ctx, 0, "Command failed: %s", cmd->name);
    }

    return rc;
}

void cbuild_register_subcommand(cbuild_context_t* ctx, const char* name, target_t* target,
                                const char* command_line,
                                cbuild_subcommand_callback callback,
                                void* user_data) {
    cbuild_subcommand_t* scmd =
        (cbuild_subcommand_t*)calloc(1, sizeof(cbuild_subcommand_t));
    scmd->name = strdup(name);
    scmd->target = target;
    scmd->command_line = command_line ? strdup(command_line) : NULL;
    scmd->callback = callback;
    scmd->user_data = user_data;
    ensure_capacity_charpp(ctx, (char***)&ctx->subcommands, &ctx->subcommand_count,
                           &ctx->subcommand_cap);
    ctx->subcommands[ctx->subcommand_count++] = scmd;
}

command_t* cbuild_command_function(cbuild_context_t* ctx, const char* name,
                                   cbuild_subcommand_callback callback,
                                   void* user_data) {
    if (!name || !callback)
        return NULL;

    command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
    cmd->name = strdup(name);
    cmd->argv = NULL;
    cmd->argc = 0;
    cmd->callback = callback;
    cmd->user_data = user_data;

    ensure_capacity_charpp(ctx, (char***)&ctx->commands, &ctx->command_count,
                           &ctx->command_cap);
    ctx->commands[ctx->command_count++] = cmd;

    return cmd;
}

/* ---- src/compile.c ---- */

/* compile.c - compile argv, signatures, header dependencies, and the parallel job queue. */


/* Thread worker argument - passes context to worker threads */
typedef struct {
    cbuild_context_t* ctx;
} compile_worker_arg_t;

/* Build the compiler argv for one source file. The compile step, the rebuild
 * signature, and compile_commands.json all go through here so they cannot drift. */
static void cbuild__compile_argv(cbuild_context_t* ctx, target_t* t, const char* src_file,
                                 const char* obj_file, const char* dep_file, cbuild_argv_t* argv) {
    int msvc = cbuild_target_cc_kind(ctx, t) == CBUILD_CC_MSVC;
    const char* inc_prefix = msvc ? "/I" : "-I";
    const char* def_prefix = msvc ? "/D" : "-D";

    /* Use append_flags to handle compilers with spaces (e.g., "zig cc -target ...") */
    cbuild_argv_append_flags(argv, cbuild_target_compiler(ctx, t));

    if (msvc) {
        cbuild_argv_append(argv, "/c");
        cbuild_argv_append(argv, "/nologo");
        cbuild__argv_append_prefixed(argv, "/Fo", obj_file);
        cbuild_argv_append(argv, "/showIncludes");
    } else {
        cbuild_argv_append(argv, "-c");
        cbuild_argv_append(argv, "-o");
        cbuild_argv_append(argv, obj_file);
        /* Add dependency generation flags for GCC/Clang */
        if (dep_file) {
            cbuild_argv_append(argv, "-MMD");
            cbuild_argv_append(argv, "-MF");
            cbuild_argv_append(argv, dep_file);
        }
    }

    for (int i = 0; i < ctx->global_cflag_count; ++i) {
        cbuild_argv_append(argv, ctx->global_cflags[i]);
    }
    for (int i = 0; i < t->cflag_count; ++i) {
        cbuild_argv_append(argv, t->cflags[i]);
    }
    for (int i = 0; i < t->include_count; ++i) {
        cbuild__argv_append_prefixed(argv, inc_prefix, t->include_dirs[i]);
    }
    for (int i = 0; i < ctx->global_def_count; ++i) {
        cbuild__argv_append_prefixed(argv, def_prefix, ctx->global_defines[i]);
    }
    for (int i = 0; i < t->define_count; ++i) {
        cbuild__argv_append_prefixed(argv, def_prefix, t->defines[i]);
    }

    cbuild_argv_append(argv, src_file);
}

/* Everything that must stay the same for an object file to be reusable. */
static char* cbuild__compile_signature(cbuild_argv_t* argv) {
    char* sig = NULL;
    for (int i = 0; i < argv->count; ++i) {
        append_format(&sig, "%s\n", argv->args[i]);
    }
    const char* ev;
    ev = getenv("CFLAGS");
    if (ev) append_format(&sig, "ENV:CFLAGS=%s\n", ev);
    ev = getenv("CPPFLAGS");
    if (ev) append_format(&sig, "ENV:CPPFLAGS=%s\n", ev);
    return sig;
}

static int cbuild__is_line_continuation(const char* p) {
    return p[0] == '\\' && (p[1] == '\n' || (p[1] == '\r' && p[2] == '\n'));
}

/* Returns 1 when a make-style dependency file (GCC/Clang -MMD) is unreadable, or
 * names a prerequisite that is missing or newer than obj_mtime. Understands the
 * escapes compilers emit: "\ " and "\#" for literal characters, "$$" for "$". */
static int cbuild__make_deps_outdated(const char* dep_file, cbuild_mtime_t obj_mtime) {
    FILE* df = fopen(dep_file, "rb");
    if (!df) return 1;
    fseek(df, 0, SEEK_END);
    long fsize = ftell(df);
    fseek(df, 0, SEEK_SET);
    char* content = (fsize >= 0) ? (char*)malloc((size_t)fsize + 1) : NULL;
    if (!content) {
        fclose(df);
        return 1;
    }
    size_t read_size = fread(content, 1, (size_t)fsize, df);
    content[read_size] = '\0';
    fclose(df);

    /* Skip "target:". The separator colon is followed by whitespace or a line
     * continuation, which tells it apart from a Windows drive letter (C:\...). */
    char* p = content;
    while (*p && !(*p == ':' && (p[1] == '\0' || p[1] == ' ' || p[1] == '\t' || p[1] == '\n' ||
                                 p[1] == '\r' || cbuild__is_line_continuation(p + 1)))) {
        p++;
    }
    if (!*p) {
        free(content);
        return 1;
    }
    p++;

    int outdated = 0;
    while (!outdated) {
        for (;;) {
            if (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            } else if (cbuild__is_line_continuation(p)) {
                p += (p[1] == '\r') ? 3 : 2;
            } else {
                break;
            }
        }
        /* Only the first rule lists prerequisites; -MP adds phony rules after it. */
        if (*p == '\0' || *p == '\n') break;

        /* Unescape the path in place; the result is never longer than the input. */
        char* token = p;
        char* out = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' &&
               !cbuild__is_line_continuation(p)) {
            if (*p == '\\') {
                /* Make halves a run of backslashes before a space or '#'; an odd
                 * run makes that character literal. Elsewhere they stand as-is. */
                size_t run = 1;
                while (p[run] == '\\') run++;
                char next = p[run];
                if (next == ' ' || next == '\t' || next == '#') {
                    for (size_t k = 0; k < run / 2; ++k) *out++ = '\\';
                    p += run;
                    if (run % 2) *out++ = *p++;
                    continue;
                }
            } else if (*p == '$' && p[1] == '$') {
                p++;
            }
            *out++ = *p++;
        }
        char terminator = *p;
        *out = '\0';

        cbuild_mtime_t dep_mtime;
        if (cbuild__mtime(token, &dep_mtime) != 0 || dep_mtime > obj_mtime) outdated = 1;

        if (out == p) *p = terminator;
    }

    free(content);
    return outdated;
}

static int need_recompile(cbuild_context_t* ctx, const char* src_file, const char* obj_file,
                          const char* dep_file, target_t* t) {
    /* First: compute and compare a signature of the would-be compile command */
    char* sig = NULL;
    cbuild_cc_kind_t cc_kind = cbuild_target_cc_kind(ctx, t);
    {
        cbuild_argv_t argv;
        cbuild_argv_init(&argv);
        cbuild__compile_argv(ctx, t, src_file, obj_file, dep_file, &argv);
        sig = cbuild__compile_signature(&argv);
        cbuild_argv_free(&argv);
    }
    if (!sig) return 1;

    char sigpath[1024];
    snprintf(sigpath, sizeof(sigpath), "%s.sig", obj_file);
    int sig_mismatch = 0;
    {
        FILE* f = fopen(sigpath, "rb");
        if (!f) {
            sig_mismatch = 1;
#ifdef CBUILD_DEBUG_SIGNATURE
            fprintf(stderr, "DEBUG: Signature file missing for %s (%s)\n", src_file, sigpath);
#endif
        } else {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* prev = (char*)malloc((size_t)sz + 1);
            if (!prev) {
                sig_mismatch = 1;
            } else {
                size_t rd = fread(prev, 1, (size_t)sz, f);
                prev[rd] = '\0';
                if (strcmp(prev, sig) != 0) {
                    sig_mismatch = 1;
#ifdef CBUILD_DEBUG_SIGNATURE
                    fprintf(stderr, "DEBUG: Signature mismatch for %s\n", src_file);
                    fprintf(stderr, "--- PREV (from file, %ld bytes) ---\n%s\n", sz, prev);
                    fprintf(stderr, "--- NEW (computed, %zu bytes) ---\n%s\n", strlen(sig), sig);
                    fprintf(stderr, "--- END ---\n");
#endif
                }
                free(prev);
            }
            fclose(f);
        }
    }
    if (sig) free(sig);
    if (sig_mismatch) {
        return 1;
    }

    /* Fallback: timestamp checks on src/object and header dependencies */
    cbuild_mtime_t src_mtime, obj_mtime;
    if (cbuild__mtime(src_file, &src_mtime) != 0) {
#ifdef CBUILD_DEBUG_SIGNATURE
        fprintf(stderr, "DEBUG: stat() failed for source file: %s\n", src_file);
#endif
        return 1;
    }
    if (cbuild__mtime(obj_file, &obj_mtime) != 0) {
#ifdef CBUILD_DEBUG_SIGNATURE
        fprintf(stderr, "DEBUG: stat() failed for object file: %s\n", obj_file);
#endif
        return 1;
    }
    if (src_mtime > obj_mtime) {
#ifdef CBUILD_DEBUG_SIGNATURE
        fprintf(stderr, "DEBUG: Source newer than object: %s (src=%llu, obj=%llu)\n",
                src_file, src_mtime, obj_mtime);
#endif
        return 1;
    }

    /* Check header dependencies from .d file */
    if (dep_file) {
        FILE* df = fopen(dep_file, "r");
        if (!df) return 1;

        /* MSVC uses a cbuild-specific, one-path-per-line format so paths
         * containing spaces (such as Windows SDK paths) remain intact. */
        if (cc_kind == CBUILD_CC_MSVC) {
            char path[4096];
            if (!fgets(path, sizeof(path), df)) {
                fclose(df);
                return 1;
            }
            path[strcspn(path, "\r\n")] = '\0';
            if (strcmp(path, "CBUILD_MSVC_DEPS_V1") != 0) {
                fclose(df);
                return 1;
            }

            while (fgets(path, sizeof(path), df)) {
                path[strcspn(path, "\r\n")] = '\0';
                if (!path[0]) continue;

                cbuild_mtime_t dep_mtime;
                if (cbuild__mtime(path, &dep_mtime) != 0 || dep_mtime > obj_mtime) {
#ifdef CBUILD_DEBUG_SIGNATURE
                    fprintf(stderr, "DEBUG: MSVC dependency changed or missing for %s: %s\n",
                            src_file, path);
#endif
                    fclose(df);
                    return 1;
                }
            }
            fclose(df);
            return 0;
        }

        fclose(df);
        if (cbuild__make_deps_outdated(dep_file, obj_mtime)) {
#ifdef CBUILD_DEBUG_SIGNATURE
            fprintf(stderr, "DEBUG: Header dep changed or missing for %s\n", src_file);
#endif
            return 1;
        }
    }

    return 0;
}

static int compile_source(cbuild_context_t* ctx, const char* src_file, const char* obj_file,
                          const char* dep_file, target_t* t) {
    ensure_dir_exists(t->obj_dir);

    cbuild_argv_t argv;
    cbuild_argv_init(&argv);
    cbuild__compile_argv(ctx, t, src_file, obj_file, dep_file, &argv);

    // Print command in verbose mode before executing
    if (ctx->verbose) {
        char cmd_buf[4096];
        int pos = 0;
        for (int i = 0; i < argv.count && pos < (int)sizeof(cmd_buf) - 1; ++i) {
            if (i > 0) cmd_buf[pos++] = ' ';
            if (strchr(argv.args[i], ' ')) {
                pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - pos, "'%s'", argv.args[i]);
            } else {
                pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - pos, "%s", argv.args[i]);
            }
        }
        cmd_buf[pos] = '\0';
        cbuild__log(ctx, CBUILD_LOG_VERBOSE, "%s", cmd_buf);
    }

    int result;
    char* output = NULL;
    result = cbuild_spawn_process(ctx, &argv, 1, &output);

    /* Report failures first: the dependency scan below tokenizes output in place */
    if (output && result != 0) {
        fwrite(output, 1, strlen(output), stderr);
    }

    /* MSVC reports headers on stdout (/showIncludes). Only a successful compile
     * may replace the dependency list: a failed one stops before listing them all. */
    if (output && result == 0 && dep_file && cbuild_target_cc_kind(ctx, t) == CBUILD_CC_MSVC) {
        FILE* df = fopen(dep_file, "w");
        if (df) {
            fprintf(df, "CBUILD_MSVC_DEPS_V1\n%s\n", src_file);
            char* saveptr = NULL;
            char* line = strtok_r(output, "\r\n", &saveptr);
            /* Allow override of the include tag for non-English locales */
            const char* include_tag = getenv("CBUILD_MSVC_INCLUDE_TAG");
            if (!include_tag) include_tag = "Note: including file:";
            while (line) {
                char* pos = strstr(line, include_tag);
                if (pos) {
                    pos += strlen(include_tag);
                    while (*pos == ' ' || *pos == '\t')
                        pos++;
                    if (*pos) {
                        fprintf(df, "%s\n", pos);
                    }
                }
                line = strtok_r(NULL, "\r\n", &saveptr);
            }
            fclose(df);
        }
    }
    if (output)
        free(output);

    /* Write compile signature for change detection (only on success) */
    char sigpath[1024];
    snprintf(sigpath, sizeof(sigpath), "%s.sig", obj_file);
    if (result == 0) {
        char* sig = cbuild__compile_signature(&argv);
        FILE* sf = fopen(sigpath, "wb");
        if (sf) {
            if (sig) fwrite(sig, 1, strlen(sig), sf);
            fclose(sf);
        }
        if (sig) free(sig);
    } else {
        /* Not every compiler deletes the previous object when it fails. Left in
         * place it could pass for up to date once the error is "fixed" by
         * restoring an older file. */
        remove(obj_file);
        remove(sigpath);
    }

    cbuild_argv_free(&argv);

    if (result != 0) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Compilation failed for %s", src_file);
    }
    return result;
}

static void enqueue_compile_job(cbuild_context_t* ctx, target_t* target, int source_index) {
    if (ctx->job_count >= ctx->job_capacity) {
        ctx->job_capacity = ctx->job_capacity ? ctx->job_capacity * 2 : 32;
        ctx->job_queue = realloc(ctx->job_queue, ctx->job_capacity * sizeof(compile_job_t));
    }

    ctx->job_queue[ctx->job_count].target = target;
    ctx->job_queue[ctx->job_count].source_index = source_index;
    ctx->job_count++;
}

#ifdef _WIN32

static DWORD WINAPI compile_worker(void* arg) {
    compile_worker_arg_t* warg = (compile_worker_arg_t*)arg;
    cbuild_context_t* ctx = warg->ctx;
    while (1) {
        EnterCriticalSection(&ctx->queue_mutex);
        if (ctx->jobs_completed >= ctx->job_count || ctx->build_error) {
            LeaveCriticalSection(&ctx->queue_mutex);
            break;
        }

        int job_index = ctx->jobs_completed++;
        compile_job_t job = ctx->job_queue[job_index];
        LeaveCriticalSection(&ctx->queue_mutex);

        target_t* t = job.target;
        int i = job.source_index;
        const char* src = t->sources[i];
        const char* slash = strrchr(src, '/');
        const char* bslash = strrchr(src, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
        const char* base = slash ? slash + 1 : src;
        char* dot = strrchr(base, '.');
        size_t len = dot ? (size_t)(dot - base) : strlen(base);
        unsigned h = cbuild__hash_path(src);
        char objname[512];
        snprintf(objname, sizeof(objname), "%s/%.*s-%08x" CBUILD_OBJ_EXT, t->obj_dir, (int)len, base, h);

        char depname[512];
        snprintf(depname, sizeof(depname), "%s/%.*s-%08x" CBUILD_OBJ_EXT ".d", t->obj_dir, (int)len, base, h);

        if (need_recompile(ctx, src, objname, depname, t)) {
            cbuild__log_step(ctx, "COMPILE", CBUILD_COLOR_BLUE, "%s", src);
            if (compile_source(ctx, src, objname, depname, t) != 0) {
                EnterCriticalSection(&ctx->queue_mutex);
                ctx->build_error = 1;
                LeaveCriticalSection(&ctx->queue_mutex);
                break;
            }
            EnterCriticalSection(&ctx->queue_mutex);
            ctx->compiled_count++;
            LeaveCriticalSection(&ctx->queue_mutex);
        }
    }
    return 0;
}

#else

static void* compile_worker(void* arg) {
    compile_worker_arg_t* warg = (compile_worker_arg_t*)arg;
    cbuild_context_t* ctx = warg->ctx;
    while (1) {
        pthread_mutex_lock(&ctx->queue_mutex);
        if (ctx->jobs_completed >= ctx->job_count || ctx->build_error) {
            pthread_mutex_unlock(&ctx->queue_mutex);
            break;
        }

        int job_index = ctx->jobs_completed++;
        compile_job_t job = ctx->job_queue[job_index];
        pthread_mutex_unlock(&ctx->queue_mutex);

        target_t* t = job.target;
        int i = job.source_index;
        const char* src = t->sources[i];
        const char* slash = strrchr(src, '/');
        const char* bslash = strrchr(src, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
        const char* base = slash ? slash + 1 : src;
        char* dot = strrchr(base, '.');
        size_t len = dot ? (size_t)(dot - base) : strlen(base);
        unsigned h = cbuild__hash_path(src);
        char objname[512];
        snprintf(objname, sizeof(objname), "%s/%.*s-%08x" CBUILD_OBJ_EXT, t->obj_dir, (int)len, base, h);

        char depname[512];
        snprintf(depname, sizeof(depname), "%s/%.*s-%08x" CBUILD_OBJ_EXT ".d", t->obj_dir, (int)len, base, h);

        if (need_recompile(ctx, src, objname, depname, t)) {
            cbuild__log_step(ctx, "COMPILE", CBUILD_COLOR_BLUE, "%s", src);
            if (compile_source(ctx, src, objname, depname, t) != 0) {
                pthread_mutex_lock(&ctx->queue_mutex);
                ctx->build_error = 1;
                pthread_mutex_unlock(&ctx->queue_mutex);
                break;
            }
            pthread_mutex_lock(&ctx->queue_mutex);
            ctx->compiled_count++;
            pthread_mutex_unlock(&ctx->queue_mutex);
        }
    }
    return NULL;
}

#endif

CBUILD_INTERNAL int process_compile_jobs_parallel(cbuild_context_t* ctx, target_t* target, int* error_flag) {
    ctx->job_count = 0;
    ctx->jobs_completed = 0;
    ctx->compiled_count = 0;
    ctx->build_error = 0;

    for (int i = 0; i < target->sources_count; ++i) {
        enqueue_compile_job(ctx, target, i);
    }

    if (ctx->job_count == 0) return 0;

    compile_worker_arg_t warg;
    warg.ctx = ctx;

    int thread_count = ctx->parallel_jobs < ctx->job_count ? ctx->parallel_jobs : ctx->job_count;

#ifdef _WIN32
    InitializeCriticalSection(&ctx->queue_mutex);
#else
    pthread_mutex_init(&ctx->queue_mutex, NULL);
#endif

    ctx->threads = malloc(thread_count * sizeof(*ctx->threads));

#ifdef _WIN32
    for (int i = 0; i < thread_count; ++i) {
        ctx->threads[i] = CreateThread(NULL, 0, compile_worker, &warg, 0, NULL);
    }
#else
    for (int i = 0; i < thread_count; ++i) {
        pthread_create(&ctx->threads[i], NULL, compile_worker, &warg);
    }
#endif

#ifdef _WIN32
    WaitForMultipleObjects(thread_count, ctx->threads, TRUE, INFINITE);
    for (int i = 0; i < thread_count; ++i) {
        CloseHandle(ctx->threads[i]);
    }
    DeleteCriticalSection(&ctx->queue_mutex);
#else
    for (int i = 0; i < thread_count; ++i) {
        pthread_join(ctx->threads[i], NULL);
    }
    pthread_mutex_destroy(&ctx->queue_mutex);
#endif

    free(ctx->threads);
    ctx->threads = NULL;

    if (ctx->job_queue) {
        free(ctx->job_queue);
        ctx->job_queue = NULL;
        ctx->job_capacity = 0;
    }

    if (ctx->build_error) {
        *error_flag = 1;
    }
    return ctx->compiled_count;
}

CBUILD_INTERNAL void collect_compile_commands_for_target(cbuild_context_t* ctx, target_t* t) {
    if (!ctx->generate_compile_commands)
        return;
    /* File-dep "sources" are generator inputs, not translation units. */
    if (t->external || !t->obj_dir ||
        (t->type != TARGET_EXECUTABLE && t->type != TARGET_STATIC_LIB && t->type != TARGET_SHARED_LIB))
        return;
    cbuild__apply_config_if_needed(ctx, t);
    for (int i = 0; i < t->sources_count; ++i) {
        const char* src_file = t->sources[i];
        const char* slash = strrchr(src_file, '/');
        const char* bslash = strrchr(src_file, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
        const char* base = slash ? slash + 1 : src_file;
        char* dot = strrchr(base, '.');
        size_t len = dot ? (size_t)(dot - base) : strlen(base);
        unsigned h = cbuild__hash_path(src_file);
        char objname[512];
        snprintf(objname, sizeof(objname), "%s/%.*s-%08x" CBUILD_OBJ_EXT, t->obj_dir, (int)len, base, h);

        /* Same argv as compile_source, minus -MMD/-MF: those are for build
           system use, not semantic analysis */
        cbuild_argv_t argv;
        cbuild_argv_init(&argv);
        cbuild__compile_argv(ctx, t, src_file, objname, NULL, &argv);

        /* Convert argv to command string for compatibility */
        char* cmd = cbuild_argv_to_cmdline(&argv);

        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            if (ctx->cc_count + 1 > ctx->cc_cap) {
                ctx->cc_cap = ctx->cc_cap ? ctx->cc_cap * 2 : 4;
                ctx->cc_entries = realloc(ctx->cc_entries, ctx->cc_cap * sizeof(*ctx->cc_entries));
            }
            ctx->cc_entries[ctx->cc_count].directory = strdup(cwd);
            ctx->cc_entries[ctx->cc_count].command = cmd;

            /* Store arguments array for tools that prefer it */
            ctx->cc_entries[ctx->cc_count].argc = argv.count;
            ctx->cc_entries[ctx->cc_count].arguments = (char**)malloc(argv.count * sizeof(char*));
            for (int j = 0; j < argv.count; ++j) {
                ctx->cc_entries[ctx->cc_count].arguments[j] = strdup(argv.args[j]);
            }

            ctx->cc_entries[ctx->cc_count].file = strdup(src_file);
            ctx->cc_count++;
        } else {
            free(cmd);
        }

        cbuild_argv_free(&argv);
    }
}

/* ---- src/config.c ---- */

/* config.c - build configurations and presets. */


CBUILD_INTERNAL void cbuild__apply_config_if_needed(cbuild_context_t* ctx, target_t* t) {
    if (t->config_applied) return;
    do {
        config_t* chain[3];
        int n = 0;

        /* Find target-specific config first */
        config_t* tcfg = NULL;
        for (int i = 0; i < ctx->cfg_count; ++i) {
            if (ctx->cfg_targets[i] == t) {
                tcfg = ctx->cfg_values[i];
                break;
            }
        }

        /* Build config chain: default -> (active OR target) -> target
         * If target has its own config, skip active_config to avoid flag duplication */
        chain[n++] = ctx->default_config;
        if (tcfg) {
            /* Target has explicit config - use it instead of active config */
            chain[n++] = tcfg;
        } else {
            /* No target config - use active config as fallback */
            chain[n++] = ctx->active_config;
        }

        for (int ci = 0; ci < n; ++ci) {
            config_t* cfg = chain[ci];
            if (!cfg) continue;

            /* Apply tool overrides */
            if (cfg->compiler) {
                if (t->compiler) free(t->compiler);
                t->compiler = strdup(cfg->compiler);
                t->cc_kind_override = (int)detect_cc_kind(t->compiler);
            }
            if (cfg->linker) {
                if (t->linker) free(t->linker);
                t->linker = strdup(cfg->linker);
            }

            /* Tokenized vectors */
            for (int i = 0; i < cfg->nincludes; ++i) {
                ensure_capacity_charpp(ctx, &t->include_dirs, &t->include_count, &t->include_cap);
                t->include_dirs[t->include_count++] = strdup(cfg->includes[i]);
            }
            for (int i = 0; i < cfg->ndefines; ++i) {
                ensure_capacity_charpp(ctx, &t->defines, &t->define_count, &t->define_cap);
                t->defines[t->define_count++] = strdup(cfg->defines[i]);
            }
            for (int i = 0; i < cfg->ncflags; ++i) {
                ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                t->cflags[t->cflag_count++] = strdup(cfg->cflags[i]);
            }
            for (int i = 0; i < cfg->nldflags; ++i) {
                ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                t->ldflags[t->ldflag_count++] = strdup(cfg->ldflags[i]);
            }
            for (int i = 0; i < cfg->nlibdirs; ++i) {
                ensure_capacity_charpp(ctx, &t->lib_dirs, &t->lib_dir_count, &t->lib_dir_cap);
                t->lib_dirs[t->lib_dir_count++] = strdup(cfg->libdirs[i]);
            }
            for (int i = 0; i < cfg->nlinklibs; ++i) {
                ensure_capacity_charpp(ctx, &t->link_libs, &t->link_lib_count, &t->link_lib_cap);
                t->link_libs[t->link_lib_count++] = strdup(cfg->linklibs[i]);
            }

            /* Structured knobs → flags */
            /* Use target's cc_kind_override if set, otherwise fall back to context */
            cbuild_cc_kind_t effective_cc_kind = (t->cc_kind_override != -1)
                ? (cbuild_cc_kind_t)t->cc_kind_override : ctx->cc_kind;

            if (cfg->opt_level >= 0) {
                if (effective_cc_kind == CBUILD_CC_MSVC) {
                    const char* f = (cfg->opt_level <= 0)   ? "/Od"
                                    : (cfg->opt_level == 1) ? "/O1"
                                                            : "/O2";
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup(f);
                } else {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "-O%d", cfg->opt_level);
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup(buf);
                }
            }

            if (cfg->debug_symbols >= 0) {
                if (cfg->debug_symbols) {
                    if (effective_cc_kind == CBUILD_CC_MSVC) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("/Zi");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("/DEBUG");
                    } else {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-g");
                    }
                }
            }

            if (cfg->lto >= 0) {
                if (effective_cc_kind == CBUILD_CC_MSVC) {
                    if (cfg->lto) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("/GL");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("/LTCG");
                    }
                } else {
                    if (cfg->lto == 0) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-fno-lto");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("-fno-lto");
                    } else if (cfg->lto > 0) {
                        const char* cflag = (cfg->lto == 2) ? "-flto=thin" : "-flto";
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup(cflag);
                        const char* ldflag = (cfg->lto == 2) ? "-flto=thin" : "-flto";
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup(ldflag);
                    }
                }
            }

            if (cfg->pic >= 0) {
                if (cfg->pic && effective_cc_kind != CBUILD_CC_MSVC) {
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup("-fPIC");
                }
            }

            if (cfg->warnings >= 0) {
                if (effective_cc_kind == CBUILD_CC_MSVC) {
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup("/W4");
                } else {
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup("-Wall");
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup("-Wextra");
                    if (cfg->warnings >= 2) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-Wpedantic");
                    }
                }
            }

            if (cfg->std && *cfg->std) {
                if (effective_cc_kind == CBUILD_CC_MSVC) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "/std:%s", cfg->std);
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup(buf);
                } else {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "-std=%s", cfg->std);
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup(buf);
                }
            }

            if (cfg->runtime && *cfg->runtime) {
                if (effective_cc_kind == CBUILD_CC_MSVC) {
                    const char* f = (strcmp(cfg->runtime, "static") == 0) ? "/MT" : "/MD";
                    ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                    t->cflags[t->cflag_count++] = strdup(f);
                }
            }

            if (cfg->freestanding >= 0) {
                if (cfg->freestanding) {
                    if (effective_cc_kind == CBUILD_CC_MSVC) {
                        /* Omit default library name and default libraries */
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("/Zl");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("/NODEFAULTLIB");
                    } else {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-ffreestanding");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("-nostdlib");
                    }
                }
            }

            if (cfg->sanitize) {
                if (effective_cc_kind != CBUILD_CC_MSVC) {
                    if (cfg->sanitize & 1) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-fsanitize=address");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("-fsanitize=address");
                    }
                    if (cfg->sanitize & 2) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-fsanitize=undefined");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("-fsanitize=undefined");
                    }
                    if (cfg->sanitize & 4) {
                        ensure_capacity_charpp(ctx, &t->cflags, &t->cflag_count, &t->cflag_cap);
                        t->cflags[t->cflag_count++] = strdup("-fsanitize=thread");
                        ensure_capacity_charpp(ctx, &t->ldflags, &t->ldflag_count, &t->ldflag_cap);
                        t->ldflags[t->ldflag_count++] = strdup("-fsanitize=thread");
                    }
                }
            }
        }
    } while (0);
    t->config_applied = 1;
}

/* ---------------- Build Configuration API (implementation) ---------------- */

static void cbuild__config_add_tokens(cbuild_context_t* ctx, char*** arr, int* count, int* cap, const char* flags) {
    if (!flags || !*flags) return;
    char* copy = strdup(flags);
    if (!copy) return;

    char* p = copy;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;
        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t'))
                break;
            else
                p++;
        }
        if (p > start) {
            char saved = *p;
            *p = '\0';

            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            ensure_capacity_charpp(ctx, arr, count, cap);
            (*arr)[(*count)++] = strdup(token);

            *p = saved;
        }
    }
    free(copy);
}

config_t* cbuild_config_new(cbuild_context_t* ctx, const char* name) {
    config_t* c = (config_t*)calloc(1, sizeof(config_t));
    if (!c) return NULL;
    c->name = name ? strdup(name) : NULL;

    /* Track this config in the context for automatic cleanup */
    if (ctx) {
        if (ctx->all_configs_count >= ctx->all_configs_cap) {
            int newcap = ctx->all_configs_cap ? ctx->all_configs_cap * 2 : 4;
            config_t** newarr = (config_t**)realloc(ctx->all_configs, newcap * sizeof(config_t*));
            if (newarr) {
                ctx->all_configs = newarr;
                ctx->all_configs_cap = newcap;
            }
        }
        if (ctx->all_configs_count < ctx->all_configs_cap) {
            ctx->all_configs[ctx->all_configs_count++] = c;
        }
    }
    c->cflags = NULL;
    c->ncflags = 0;
    c->cflags_cap = 0;
    c->ldflags = NULL;
    c->nldflags = 0;
    c->ldflags_cap = 0;
    c->defines = NULL;
    c->ndefines = 0;
    c->defines_cap = 0;
    c->includes = NULL;
    c->nincludes = 0;
    c->includes_cap = 0;
    c->libdirs = NULL;
    c->nlibdirs = 0;
    c->libdirs_cap = 0;
    c->linklibs = NULL;
    c->nlinklibs = 0;
    c->linklibs_cap = 0;

    c->opt_level = -1;
    c->debug_symbols = -1;
    c->lto = -1;
    c->pic = -1;
    c->warnings = -1;
    c->output_dir = NULL;
    c->std = NULL;
    c->runtime = NULL;
    c->sanitize = 0;
    c->compiler = NULL;
    c->linker = NULL;
    return c;
}

void cbuild_config_free(cbuild_context_t* ctx, config_t* c) {
    if (!c) return;

    /* Remove from tracking array if present */
    if (ctx && ctx->all_configs) {
        for (int i = 0; i < ctx->all_configs_count; ++i) {
            if (ctx->all_configs[i] == c) {
                ctx->all_configs[i] = NULL;
                break;
            }
        }
    }
    if (c->cflags) {
        for (int i = 0; i < c->ncflags; ++i) free(c->cflags[i]);
        free(c->cflags);
    }
    if (c->ldflags) {
        for (int i = 0; i < c->nldflags; ++i) free(c->ldflags[i]);
        free(c->ldflags);
    }
    if (c->defines) {
        for (int i = 0; i < c->ndefines; ++i) free(c->defines[i]);
        free(c->defines);
    }
    if (c->includes) {
        for (int i = 0; i < c->nincludes; ++i) free(c->includes[i]);
        free(c->includes);
    }
    if (c->libdirs) {
        for (int i = 0; i < c->nlibdirs; ++i) free(c->libdirs[i]);
        free(c->libdirs);
    }
    if (c->linklibs) {
        for (int i = 0; i < c->nlinklibs; ++i) free(c->linklibs[i]);
        free(c->linklibs);
    }
    if (c->name) free((void*)c->name);
    if (c->output_dir) free((void*)c->output_dir);
    if (c->std) free((void*)c->std);
    if (c->runtime) free((void*)c->runtime);
    if (c->compiler) free((void*)c->compiler);
    if (c->linker) free((void*)c->linker);
    free(c);
}

void cbuild_config_add_cflags(cbuild_context_t* ctx, config_t* c, const char* flags) {
    if (!c) return;
    cbuild__config_add_tokens(ctx, &c->cflags, &c->ncflags, &c->cflags_cap, flags);
}

void cbuild_config_add_ldflags(cbuild_context_t* ctx, config_t* c, const char* flags) {
    if (!c) return;
    cbuild__config_add_tokens(ctx, &c->ldflags, &c->nldflags, &c->ldflags_cap, flags);
}

void cbuild_config_add_define(cbuild_context_t* ctx, config_t* c, const char* def) {
    (void)ctx;
    if (!c || !def) return;
    ensure_capacity_charpp(ctx, &c->defines, &c->ndefines, &c->defines_cap);
    c->defines[c->ndefines++] = strdup(def);
}

void cbuild_config_add_include(cbuild_context_t* ctx, config_t* c, const char* dir) {
    (void)ctx;
    if (!c || !dir) return;
    ensure_capacity_charpp(ctx, &c->includes, &c->nincludes, &c->includes_cap);
    /* Normalize path separators for consistent path handling on Windows */
    c->includes[c->nincludes++] = cbuild__normalize_path(dir);
}

void cbuild_config_add_libdir(cbuild_context_t* ctx, config_t* c, const char* dir) {
    (void)ctx;
    if (!c || !dir) return;
    ensure_capacity_charpp(ctx, &c->libdirs, &c->nlibdirs, &c->libdirs_cap);
    /* Normalize path separators for consistent path handling on Windows */
    c->libdirs[c->nlibdirs++] = cbuild__normalize_path(dir);
}

void cbuild_config_add_linklib(cbuild_context_t* ctx, config_t* c, const char* lib) {
    (void)ctx;
    if (!c || !lib) return;
    ensure_capacity_charpp(ctx, &c->linklibs, &c->nlinklibs, &c->linklibs_cap);
    /* Normalize path separators for consistent path handling on Windows */
    c->linklibs[c->nlinklibs++] = cbuild__normalize_path(lib);
}

void cbuild_config_set_opt(cbuild_context_t* ctx, config_t* c, int level) {
    (void)ctx;
    if (!c) return;
    c->opt_level = level;
}

void cbuild_config_set_debug(cbuild_context_t* ctx, config_t* c, int on) {
    (void)ctx;
    if (!c) return;
    c->debug_symbols = on ? 1 : 0;
}

void cbuild_config_set_lto(cbuild_context_t* ctx, config_t* c, int mode) {
    (void)ctx;
    if (!c) return;
    c->lto = mode;
}

void cbuild_config_set_pic(cbuild_context_t* ctx, config_t* c, int on) {
    (void)ctx;
    if (!c) return;
    c->pic = on ? 1 : 0;
}

void cbuild_config_set_warnings(cbuild_context_t* ctx, config_t* c, int mode) {
    (void)ctx;
    if (!c) return;
    c->warnings = mode;
}

void cbuild_config_set_std(cbuild_context_t* ctx, config_t* c, const char* std) {
    (void)ctx;
    if (!c) return;
    if (c->std) {
        free((void*)c->std);
        c->std = NULL;
    }
    c->std = std ? strdup(std) : NULL;
}

void cbuild_config_set_runtime(cbuild_context_t* ctx, config_t* c, const char* runtime) {
    (void)ctx;
    if (!c) return;
    if (c->runtime) {
        free((void*)c->runtime);
        c->runtime = NULL;
    }
    c->runtime = runtime ? strdup(runtime) : NULL;
}

void cbuild_config_set_output_dir(cbuild_context_t* ctx, config_t* c, const char* output_dir) {
    (void)ctx;
    if (!c) return;
    if (c->output_dir) {
        free((void*)c->output_dir);
        c->output_dir = NULL;
    }
    /* Normalize path separators for consistent path handling on Windows */
    c->output_dir = output_dir ? cbuild__normalize_path(output_dir) : NULL;
}

void cbuild_config_enable_sanitizers(cbuild_context_t* ctx, config_t* c, int mask) {
    (void)ctx;
    if (!c) return;
    c->sanitize |= mask;
}

void cbuild_config_disable_sanitizers(cbuild_context_t* ctx, config_t* c, int mask) {
    (void)ctx;
    if (!c) return;
    c->sanitize &= ~mask;
}

void cbuild_config_set_compiler(cbuild_context_t* ctx, config_t* c, const char* compiler) {
    (void)ctx;
    if (!c) return;
    if (c->compiler) {
        free((void*)c->compiler);
        c->compiler = NULL;
    }
    c->compiler = compiler ? strdup(compiler) : NULL;
}

void cbuild_config_set_linker(cbuild_context_t* ctx, config_t* c, const char* linker) {
    (void)ctx;
    if (!c) return;
    if (c->linker) {
        free((void*)c->linker);
        c->linker = NULL;
    }
    c->linker = linker ? strdup(linker) : NULL;
}

void cbuild_set_active_config(cbuild_context_t* ctx, config_t* c) {
    ctx->active_config = c;
}

void cbuild_target_set_config(cbuild_context_t* ctx, target_t* t, config_t* c) {
    if (!t) return;
    for (int i = 0; i < ctx->cfg_count; ++i) {
        if (ctx->cfg_targets[i] == t) {
            ctx->cfg_values[i] = c;
            return;
        }
    }
    if (ctx->cfg_count + 1 > ctx->cfg_cap) {
        ctx->cfg_cap = ctx->cfg_cap ? ctx->cfg_cap * 2 : 8;
        ctx->cfg_targets = (target_t**)realloc(ctx->cfg_targets, ctx->cfg_cap * sizeof(*ctx->cfg_targets));
        ctx->cfg_values = (config_t**)realloc(ctx->cfg_values, ctx->cfg_cap * sizeof(*ctx->cfg_values));
    }
    ctx->cfg_targets[ctx->cfg_count] = t;
    ctx->cfg_values[ctx->cfg_count] = c;
    ctx->cfg_count++;
}

/* ---------------- Pre-baked platform-aware standard configs ---------------- */

config_t* cbuild_config_default_debug(cbuild_context_t* ctx) {
    config_t* c = cbuild_config_new(ctx, "Debug");
    if (!c) return NULL;

    cbuild_config_set_output_dir(ctx, c, "build/debug");
    cbuild_config_set_std(ctx, c, "c23");
    cbuild_config_set_warnings(ctx, c, 2); /* -Wall -Wextra -Wpedantic or /W4 */
    cbuild_config_set_opt(ctx, c, 0);
    cbuild_config_set_debug(ctx, c, 1);

#ifdef _WIN32
    /* MSVC-specific niceties */
    cbuild_config_add_cflags(ctx, c, "/nologo");
    cbuild_config_add_cflags(ctx, c, "/WX");      /* treat warnings as errors */
    cbuild_config_set_runtime(ctx, c, "dynamic"); /* /MD by default */
#else
    /* GCC/Clang on Linux/macOS */
    cbuild_config_enable_sanitizers(ctx, c, 1 | 2); /* ASAN | UBSAN */
    cbuild_config_add_cflags(ctx, c, "-Werror");    /* treat warnings as errors */
#endif
    return c;
}

config_t* cbuild_config_default_release(cbuild_context_t* ctx) {
    config_t* c = cbuild_config_new(ctx, "Release");
    if (!c) return NULL;

    cbuild_config_set_output_dir(ctx, c, "build/release");
    cbuild_config_set_std(ctx, c, "c23");
    cbuild_config_set_warnings(ctx, c, 2); /* -Wall -Wextra -Wpedantic or /W4 */
    cbuild_config_set_opt(ctx, c, 3);
    cbuild_config_set_debug(ctx, c, 0);

#ifdef _WIN32
    /* MSVC-specific niceties */
    cbuild_config_add_cflags(ctx, c, "/nologo");
    cbuild_config_add_cflags(ctx, c, "/WX");      /* treat warnings as errors */
    cbuild_config_set_runtime(ctx, c, "dynamic"); /* /MD by default */
#else
    /* GCC/Clang on Linux/macOS */
    cbuild_config_add_cflags(ctx, c, "-Werror"); /* treat warnings as errors */
#endif
    return c;
}

config_t* cbuild_config_default_wasm32(cbuild_context_t* ctx) {
    config_t* c = cbuild_config_new(ctx, "WASM32");
    if (!c) return NULL;

    cbuild_config_set_std(ctx, c, "c23");
    cbuild_config_set_warnings(ctx, c, 2); /* -Wall -Wextra -Wpedantic or /W4 */
    cbuild_config_set_opt(ctx, c, 3);
    cbuild_config_set_debug(ctx, c, 0);

    cbuild_config_add_cflags(ctx, c, "--target=wasm32");
    cbuild_config_add_ldflags(ctx, c, "--target=wasm32");
    cbuild_config_add_ldflags(ctx, c, "-nostdlib");

    return c;
}

void cbuild_config_set_freestanding(cbuild_context_t* ctx, config_t* c, int on) {
    (void)ctx;
    if (!c) return;
    c->freestanding = on ? 1 : 0;
}

/* ---- src/context.c ---- */

/* context.c - context lifecycle and global settings. */


void cbuild_set_user_data(cbuild_context_t* ctx, void* data) {
    if (ctx) ctx->user_data = data;
}

void* cbuild_get_user_data(cbuild_context_t* ctx) {
    return ctx ? ctx->user_data : NULL;
}

/* Context constructor - allocates and initializes a new build context */
cbuild_context_t* cbuild_context_new(void) {
    cbuild_context_t* ctx = (cbuild_context_t*)calloc(1, sizeof(cbuild_context_t));
    if (!ctx) return NULL;

    /* Initialize with defaults */
    ctx->cc_kind = CBUILD_CC_GCC_CLANG;
    ctx->argv0_for_help = "cbuild";

#ifndef _WIN32
    pthread_mutex_init(&ctx->queue_mutex, NULL);
#endif

    return ctx;
}

/* Context destructor - frees all resources owned by the context */
void cbuild_context_free(cbuild_context_t* ctx) {
    if (!ctx) return;

    /* Use cbuild_teardown to free all internal state without reinitializing */
    cbuild_teardown(ctx);

#ifndef _WIN32
    pthread_mutex_destroy(&ctx->queue_mutex);
#endif

    /* Free the context itself */
    free(ctx);
}

void cbuild_enable_compile_commands(cbuild_context_t* ctx, int enabled) {
    ctx->generate_compile_commands = enabled;
}

void cbuild_set_verbose(cbuild_context_t* ctx, int verbose) {
    ctx->verbose = verbose;
}

void cbuild_set_output_dir(cbuild_context_t* ctx, const char* dir) {
    if (ctx->output_dir) {
        free(ctx->output_dir);
    }
    /* Normalize path separators for consistent path handling on Windows */
    ctx->output_dir = cbuild__normalize_path(dir);
}

void cbuild_set_parallelism(cbuild_context_t* ctx, int jobs_count) {
    ctx->parallel_jobs = jobs_count;
}

void cbuild_set_compiler(cbuild_context_t* ctx, const char* compiler_exe) {
    if (ctx->cc) free(ctx->cc);
    ctx->cc = strdup(compiler_exe);
    ctx->cc_kind = detect_cc_kind(compiler_exe);

    if (ctx->ar) free(ctx->ar);
    if (ctx->cc_kind == CBUILD_CC_MSVC) {
        ctx->ar = strdup("lib");
    } else {
        ctx->ar = strdup("ar");
    }

    if (ctx->ld) free(ctx->ld);
    if (ctx->cc_kind == CBUILD_CC_MSVC) {
        ctx->ld = strdup("cl");
    } else {
        ctx->ld = strdup(ctx->cc);
    }
}

void cbuild_set_linker(cbuild_context_t* ctx, const char* linker_exe) {
    if (ctx->ld) free(ctx->ld);
    ctx->ld = strdup(linker_exe);
}

void cbuild_set_archiver(cbuild_context_t* ctx, const char* archiver_exe) {
    if (ctx->ar) free(ctx->ar);
    ctx->ar = strdup(archiver_exe);
}

void cbuild_set_build_type(cbuild_context_t* ctx, const char* build_type) {
    if (!build_type) return;

    // Detect compiler kind if not already set
    if (ctx->cc == NULL) {
        ctx->cc_kind = detect_cc_kind(NULL);
    }

    if (strcmp(build_type, "Debug") == 0) {
        if (ctx->cc_kind == CBUILD_CC_MSVC) {
            // MSVC Debug: /Zi (debug info), /Od (no optimization), /RTC1 (runtime checks)
            cbuild_add_global_cflags(ctx, "/Zi /Od /RTC1");
            cbuild_add_global_ldflags(ctx, "/DEBUG");
        } else {
            // GCC/Clang Debug: -g (debug info), -O0 (no optimization)
            cbuild_add_global_cflags(ctx, "-g -O0");
        }
    } else if (strcmp(build_type, "Release") == 0) {
        if (ctx->cc_kind == CBUILD_CC_MSVC) {
            // MSVC Release: /O2 (optimize for speed), /DNDEBUG
            cbuild_add_global_cflags(ctx, "/O2");
            cbuild_add_global_define(ctx, "NDEBUG");
        } else {
            // GCC/Clang Release: -O3 (aggressive optimization), -DNDEBUG
            cbuild_add_global_cflags(ctx, "-O3");
            cbuild_add_global_define(ctx, "NDEBUG");
        }
    }
    // If build_type is neither "Debug" nor "Release", do nothing
    // This allows users to pass empty string or custom values without error
}

void cbuild_add_global_cflags(cbuild_context_t* ctx, const char* flags) {
    if (!flags) return;
    char* copy = strdup(flags);
    if (!copy) return;

    char* p = copy;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;
        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t')) {
                break;
            } else {
                p++;
            }
        }

        if (p > start) {
            char saved = *p;
            *p = '\0';

            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            ensure_capacity_charpp(ctx, &ctx->global_cflags, &ctx->global_cflag_count, &ctx->global_cflag_cap);
            ctx->global_cflags[ctx->global_cflag_count++] = strdup(token);

            *p = saved;
        }
    }

    free(copy);
}

void cbuild_add_global_ldflags(cbuild_context_t* ctx, const char* flags) {
    if (!flags) return;
    char* copy = strdup(flags);
    if (!copy) return;

    char* p = copy;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;
        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t')) {
                break;
            } else {
                p++;
            }
        }

        if (p > start) {
            char saved = *p;
            *p = '\0';

            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            ensure_capacity_charpp(ctx, &ctx->global_ldflags, &ctx->global_ldflag_count, &ctx->global_ldflag_cap);
            ctx->global_ldflags[ctx->global_ldflag_count++] = strdup(token);

            *p = saved;
        }
    }

    free(copy);
}

void cbuild_add_global_define(cbuild_context_t* ctx, const char* macro) {
    if (macro)
        cbuild__add_define_to_list(ctx, &ctx->global_defines, &ctx->global_def_count,
                                   &ctx->global_def_cap, macro, NULL);
}

void cbuild_add_global_define_val(cbuild_context_t* ctx, const char* macro, const char* val) {
    if (macro)
        cbuild__add_define_to_list(ctx, &ctx->global_defines, &ctx->global_def_count,
                                   &ctx->global_def_cap, macro, val);
}

void cbuild_add_global_flag(cbuild_context_t* ctx, const char* flag, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", !!value);
    cbuild_add_global_define_val(ctx, flag, buf);
}

CBUILD_INTERNAL void cbuild_init(cbuild_context_t* ctx) {
    if (!ctx->output_dir)
        ctx->output_dir = strdup("build");

    if (!ctx->cc) {
#ifdef _WIN32
        ctx->cc = strdup("cl");
        ctx->cc_kind = CBUILD_CC_MSVC;
#else
        ctx->cc = strdup("cc");
        ctx->cc_kind = CBUILD_CC_GCC_CLANG;
#endif
    } else {
        ctx->cc_kind = detect_cc_kind(ctx->cc);
    }

    if (!ctx->ar) ctx->ar = strdup(ctx->cc_kind == CBUILD_CC_MSVC ? "lib" : "ar");
    if (!ctx->ld) ctx->ld = strdup(ctx->cc_kind == CBUILD_CC_MSVC ? "cl" : ctx->cc);

    if (ctx->parallel_jobs <= 0) {
        int n = 1;
#ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        n = sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
        int logical_cpus = 0;
        size_t logical_cpus_size = sizeof(logical_cpus);
        if (sysctlbyname("hw.logicalcpu", &logical_cpus, &logical_cpus_size,
                         NULL, 0) == 0 && logical_cpus > 0) {
            n = logical_cpus;
        }
#else
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpus > 0)
            n = (int)cpus;
#endif
        ctx->parallel_jobs = n > 0 ? n : 1;
    }
}

/* Internal teardown - frees all resources, leaves ctx in zeroed state */
void cbuild_teardown(cbuild_context_t* ctx) {
    // Free DFS state
    if (ctx->visited) {
        free(ctx->visited);
        ctx->visited = NULL;
    }
    if (ctx->in_stack) {
        free(ctx->in_stack);
        ctx->in_stack = NULL;
    }

    // Free targets
    if (ctx->targets) {
        for (int i = 0; i < ctx->target_count; ++i) {
            target_t* t = ctx->targets[i];
            if (!t) continue;
            if (t->name) free(t->name);

            if (t->sources) {
                for (int j = 0; j < t->sources_count; ++j) free(t->sources[j]);
                free(t->sources);
            }
            if (t->include_dirs) {
                for (int j = 0; j < t->include_count; ++j) free(t->include_dirs[j]);
                free(t->include_dirs);
            }
            if (t->lib_dirs) {
                for (int j = 0; j < t->lib_dir_count; ++j) free(t->lib_dirs[j]);
                free(t->lib_dirs);
            }
            if (t->link_libs) {
                for (int j = 0; j < t->link_lib_count; ++j) free(t->link_libs[j]);
                free(t->link_libs);
            }
            if (t->exposed_libs) {
                for (int j = 0; j < t->exposed_lib_count; ++j) free(t->exposed_libs[j]);
                free(t->exposed_libs);
            }
            if (t->dependencies) free(t->dependencies);
            if (t->cflags) {
                for (int j = 0; j < t->cflag_count; ++j) free(t->cflags[j]);
                free(t->cflags);
            }
            if (t->ldflags) {
                for (int j = 0; j < t->ldflag_count; ++j) free(t->ldflags[j]);
                free(t->ldflags);
            }
            if (t->output_file) free(t->output_file);
            if (t->obj_dir) free(t->obj_dir);
            if (t->commands) free(t->commands);
            if (t->post_commands) free(t->post_commands);
            if (t->defines) {
                for (int j = 0; j < t->define_count; ++j) free(t->defines[j]);
                free(t->defines);
            }
            if (t->soname) free(t->soname);
            if (t->compiler) free(t->compiler);
            if (t->linker) free(t->linker);
            free(t);
        }
        free(ctx->targets);
    }
    ctx->targets = NULL;
    ctx->target_count = 0;
    ctx->target_cap = 0;

    // Free commands
    if (ctx->commands) {
        for (int i = 0; i < ctx->command_count; ++i) {
            command_t* c = ctx->commands[i];
            if (!c) continue;
            if (c->name) free(c->name);
            if (c->command_line) free(c->command_line);
            if (c->argv) {
                for (int j = 0; j < c->argc; ++j) free(c->argv[j]);
                free(c->argv);
            }
            if (c->dependencies) free(c->dependencies);
            free(c);
        }
        free(ctx->commands);
    }
    ctx->commands = NULL;
    ctx->command_count = 0;
    ctx->command_cap = 0;

    // Free subcommands
    if (ctx->subcommands) {
        for (int i = 0; i < ctx->subcommand_count; ++i) {
            cbuild_subcommand_t* sc = ctx->subcommands[i];
            if (!sc) continue;
            if (sc->name) free(sc->name);
            if (sc->command_line) free(sc->command_line);
            // sc->target and sc->user_data are not owned
            free(sc);
        }
        free(ctx->subcommands);
    }
    ctx->subcommands = NULL;
    ctx->subcommand_count = 0;
    ctx->subcommand_cap = 0;

    cbuild__free_tests(ctx);

    // Free subprojects
    if (ctx->subprojects) {
        for (int i = 0; i < ctx->subproject_count; ++i) {
            subproject_t* sp = ctx->subprojects[i];
            if (!sp) continue;
            if (sp->alias) free(sp->alias);
            if (sp->directory) free(sp->directory);
            if (sp->cbuild_exe) free(sp->cbuild_exe);
            if (sp->targets) {
                for (int j = 0; j < sp->target_count; ++j) {
                    if (sp->targets[j].name) free(sp->targets[j].name);
                    if (sp->targets[j].type) free(sp->targets[j].type);
                    if (sp->targets[j].output_path) free(sp->targets[j].output_path);
                    // proxy_target is a normal target and freed with ctx->targets
                }
                free(sp->targets);
            }
            // build_cmd is a command_t managed by ctx->commands
            free(sp);
        }
        free(ctx->subprojects);
    }
    ctx->subprojects = NULL;
    ctx->subproject_count = 0;
    ctx->subproject_cap = 0;

    // Free global defines
    if (ctx->global_defines) {
        for (int i = 0; i < ctx->global_def_count; ++i) free(ctx->global_defines[i]);
        free(ctx->global_defines);
    }
    ctx->global_defines = NULL;
    ctx->global_def_count = 0;
    ctx->global_def_cap = 0;

    // Free target filters
    if (ctx->target_filters) {
        for (int i = 0; i < ctx->target_filter_count; ++i) free(ctx->target_filters[i]);
        free(ctx->target_filters);
    }
    ctx->target_filters = NULL;
    ctx->target_filter_count = 0;
    ctx->target_filter_cap = 0;

    // Free compile_commands entries
    if (ctx->cc_entries) {
        for (int i = 0; i < ctx->cc_count; ++i) {
            if (ctx->cc_entries[i].directory) free(ctx->cc_entries[i].directory);
            if (ctx->cc_entries[i].command) free(ctx->cc_entries[i].command);
            if (ctx->cc_entries[i].file) free(ctx->cc_entries[i].file);
            if (ctx->cc_entries[i].arguments) {
                for (int j = 0; j < ctx->cc_entries[i].argc; ++j) {
                    free(ctx->cc_entries[i].arguments[j]);
                }
                free(ctx->cc_entries[i].arguments);
            }
        }
        free(ctx->cc_entries);
    }
    ctx->cc_entries = NULL;
    ctx->cc_count = 0;
    ctx->cc_cap = 0;
    ctx->generate_compile_commands = 0;

    // Free job queue and reset build state
    if (ctx->job_queue) {
        free(ctx->job_queue);
    }
    ctx->job_queue = NULL;
    ctx->job_capacity = 0;
    ctx->job_count = 0;
    ctx->jobs_completed = 0;
    ctx->compiled_count = 0;
    ctx->build_error = 0;

    // Free flag handlers
    if (ctx->flag_handlers) {
        for (int i = 0; i < ctx->flag_count; ++i) {
            if (ctx->flag_handlers[i].long_name) free(ctx->flag_handlers[i].long_name);
            if (ctx->flag_handlers[i].help) free(ctx->flag_handlers[i].help);
            if (ctx->flag_handlers[i].allocated_str) free(ctx->flag_handlers[i].allocated_str);
        }
        free(ctx->flag_handlers);
    }
    ctx->flag_handlers = NULL;
    ctx->flag_count = 0;
    ctx->flag_cap = 0;

    // Reset CLI/run helpers
    ctx->run_subcmd = NULL;
    ctx->argv0_for_help = "cbuild";

    // Free and reset global build settings
    if (ctx->output_dir) {
        free(ctx->output_dir);
        ctx->output_dir = NULL;
    }
    if (ctx->cc) {
        free(ctx->cc);
        ctx->cc = NULL;
    }
    if (ctx->ar) {
        free(ctx->ar);
        ctx->ar = NULL;
    }
    if (ctx->ld) {
        free(ctx->ld);
        ctx->ld = NULL;
    }
    if (ctx->global_cflags) {
        for (int i = 0; i < ctx->global_cflag_count; ++i) free(ctx->global_cflags[i]);
        free(ctx->global_cflags);
        ctx->global_cflags = NULL;
    }
    ctx->global_cflag_count = 0;
    ctx->global_cflag_cap = 0;
    if (ctx->global_ldflags) {
        for (int i = 0; i < ctx->global_ldflag_count; ++i) free(ctx->global_ldflags[i]);
        free(ctx->global_ldflags);
        ctx->global_ldflags = NULL;
    }
    ctx->global_ldflag_count = 0;
    ctx->global_ldflag_cap = 0;
    ctx->parallel_jobs = 0;
    ctx->cc_kind = CBUILD_CC_GCC_CLANG;
    ctx->verbose = 0;

    /* Free config mapping */
    if (ctx->cfg_targets) {
        free(ctx->cfg_targets);
        ctx->cfg_targets = NULL;
    }
    if (ctx->cfg_values) {
        free(ctx->cfg_values);
        ctx->cfg_values = NULL;
    }
    ctx->cfg_count = 0;
    ctx->cfg_cap = 0;
    ctx->active_config = NULL;

    /* Free all tracked configs (includes default_config if it was created via cbuild_config_new) */
    if (ctx->all_configs) {
        for (int i = 0; i < ctx->all_configs_count; ++i) {
            if (ctx->all_configs[i]) {
                cbuild_config_free(ctx, ctx->all_configs[i]);
            }
        }
        free(ctx->all_configs);
        ctx->all_configs = NULL;
    }
    ctx->all_configs_count = 0;
    ctx->all_configs_cap = 0;

    /* Configurations are tracked in all_configs and were released above. */
    ctx->default_config = NULL;

    // Clear error state
    ctx->last_error[0] = '\0';
}

/* Public reset - tears down and reinitializes for reuse */
void cbuild_reset(cbuild_context_t* ctx) {
    cbuild_teardown(ctx);
    // Reinitialize defaults so a fresh graph can be constructed
    cbuild_init(ctx);
}

/* ---- src/flags.c ---- */

/* flags.c - flag registration and dispatch. */


static void cbuild__register_flag(cbuild_context_t* ctx, cbuild_flag_handler_t fh) {
    if (ctx->flag_count + 1 > ctx->flag_cap) {
        ctx->flag_cap = ctx->flag_cap ? ctx->flag_cap * 2 : 8;
        ctx->flag_handlers = (cbuild_flag_handler_t*)realloc(
            ctx->flag_handlers, ctx->flag_cap * sizeof(*ctx->flag_handlers));
    }
    ctx->flag_handlers[ctx->flag_count++] = fh;
}

void cbuild_register_flag(cbuild_context_t* ctx, const char* long_name, char short_name, int takes_value,
                          cbuild_flag_phase_t phase, const char* help,
                          cbuild_flag_callback cb, void* user_data) {
    cbuild_flag_handler_t fh = (cbuild_flag_handler_t){ 0 };
    if (long_name) fh.long_name = strdup(long_name);
    fh.short_name = short_name;
    fh.takes_value = takes_value ? 1 : 0;
    fh.phase = phase;
    fh.help = help ? strdup(help) : NULL;
    fh.cb = cb;
    fh.user_data = user_data;
    cbuild__register_flag(ctx, fh);
}

void cbuild_register_flag_bool(cbuild_context_t* ctx, const char* long_name, char short_name,
                               cbuild_flag_phase_t phase, const char* help,
                               int* out_bool) {
    cbuild_flag_handler_t fh = (cbuild_flag_handler_t){ 0 };
    if (long_name) fh.long_name = strdup(long_name);
    fh.short_name = short_name;
    fh.phase = phase;
    fh.help = help ? strdup(help) : NULL;
    fh.bind_bool = out_bool;
    cbuild__register_flag(ctx, fh);
}

void cbuild_register_flag_int(cbuild_context_t* ctx, const char* long_name, char short_name,
                              cbuild_flag_phase_t phase, const char* help,
                              int* out_int) {
    cbuild_flag_handler_t fh = (cbuild_flag_handler_t){ 0 };
    if (long_name) fh.long_name = strdup(long_name);
    fh.short_name = short_name;
    fh.takes_value = 1;
    fh.phase = phase;
    fh.help = help ? strdup(help) : NULL;
    fh.bind_int = out_int;
    cbuild__register_flag(ctx, fh);
}

void cbuild_register_flag_str(cbuild_context_t* ctx, const char* long_name, char short_name,
                              cbuild_flag_phase_t phase, const char* help,
                              const char** out_str) {
    cbuild_flag_handler_t fh = (cbuild_flag_handler_t){ 0 };
    if (long_name) fh.long_name = strdup(long_name);
    fh.short_name = short_name;
    fh.takes_value = 1;
    fh.phase = phase;
    fh.help = help ? strdup(help) : NULL;
    fh.bind_str = out_str;
    cbuild__register_flag(ctx, fh);
}

/* Check if a flag is present in argv (for early/pre-cbuild_run parsing) */
int cbuild_has_flag(int argc, char** argv, const char* long_name, char short_name) {
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (!arg) continue;

        /* Check --long_name */
        if (long_name && arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg + 2, long_name) == 0) return 1;
        }
        /* Check -X short flag */
        if (short_name && arg[0] == '-' && arg[1] != '-') {
            /* Could be -s or -abc (bundled) */
            for (int j = 1; arg[j]; j++) {
                if (arg[j] == short_name) return 1;
            }
        }
    }
    return 0;
}

const char* cbuild_get_flag_value(int argc, char** argv, const char* long_name, char short_name) {
    const char* value = NULL;
    size_t long_len = long_name ? strlen(long_name) : 0;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (!arg || arg[0] != '-') continue;
        if (strcmp(arg, "--") == 0) break;

        if (arg[1] == '-') {
            if (!long_name || strncmp(arg + 2, long_name, long_len) != 0) continue;
            const char* rest = arg + 2 + long_len;
            if (*rest == '=') {
                value = rest + 1;
            } else if (*rest == '\0' && i + 1 < argc) {
                value = argv[++i];
            }
        } else if (short_name && arg[1] == short_name) {
            if (arg[2]) {
                value = arg + 2;
            } else if (i + 1 < argc) {
                value = argv[++i];
            }
        }
    }
    return value;
}

CBUILD_INTERNAL void cbuild__print_help(cbuild_context_t* ctx) {
    /* One log call per line: the logger formats into a fixed-size buffer. */
    cbuild__log(ctx, CBUILD_LOG_INFO, "Usage: %s [flags] [subcommand]\n\nFlags:", ctx->argv0_for_help);
    const char* phase_names[] = { "Pre-parse", "Before build", "After build" };
    for (int ph = CBUILD_FLAG_PRE; ph <= CBUILD_FLAG_AFTER_BUILD; ++ph) {
        int header = 0;
        for (int i = 0; i < ctx->flag_count; ++i) {
            cbuild_flag_handler_t* fh = &ctx->flag_handlers[i];
            if (fh->phase != (cbuild_flag_phase_t)ph) continue;
            if (!header) {
                cbuild__log(ctx, CBUILD_LOG_INFO, "\n  %s:", phase_names[ph]);
                header = 1;
            }
            char short_part[8] = "";
            if (fh->short_name) snprintf(short_part, sizeof(short_part), "-%c%s", fh->short_name,
                                         fh->long_name ? ", " : "");
            cbuild__log(ctx, CBUILD_LOG_INFO, "    %s%s%s%s", short_part, fh->long_name ? "--" : "",
                        fh->long_name ? fh->long_name : "", fh->takes_value ? " <val>" : "");
            if (fh->help) cbuild__log(ctx, CBUILD_LOG_INFO, "        %s", fh->help);
        }
    }
    if (ctx->subcommand_count > 0) {
        cbuild__log(ctx, CBUILD_LOG_INFO, "\nSubcommands:");
        for (int i = 0; i < ctx->subcommand_count; ++i) {
            cbuild__log(ctx, CBUILD_LOG_INFO, "    %s", ctx->subcommands[i]->name);
        }
    }
}

/* The handler registered for a flag token in any phase, or NULL. For a short
   cluster, the first letter decides. */
static cbuild_flag_handler_t* cbuild__find_any_phase(cbuild_context_t* ctx, const char* arg) {
    if (arg[1] == '-') {
        const char* name = arg + 2;
        size_t n = strcspn(name, "=");
        for (int i = 0; i < ctx->flag_count; ++i) {
            cbuild_flag_handler_t* fh = &ctx->flag_handlers[i];
            if (fh->long_name && strlen(fh->long_name) == n && strncmp(fh->long_name, name, n) == 0) return fh;
        }
        return NULL;
    }
    for (int i = 0; i < ctx->flag_count; ++i) {
        if (ctx->flag_handlers[i].short_name == arg[1]) return &ctx->flag_handlers[i];
    }
    return NULL;
}

/* Whether a flag token takes the following argument as its value. */
static int cbuild__flag_uses_next(cbuild_flag_handler_t* fh, const char* arg) {
    if (!fh->takes_value) return 0;
    if (arg[1] == '-') return strchr(arg, '=') == NULL;
    return arg[2] == '\0';
}

/* Counts the arguments that are not flags (or flag values) left for a later
   phase, storing the index of the first in *first (-1 if none). */
CBUILD_INTERNAL int cbuild__positional_args(cbuild_context_t* ctx, int argc, char** argv, int* first) {
    int count = 0;
    int stop = 0;
    *first = -1;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!arg) continue;
        if (!stop && strcmp(arg, "--") == 0) {
            stop = 1;
            continue;
        }
        if (!stop && arg[0] == '-' && arg[1] != '\0') {
            cbuild_flag_handler_t* fh = cbuild__find_any_phase(ctx, arg);
            if (fh && cbuild__flag_uses_next(fh, arg)) i++;
            continue;
        }
        if (*first < 0) *first = i;
        count++;
    }
    return count;
}

/* strict dispatcher:
   - consumes handled flags from argv (compacting)
   - leaves flags registered for another phase, and their values, in place
   - errors on any unknown token that *looks* like a flag (- or --)
   - honors "--" to stop flag parsing (and keeps it for later phases)
*/
CBUILD_INTERNAL int cbuild_dispatch_flags_strict(cbuild_context_t* ctx, cbuild_flag_phase_t phase,
                                                 int* argc, char*** argvp) {
    int ac = *argc;
    char** av = *argvp;
    int write = 1;
    int stop = 0;

    for (int read = 1; read < ac; ++read) {
        const char* arg = av[read];

        if (stop || !arg || arg[0] != '-' || arg[1] == '\0') {
            av[write++] = av[read];
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            av[write++] = av[read];
            stop = 1;
            continue;
        }

        /* Another phase's flag waits for that phase's dispatch. */
        cbuild_flag_handler_t* other = cbuild__find_any_phase(ctx, arg);
        if (other && other->phase != phase) {
            av[write++] = av[read];
            if (cbuild__flag_uses_next(other, arg) && read + 1 < ac) av[write++] = av[++read];
            continue;
        }

        int handled = 0;
        const char* val = NULL;
        int consume_next = 0;

        if (arg[1] == '-') {
            /* --long or --long=value */
            const char* name = arg + 2;
            const char* eq = strchr(name, '=');
            char namebuf[256];
            if (eq) {
                size_t n = (size_t)(eq - name);
                    if (n >= sizeof(namebuf)) n = sizeof(namebuf) - 1;
                    memcpy(namebuf, name, n);
                    namebuf[n] = 0;
                    val = eq + 1;
                    name = namebuf;
                }
                for (int i = 0; i < ctx->flag_count; ++i) {
                    cbuild_flag_handler_t* fh = &ctx->flag_handlers[i];
                if (fh->phase != phase) continue;
                if (!fh->long_name || strcmp(fh->long_name, name) != 0) continue;

                if (fh->takes_value && !val) {
                    if (read + 1 < ac) {
                        val = av[read + 1];
                        consume_next = 1;
                    } else {
                        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: --%s requires a value", name);
                        return 1;
                    }
                }

                if (fh->bind_bool) *fh->bind_bool = 1;
                if (fh->bind_int && val) *fh->bind_int = atoi(val);
                if (fh->bind_str && val) {
                    /* Free any previously allocated string */
                    if (fh->allocated_str) free(fh->allocated_str);
                    fh->allocated_str = strdup(val);
                    *fh->bind_str = fh->allocated_str;
                }

                int rc = fh->cb ? fh->cb(val, fh->user_data) : 0;
                handled = 1;
                if (consume_next) read++;
                if (rc == CBUILD_FLAG_EXIT) return CBUILD_FLAG_EXIT;
                if (rc) return rc;
                break;
            }
        } else {
            /* -xyz or -j8 or -j 8 */
            const char* p = arg + 1;
            while (*p && !handled) {
                char s = *p++;
                cbuild_flag_handler_t* fh = NULL;
                for (int i = 0; i < ctx->flag_count; ++i) {
                    if (ctx->flag_handlers[i].phase == phase &&
                        ctx->flag_handlers[i].short_name == s) {
                        fh = &ctx->flag_handlers[i];
                        break;
                    }
                }
                if (!fh) {
                    cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: unknown flag -%c", s);
                    return 1;
                }

                if (fh->takes_value) {
                    if (*p)
                        val = p;
                    else if (read + 1 < ac) {
                        val = av[read + 1];
                        consume_next = 1;
                    } else {
                        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: -%c requires a value", s);
                        return 1;
                    }
                }

                if (fh->bind_bool) *fh->bind_bool = 1;
                if (fh->bind_int && val) *fh->bind_int = atoi(val);
                if (fh->bind_str && val) {
                    /* Free any previously allocated string */
                    if (fh->allocated_str) free(fh->allocated_str);
                    fh->allocated_str = strdup(val);
                    *fh->bind_str = fh->allocated_str;
                }

                int rc = fh->cb ? fh->cb(val, fh->user_data) : 0;
                handled = 1;
                if (consume_next) read++;
                if (rc == CBUILD_FLAG_EXIT) return CBUILD_FLAG_EXIT;
                if (rc) return rc;
                break; /* stop cluster if one took a value */
            }
        }

        if (!handled) {
            cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: unknown option '%s'", arg);
            return 1;
        }
    }

    av[write] = NULL;
    *argc = write;
    return 0;
}

/* ---- src/fs.c ---- */

/* fs.c - filesystem, path, and wildcard helpers. */


static void remove_dir_recursive(cbuild_context_t* ctx, const char* path);

/* Normalize path separators to forward slashes for consistent hashing/comparison */
CBUILD_INTERNAL char* cbuild__normalize_path(const char* path) {
    if (!path) return NULL;
    char* normalized = strdup(path);
    if (!normalized) return NULL;
#ifdef _WIN32
    for (char* p = normalized; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
#endif
    return normalized;
}

/* Returns 0 and fills *out, or -1 when the path cannot be examined. */
CBUILD_INTERNAL int cbuild__mtime(const char* path, cbuild_mtime_t* out) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return -1;
    *out = ((cbuild_mtime_t)data.ftLastWriteTime.dwHighDateTime << 32) |
           (cbuild_mtime_t)data.ftLastWriteTime.dwLowDateTime;
    return 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    cbuild_mtime_t nsec = 0;
    /* The sub-second field has no portable name. Where st_mtime is a macro it
     * aliases the seconds of a timespec member, which tells the layouts apart
     * even when this header is not the first include. */
#if defined(st_mtime) && defined(__APPLE__)
    nsec = (cbuild_mtime_t)st.st_mtimespec.tv_nsec;
#elif defined(st_mtime)
    nsec = (cbuild_mtime_t)st.st_mtim.tv_nsec;
#elif defined(__APPLE__) || defined(__GLIBC__)
    nsec = (cbuild_mtime_t)st.st_mtimensec;
#endif
    *out = (cbuild_mtime_t)st.st_mtime * 1000000000ULL + nsec;
    return 0;
#endif
}

int cbuild_file_exists(const char* path) {
    if (!path || !*path)
        return 0;
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int cbuild_dir_exists(const char* path) {
    if (!path || !*path)
        return 0;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int cbuild_remove_file(const char* path) {
    if (!path || !*path)
        return -1;
    if (!cbuild_file_exists(path))
        return 0;
    return unlink(path);
}

int cbuild_remove_dir(const char* path) {
    if (!path || !*path)
        return -1;
    if (!cbuild_dir_exists(path))
        return 0;
#ifdef _WIN32
    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", path);
    return system(cmd);
#else
    DIR* dir;
    struct dirent* entry;
    char full_path[PATH_MAX];
    if (!(dir = opendir(path)))
        return -1;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (cbuild_remove_dir(full_path) != 0) {
                closedir(dir);
                return -1;
            }
        } else {
            if (cbuild_remove_file(full_path) != 0) {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    return rmdir(path);
#endif
}

int cbuild_get_cwd(char* buf, long size) {
    if (!buf || size <= 0)
        return -1;
    return getcwd(buf, size) ? 0 : -1;
}

int cbuild_match_wildcard(const char* pattern, const char* string) {
    if (!pattern || !string)
        return 0;

    if (*pattern == '\0')
        return *string == '\0';

    if (*pattern == '*') {
        if (*(pattern + 1) == '*' && *(pattern + 2) != '*')
            pattern++;

        if (*(pattern + 1) == '\0')
            return 1;

        while (*string) {
            if (cbuild_match_wildcard(pattern + 1, string))
                return 1;
            string++;
        }
        return cbuild_match_wildcard(pattern + 1, string);
    }

    if (*pattern == '?' || *pattern == *string) {
        return cbuild_match_wildcard(pattern + 1, string + 1);
    }

    return 0;
}

int cbuild_expand_wildcard(const char* pattern, char*** files,
                           int* file_count) {
    char dir_path[PATH_MAX] = { 0 };
    char base_pattern[PATH_MAX] = { 0 };
    int capacity = 32;

    if (!pattern || !files || !file_count)
        return -1;

    *files = NULL;
    *file_count = 0;

    /* Search from the deepest directory named before the first wildcard, so
     * wildcards in directory names, recursive ones included, can match. */
    const char* wild = strpbrk(pattern, "*?");
    const char* separator = NULL;
    for (const char* p = pattern; *p && (!wild || p < wild); ++p) {
        if (*p == '/' || *p == '\\') separator = p;
    }

    if (!separator) {
        strcpy(dir_path, ".");
        strcpy(base_pattern, pattern);
    } else {
        size_t dir_len = separator - pattern;
        strncpy(dir_path, pattern, dir_len);
        dir_path[dir_len] = '\0';

        strcpy(base_pattern, separator + 1);
    }

    *files = (char**)malloc(capacity * sizeof(char*));
    if (!*files)
        return -1;

    int result = cbuild_expand_wildcard_recursive(dir_path, base_pattern, files,
                                                  file_count, &capacity);

    /* Directory order varies between machines and runs; sorting keeps builds reproducible. */
    if (result == 0 && *file_count > 1) {
        qsort(*files, *file_count, sizeof(char*), cbuild__strcmp_wrapper);
    }

    if (result != 0 && *files) {
        for (int i = 0; i < *file_count; i++) {
            free((*files)[i]);
        }
        free(*files);
        *files = NULL;
        *file_count = 0;
    }

    return result;
}

static int cbuild_add_to_file_list(char*** files, int* file_count,
                                   int* capacity, const char* path) {
    if (*file_count >= *capacity) {
        *capacity *= 2;
        char** new_files = (char**)realloc(*files, *capacity * sizeof(char*));
        if (!new_files) {
            return -1;
        }
        *files = new_files;
    }

    /* Normalize path separators for consistent hashing on Windows */
    (*files)[*file_count] = cbuild__normalize_path(path);
    if (!(*files)[*file_count]) {
        return -1;
    }
    (*file_count)++;

    return 0;
}

int cbuild_expand_wildcard_recursive(const char* dir_path, const char* pattern,
                                     char*** files, int* file_count,
                                     int* capacity) {
    int recursive_search = 0;

    if (strncmp(pattern, "**", 2) == 0) {
        /* "**" spans zero or more directories: the rest of the pattern applies
         * here, and the whole pattern again in every subdirectory below. */
        const char* rest = pattern + 2;
        if (*rest == '/' || *rest == '\\') rest++;
        if (cbuild_expand_wildcard_recursive(dir_path, *rest ? rest : "*", files, file_count, capacity) != 0)
            return -1;
        recursive_search = 1;
    }

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    char search_path[PATH_MAX];

    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return -1;
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        if (strcmp(dir_path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", find_data.cFileName);
        } else {
            snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive_search) {
                cbuild_expand_wildcard_recursive(full_path, pattern, files, file_count,
                                                 capacity);
            } else if (strchr(pattern, '/') || strchr(pattern, '\\')) {
                const char* slash = strchr(pattern, '/');
                if (!slash)
                    slash = strchr(pattern, '\\');

                char subdir_pattern[PATH_MAX] = { 0 };
                strncpy(subdir_pattern, pattern, slash - pattern);
                subdir_pattern[slash - pattern] = '\0';

                if (cbuild_match_wildcard(subdir_pattern, find_data.cFileName)) {
                    cbuild_expand_wildcard_recursive(full_path, slash + 1, files,
                                                     file_count, capacity);
                }
            }
        } else if (!recursive_search &&
                   cbuild_match_wildcard(pattern, find_data.cFileName)) {
            if (cbuild_add_to_file_list(files, file_count, capacity, full_path) !=
                0) {
                FindClose(hFind);
                return -1;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    DIR* dir;
    struct dirent* entry;

    dir = opendir(dir_path);
    if (!dir) {
        return -1;
    }

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        if (strcmp(dir_path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        }

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (recursive_search) {
                cbuild_expand_wildcard_recursive(full_path, pattern, files, file_count,
                                                 capacity);
            } else if (strchr(pattern, '/') || strchr(pattern, '\\')) {
                const char* slash = strchr(pattern, '/');
                if (!slash)
                    slash = strchr(pattern, '\\');

                char subdir_pattern[PATH_MAX] = { 0 };
                strncpy(subdir_pattern, pattern, slash - pattern);
                subdir_pattern[slash - pattern] = '\0';

                if (cbuild_match_wildcard(subdir_pattern, entry->d_name)) {
                    cbuild_expand_wildcard_recursive(full_path, slash + 1, files,
                                                     file_count, capacity);
                }
            }
        } else if (!recursive_search &&
                   cbuild_match_wildcard(pattern, entry->d_name)) {
            if (cbuild_add_to_file_list(files, file_count, capacity, full_path) !=
                0) {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
#endif
    return 0;
}

CBUILD_INTERNAL char* cbuild__join_path(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    int need_slash = (la > 0 && a[la - 1] != '/' && a[la - 1] != '\\');
    char* out = (char*)malloc(la + lb + 2);
    strcpy(out, a);
    if (need_slash)
        strcat(out, "/");
    strcat(out, b);
    return out;
}

CBUILD_INTERNAL int cbuild__is_library_path(const char* path) {
    if (!path) return 0;
#ifdef _WIN32
    // linkable libs on Windows are .lib (and MinGW can use .a)
    if (cbuild__has_suffix(path, ".lib")) return 1;
    if (cbuild__has_suffix(path, ".a")) return 1;
    // .dll is NOT linkable directly
    return 0;
#else
#if defined(__APPLE__)
    if (cbuild__has_suffix(path, ".dylib")) return 1;
#endif
    if (cbuild__has_suffix(path, ".a")) return 1;
    if (cbuild__has_suffix(path, ".so")) return 1;
    return 0;
#endif
}

CBUILD_INTERNAL void get_dir_from_path(const char* path, char* out_dir, size_t out_size) {
    if (!path || !out_dir || out_size == 0) return;
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* separator = last_slash > last_backslash ? last_slash : last_backslash;

    if (separator) {
        size_t dir_len = separator - path;
        if (dir_len >= out_size) dir_len = out_size - 1;
        strncpy(out_dir, path, dir_len);
        out_dir[dir_len] = '\0';
    } else {
        out_dir[0] = '.';
        out_dir[1] = '\0';
    }
}

CBUILD_INTERNAL int ensure_dir_exists(const char* path) {
    if (!path || !*path)
        return 0;
    char temp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) {
        return -1;  // path too long
    }
    strcpy(temp, path);
    if (temp[len - 1] == '/' || temp[len - 1] == '\\') {
        temp[len - 1] = '\0';
    }
    for (char* p = temp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
#ifdef _WIN32
            if (_mkdir(temp) != 0) {
                if (errno != EEXIST) {
                    *p = '/';
                    return -1;
                }
            }
#else
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                *p = '/';
                return -1;
            }
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    if (_mkdir(temp) != 0) {
        if (errno != EEXIST)
            return -1;
    }
#else
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
#endif
    return 0;
}

/* Comparison functions for sorting to stabilize build results */
CBUILD_INTERNAL int cbuild__strcmp_wrapper(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    return strcmp(sa, sb);
}

CBUILD_INTERNAL void remove_file(cbuild_context_t* ctx, const char* path) {
    (void)ctx;
    if (!path || !*path)
        return;
    remove(path);
}

/* Classify a path without following links: 1 = real directory, -1 = symlink
 * (or Windows reparse point), 0 = anything else, including a missing path. */
static int cbuild__real_dir_kind(const char* path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return 0;
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) return -1;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISLNK(st.st_mode)) return -1;
    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

/* Returns 1 when path is the current working directory or one of its ancestors
 * (which includes the filesystem root), or when that cannot be ruled out. */
static int cbuild__dir_contains_cwd(const char* path) {
#ifdef _WIN32
    char full[MAX_PATH];
    char cwd[MAX_PATH];
    if (!_fullpath(full, path, sizeof(full)) || !_getcwd(cwd, sizeof(cwd))) return 1;
    size_t n = strlen(full);
    while (n > 0 && (full[n - 1] == '\\' || full[n - 1] == '/')) n--;
    if (_strnicmp(full, cwd, n) != 0) return 0;
    return cwd[n] == '\0' || cwd[n] == '\\' || cwd[n] == '/';
#else
    struct stat st_target;
    if (stat(path, &st_target) != 0) return 0;
    char rel[PATH_MAX] = ".";
    for (;;) {
        struct stat st_cur, st_parent;
        if (stat(rel, &st_cur) != 0) return 1;
        if (st_cur.st_dev == st_target.st_dev && st_cur.st_ino == st_target.st_ino) return 1;
        if (strlen(rel) + 4 > sizeof(rel)) return 1;
        strcat(rel, "/..");
        if (stat(rel, &st_parent) != 0) return 1;
        if (st_parent.st_dev == st_cur.st_dev && st_parent.st_ino == st_cur.st_ino) return 0;
    }
#endif
}

/* Remove a directory tree that cbuild created. Symlinked directories are left
 * alone, as is anything that would take the working directory with it. */
CBUILD_INTERNAL void cbuild__clean_dir(cbuild_context_t* ctx, const char* path) {
    if (!path || !*path)
        return;
    int kind = cbuild__real_dir_kind(path);
    if (kind == 0)
        return;
    if (kind < 0) {
        cbuild__log(ctx, CBUILD_LOG_WARNING, "cbuild: not cleaning '%s': it is a symbolic link", path);
        return;
    }
    if (cbuild__dir_contains_cwd(path)) {
        cbuild__log(ctx, CBUILD_LOG_WARNING,
                    "cbuild: not cleaning '%s': it contains the current working directory", path);
        return;
    }
    remove_dir_recursive(ctx, path);
}

/* Links are removed, never followed, so nothing outside the tree is touched. */
static void remove_dir_recursive(cbuild_context_t* ctx, const char* path) {
    if (!path || !*path)
        return;
    if (cbuild__real_dir_kind(path) != 1)
        return;
#ifdef _WIN32
    WIN32_FIND_DATA ffd;
    char* pattern = cbuild__join_path(path, "*");
    HANDLE hFind = FindFirstFile(pattern, &ffd);
    free(pattern);
    if (hFind == INVALID_HANDLE_VALUE)
        return;
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;
        char* full = cbuild__join_path(path, ffd.cFileName);
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                _rmdir(full);
            } else {
                remove_dir_recursive(ctx, full);
            }
        } else {
            remove_file(ctx, full);
        }
        free(full);
    } while (FindNextFile(hFind, &ffd));
    FindClose(hFind);
    _rmdir(path);
#else
    DIR* dir = opendir(path);
    if (!dir)
        return;
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        char* full = cbuild__join_path(path, entry->d_name);
        struct stat st;
        if (!lstat(full, &st)) {
            if (S_ISDIR(st.st_mode)) {
                remove_dir_recursive(ctx, full);
            } else {
                remove_file(ctx, full);
            }
        }
        free(full);
    }
    closedir(dir);
    rmdir(path);
#endif
}

/* ---- src/log.c ---- */

/* log.c - logging and error reporting. */


static void cbuild__default_logger(void* user_data, cbuild_log_level_t level, const char* msg) {
    (void)user_data;
    FILE* out = (level == CBUILD_LOG_ERROR || level == CBUILD_LOG_WARNING || level == CBUILD_LOG_STATUS_FAIL) ? stderr : stdout;
    switch (level) {
        case CBUILD_LOG_INFO:
            fprintf(out, "%s\n", msg);
            break;
        case CBUILD_LOG_WARNING:
            fprintf(out, "%sWarning:%s %s\n", CBUILD_COLOR_YELLOW, CBUILD_COLOR_RESET, msg);
            break;
        case CBUILD_LOG_ERROR:
            fprintf(out, "%sError:%s %s\n", CBUILD_COLOR_RED, CBUILD_COLOR_RESET, msg);
            break;
        case CBUILD_LOG_STEP: {
            const char* space = strchr(msg, ' ');
            if (space) {
                int label_len = (int)(space - msg);
                fprintf(out, "%s%-10.*s%s %s\n", CBUILD_COLOR_BLUE, label_len, msg, CBUILD_COLOR_RESET, space + 1);
            } else {
                fprintf(out, "%s%-10s%s\n", CBUILD_COLOR_BLUE, msg, CBUILD_COLOR_RESET);
            }
            break;
        }
        case CBUILD_LOG_STATUS_OK:
            fprintf(out, "%s%s%s %s\n", CBUILD_COLOR_GREEN, "✔", CBUILD_COLOR_RESET, msg);
            break;
        case CBUILD_LOG_STATUS_FAIL:
            fprintf(out, "%s%s%s %s\n", CBUILD_COLOR_RED, "✖", CBUILD_COLOR_RESET, msg);
            break;
        case CBUILD_LOG_VERBOSE:
            fprintf(out, "%s\n", msg);
            break;
    }
    fflush(out);
}

CBUILD_INTERNAL void cbuild__log(cbuild_context_t* ctx, cbuild_log_level_t level, const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    cbuild_log_fn fn = ctx->log_fn ? ctx->log_fn : cbuild__default_logger;
    fn(ctx->log_user_data, level, buf);
}

CBUILD_INTERNAL void cbuild__log_step(cbuild_context_t* ctx, const char* label, const char* color, const char* fmt, ...) {
    (void)color;
    char buf[2048];
    char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    snprintf(msg, sizeof(msg), "%s ", label);
    size_t used = strlen(msg);
    strncat(msg, buf, sizeof(msg) - used - 1);
    cbuild_log_fn fn = ctx->log_fn ? ctx->log_fn : cbuild__default_logger;
    fn(ctx->log_user_data, CBUILD_LOG_STEP, msg);
}

CBUILD_INTERNAL void cbuild__log_status(cbuild_context_t* ctx, int ok, const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    cbuild_log_fn fn = ctx->log_fn ? ctx->log_fn : cbuild__default_logger;
    fn(ctx->log_user_data, ok ? CBUILD_LOG_STATUS_OK : CBUILD_LOG_STATUS_FAIL, buf);
}

void cbuild_set_logger(cbuild_context_t* ctx, cbuild_log_fn callback, void* user_data) {
    ctx->log_fn = callback;
    ctx->log_user_data = user_data;
}

const char* cbuild_get_last_error(cbuild_context_t* ctx) {
    if (!ctx) return "NULL context";
    return ctx->last_error[0] ? ctx->last_error : NULL;
}

CBUILD_INTERNAL void cbuild__set_error(cbuild_context_t* ctx, const char* fmt, ...) {
    if (!ctx) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->last_error, sizeof(ctx->last_error), fmt, args);
    va_end(args);
}

/* ---- src/process.c ---- */

/* process.c - argv building and process spawning. */


CBUILD_INTERNAL void cbuild_argv_init(cbuild_argv_t* argv) {
    argv->args = NULL;
    argv->count = 0;
    argv->capacity = 0;
}

CBUILD_INTERNAL void cbuild_argv_free(cbuild_argv_t* argv) {
    if (argv->args) {
        for (int i = 0; i < argv->count; ++i) {
            free(argv->args[i]);
        }
        free(argv->args);
    }
    argv->args = NULL;
    argv->count = 0;
    argv->capacity = 0;
}

CBUILD_INTERNAL int cbuild_argv_append(cbuild_argv_t* argv, const char* arg) {
    if (argv->count >= argv->capacity) {
        int new_cap = argv->capacity == 0 ? 16 : argv->capacity * 2;
        char** new_args = (char**)realloc(argv->args, (new_cap + 1) * sizeof(char*));
        if (!new_args) {
            return -1;
        }
        argv->args = new_args;
        argv->capacity = new_cap;
    }
    argv->args[argv->count] = strdup(arg);
    if (!argv->args[argv->count]) {
        return -1;
    }
    argv->count++;
    argv->args[argv->count] = NULL; /* NULL-terminate for execvp */
    return 0;
}

#ifdef _WIN32

/* Windows: Build command line from argv for CreateProcess */
CBUILD_INTERNAL char* cbuild_argv_to_cmdline(cbuild_argv_t* argv) {
    size_t total_len = 0;
    for (int i = 0; i < argv->count; ++i) {
        const char* arg = argv->args[i];
        int needs_quote = 0;
        if (strchr(arg, ' ') || strchr(arg, '\t') || strchr(arg, '"') || *arg == '\0') {
            needs_quote = 1;
        }
        if (needs_quote) total_len += 2; /* quotes */
        for (const char* p = arg; *p; ++p) {
            if (*p == '"')
                total_len += 2; /* \" */
            else if (*p == '\\') {
                const char* q = p;
                int num_backslash = 0;
                while (*q == '\\') {
                    q++;
                    num_backslash++;
                }
                if (*q == '"' || *q == '\0') {
                    total_len += num_backslash * 2;
                    p = q - 1;
                } else {
                    total_len += num_backslash;
                    p = q - 1;
                }
            } else {
                total_len += 1;
            }
        }
        if (i > 0) total_len += 1; /* space separator */
    }

    char* cmdline = (char*)malloc(total_len + 1);
    if (!cmdline) return NULL;
    char* out = cmdline;

    for (int i = 0; i < argv->count; ++i) {
        if (i > 0) *out++ = ' ';
        const char* arg = argv->args[i];
        int needs_quote = 0;
        if (strchr(arg, ' ') || strchr(arg, '\t') || strchr(arg, '"') || *arg == '\0') {
            needs_quote = 1;
        }
        if (needs_quote) *out++ = '"';

        for (const char* p = arg; *p; ++p) {
            if (*p == '"') {
                *out++ = '\\';
                *out++ = '"';
            } else if (*p == '\\') {
                const char* q = p;
                int num_backslash = 0;
                while (*q == '\\') {
                    q++;
                    num_backslash++;
                }
                if (*q == '"' || *q == '\0') {
                    for (int k = 0; k < num_backslash * 2; ++k) *out++ = '\\';
                    p = q - 1;
                } else {
                    for (int k = 0; k < num_backslash; ++k) *out++ = '\\';
                    p = q - 1;
                }
            } else {
                *out++ = *p;
            }
        }
        if (needs_quote) *out++ = '"';
    }
    *out = '\0';
    return cmdline;
}

#else

/* Unix/Linux: Build command line from argv for compile_commands.json */
CBUILD_INTERNAL char* cbuild_argv_to_cmdline(cbuild_argv_t* argv) {
    size_t total_len = 0;
    for (int i = 0; i < argv->count; ++i) {
        const char* arg = argv->args[i];
        int needs_quote = 0;
        /* Check if argument needs quoting */
        for (const char* p = arg; *p; ++p) {
            if (*p == ' ' || *p == '\t' || *p == '\'' || *p == '"' || *p == '\\' ||
                *p == '$' || *p == '`' || *p == '!' || *p == '*' || *p == '?' ||
                *p == '[' || *p == ']' || *p == '(' || *p == ')' || *p == '{' || *p == '}' ||
                *p == '&' || *p == '|' || *p == ';' || *p == '<' || *p == '>') {
                needs_quote = 1;
                break;
            }
        }
        if (*arg == '\0') needs_quote = 1;

        if (needs_quote) {
            total_len += 2; /* quotes */
            for (const char* p = arg; *p; ++p) {
                if (*p == '\'' || *p == '\\' || *p == '"') {
                    total_len += 2; /* escape char + char */
                } else {
                    total_len += 1;
                }
            }
        } else {
            total_len += strlen(arg);
        }
        if (i > 0) total_len += 1; /* space separator */
    }

    char* cmdline = (char*)malloc(total_len + 1);
    if (!cmdline) return NULL;
    char* out = cmdline;

    for (int i = 0; i < argv->count; ++i) {
        if (i > 0) *out++ = ' ';
        const char* arg = argv->args[i];
        int needs_quote = 0;

        /* Check if argument needs quoting */
        for (const char* p = arg; *p; ++p) {
            if (*p == ' ' || *p == '\t' || *p == '\'' || *p == '"' || *p == '\\' ||
                *p == '$' || *p == '`' || *p == '!' || *p == '*' || *p == '?' ||
                *p == '[' || *p == ']' || *p == '(' || *p == ')' || *p == '{' || *p == '}' ||
                *p == '&' || *p == '|' || *p == ';' || *p == '<' || *p == '>') {
                needs_quote = 1;
                break;
            }
        }
        if (*arg == '\0') needs_quote = 1;

        if (needs_quote) {
            *out++ = '"';
            for (const char* p = arg; *p; ++p) {
                if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
                    *out++ = '\\';
                }
                *out++ = *p;
            }
            *out++ = '"';
        } else {
            for (const char* p = arg; *p; ++p) {
                *out++ = *p;
            }
        }
    }
    *out = '\0';
    return cmdline;
}

#endif

CBUILD_INTERNAL int cbuild_spawn_process(cbuild_context_t* ctx, cbuild_argv_t* argv, int capture_output, char** captured_output) {
    if (ctx && ctx->verbose && !capture_output) {
        char cmd_buf[4096];
        int pos = 0;
        for (int i = 0; i < argv->count && pos < (int)sizeof(cmd_buf) - 1; ++i) {
            if (i > 0) cmd_buf[pos++] = ' ';
            if (strchr(argv->args[i], ' ')) {
                pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - pos, "'%s'", argv->args[i]);
            } else {
                pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - pos, "%s", argv->args[i]);
            }
        }
        cmd_buf[pos] = '\0';
        cbuild__log(ctx, CBUILD_LOG_VERBOSE, "%s", cmd_buf);
    }

#ifdef _WIN32
    HANDLE hOutputRead = NULL, hOutputWrite = NULL;
    SECURITY_ATTRIBUTES sa = { 0 };
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (capture_output) {
        if (!CreatePipe(&hOutputRead, &hOutputWrite, &sa, 0)) {
            return -1;
        }
        SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(STARTUPINFOA);
    if (capture_output) {
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hOutputWrite;
        si.hStdError = hOutputWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    PROCESS_INFORMATION pi = { 0 };
    char* cmdline = cbuild_argv_to_cmdline(argv);
    if (!cmdline) {
        if (capture_output) {
            CloseHandle(hOutputRead);
            CloseHandle(hOutputWrite);
        }
        return -1;
    }

    BOOL success = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi);

    free(cmdline);

    if (!success) {
        if (capture_output) {
            CloseHandle(hOutputRead);
            CloseHandle(hOutputWrite);
        }
        return -1;
    }

    if (capture_output) {
        CloseHandle(hOutputWrite);

        *captured_output = NULL;
        size_t out_len = 0;
        char buffer[4096];
        DWORD bytes_read;

        while (ReadFile(hOutputRead, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
            char* new_output = (char*)realloc(*captured_output, out_len + bytes_read + 1);
            if (!new_output) {
                free(*captured_output);
                *captured_output = NULL;
                CloseHandle(hOutputRead);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return -1;
            }
            *captured_output = new_output;
            memcpy(*captured_output + out_len, buffer, bytes_read);
            out_len += bytes_read;
            (*captured_output)[out_len] = '\0';
        }

        CloseHandle(hOutputRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;

#else
    /* POSIX: try posix_spawn first, fall back to fork/execvp */
    int pipefd[2] = { -1, -1 };

    if (capture_output) {
        if (pipe(pipefd) != 0) {
            return -1;
        }
    }

    pid_t pid;

#ifdef __APPLE__
    /* macOS: use posix_spawn (preferred) */
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    if (capture_output) {
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipefd[0]);
        posix_spawn_file_actions_addclose(&actions, pipefd[1]);
    }

    int spawn_result = posix_spawnp(&pid, argv->args[0], &actions, NULL, argv->args, environ);
    posix_spawn_file_actions_destroy(&actions);

    if (spawn_result != 0) {
        if (capture_output) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return -1;
    }

#else
    /* Linux and other POSIX: use fork/execvp */
    pid = fork();
    if (pid < 0) {
        if (capture_output) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        if (capture_output) {
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
        }
        execvp(argv->args[0], argv->args);
        _exit(127); /* execvp failed */
    }
#endif

    /* Parent process */
    if (capture_output) {
        close(pipefd[1]);

        *captured_output = NULL;
        size_t out_len = 0;
        char buffer[4096];
        ssize_t bytes_read;

        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            char* new_output = (char*)realloc(*captured_output, out_len + bytes_read + 1);
            if (!new_output) {
                free(*captured_output);
                *captured_output = NULL;
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                return -1;
            }
            *captured_output = new_output;
            memcpy(*captured_output + out_len, buffer, bytes_read);
            out_len += bytes_read;
            (*captured_output)[out_len] = '\0';
        }

        close(pipefd[0]);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return -1;
#endif
}

int cbuild_spawn(cbuild_context_t* ctx, const char* const* argv, int argc, char** output) {
    if (output) *output = NULL;
    if (!ctx || !argv || argc <= 0 || !argv[0]) return -1;
    cbuild_argv_t av;
    cbuild_argv_init(&av);
    for (int i = 0; i < argc; ++i) {
        if (cbuild_argv_append(&av, argv[i]) != 0) {
            cbuild_argv_free(&av);
            return -1;
        }
    }
    int rc = cbuild_spawn_process(ctx, &av, output != NULL, output);
    cbuild_argv_free(&av);
    return rc;
}

/* Legacy shell-based command execution (deprecated, kept for compatibility) */
CBUILD_INTERNAL int run_command(cbuild_context_t* ctx, const char* cmd, int capture_out,
                                char** captured_output) {
    if (ctx && ctx->verbose && !capture_out) {
        cbuild__log(ctx, CBUILD_LOG_VERBOSE, "%s", cmd);
    }
    if (capture_out) {
#ifdef _WIN32
        FILE* pipe = _popen(cmd, "r");
#else
        FILE* pipe = popen(cmd, "r");
#endif
        if (!pipe) {
            return -1;
        }
        char buffer[256];
        size_t out_len = 0;
        *captured_output = NULL;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            size_t chunk = strlen(buffer);
            *captured_output = (char*)realloc(*captured_output, out_len + chunk + 1);
            if (!*captured_output) {
                cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Out of memory capturing command output");
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                return -1;
            }
            memcpy(*captured_output + out_len, buffer, chunk);
            out_len += chunk;
            (*captured_output)[out_len] = '\0';
        }
#ifdef _WIN32
        int exitCode = _pclose(pipe);
#else
        int exitCode = pclose(pipe);
#endif
        if (exitCode == -1) {
            return -1;
        }
        return exitCode;
    } else {
#ifdef _WIN32
        /* Use PowerShell on Windows for better path and command handling */
        size_t cmd_len = strlen(cmd);
        char* ps_cmd = (char*)malloc(cmd_len + 128);
        if (!ps_cmd) return -1;
        snprintf(ps_cmd, cmd_len + 128, "powershell -NoProfile -ExecutionPolicy Bypass -Command \"%s\"", cmd);
        int ret = system(ps_cmd);
        free(ps_cmd);
        return ret;
#else
        int ret = system(cmd);
        return ret;
#endif
    }
}

CBUILD_INTERNAL void cbuild__argv_append_prefixed(cbuild_argv_t* argv, const char* prefix, const char* value) {
    char* arg = NULL;
    if (append_format(&arg, "%s%s", prefix, value) == 0) {
        cbuild_argv_append(argv, arg);
    }
    free(arg);
}

/* Helper to split space-separated flag strings into argv tokens */
/* Note: No caching - called from parallel worker threads, must be thread-safe */
CBUILD_INTERNAL void cbuild_argv_append_flags(cbuild_argv_t* argv, const char* flags) {
    if (!flags || !*flags) return;

    /* Tokenize the flags string */
    char* copy = strdup(flags);
    if (!copy) return;

    char* p = copy;
    while (*p) {
        /* Skip leading whitespace */
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;

        /* Find end of token */
        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t')) {
                break;
            } else {
                p++;
            }
        }

        if (p > start) {
            char saved = *p;
            *p = '\0';

            /* Remove surrounding quotes if present */
            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            cbuild_argv_append(argv, token);

            *p = saved;
        }
    }

    free(copy);
}

/* ---- src/self_rebuild.c ---- */

/* self_rebuild.c - rebuild and re-exec the build program when its sources change. */


static int cbuild__needs_rebuild(const char* exe_path, const char** sources,
                                 int sources_count) {
    cbuild_mtime_t exe_mtime;
    if (cbuild__mtime(exe_path, &exe_mtime) != 0)
        return 1;
    for (int i = 0; i < sources_count; ++i) {
        cbuild_mtime_t src_mtime;
        if (cbuild__mtime(sources[i], &src_mtime) != 0)
            continue;
        if (src_mtime > exe_mtime)
            return 1;
    }
    return 0;
}

static int cbuild__exec_new_build(const char* exe_path, int argc,
                                  char** argv) {
    (void)argc;
#ifdef _WIN32
    _spawnv(_P_OVERLAY, exe_path, argv);
    return -1;
#else
    execv(exe_path, argv);
    return -1;
#endif
}

int cbuild_self_rebuild_if_needed(cbuild_context_t* ctx, int argc, char** argv, const char** sources,
                                  int sources_count) {
    char exe_path[512];
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
#elif defined(__APPLE__)
    uint32_t sz = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &sz) != 0) {
        strncpy(exe_path, argv[0], sizeof(exe_path));
    }
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0)
        exe_path[len] = 0;
    else
        strncpy(exe_path, argv[0], sizeof(exe_path));
#endif

    // Always remove any lingering .old file
    char old_path[520];
    snprintf(old_path, sizeof(old_path), "%s.old", exe_path);
    remove(old_path);

    // Pick the main build source: the first item passed to CBUILD_SELF_REBUILD(...)
    const char* build_src = (sources_count > 0 && sources[0] && *sources[0])
                                ? sources[0]
                                : "build.c";  // conservative fallback

    if (cbuild__needs_rebuild(exe_path, sources, sources_count)) {
        cbuild__log(ctx, CBUILD_LOG_INFO, "cbuild: Detected changes, rebuilding build executable from %s...", build_src);
        rename(exe_path, old_path);

        char cmd[1024];
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd),
                 "cl /nologo /Fe:\"%s\" \"%s\" /I. /Iinclude",
                 exe_path, build_src);
#else
        snprintf(cmd, sizeof(cmd),
                 "cc -o \"%s\" \"%s\" -I. -Iinclude",
                 exe_path, build_src);
#endif
        int rc = system(cmd);
        if (rc != 0) {
            cbuild__set_error(ctx, "Self-rebuild failed with exit code %d", rc);
            cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Self-rebuild failed!");
            return -1;
        }
        if (cbuild__exec_new_build(exe_path, argc, argv) != 0) {
            cbuild__set_error(ctx, "Failed to exec new build executable");
            return -1;
        }
    }
    return 0;
}

/* ---- src/subproject.c ---- */

/* subproject.c - subproject manifests and imported targets. */


static void cbuild__parse_manifest(cbuild_context_t* ctx, subproject_t* sub) {
    if (sub->manifest_loaded)
        return;

    /* Save current directory and change to subproject directory */
    char old_cwd[PATH_MAX];
    if (!getcwd(old_cwd, sizeof(old_cwd))) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Failed to get current directory");
        return;
    }

    if (chdir(sub->directory) != 0) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Failed to change to subproject directory '%s'", sub->directory);
        return;
    }

    /* Build argv for manifest command */
    cbuild_argv_t argv;
    cbuild_argv_init(&argv);
    cbuild_argv_append(&argv, sub->cbuild_exe);
    cbuild_argv_append(&argv, "--manifest");

    char* output = NULL;
    int result = cbuild_spawn_process(ctx, &argv, 1, &output);
    cbuild_argv_free(&argv);

    /* Restore original directory */
    chdir(old_cwd);

    if (result != 0 || !output) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Failed to get manifest from subproject '%s'", sub->alias);
        if (output)
            free(output);
        return;
    }

    char* saveptr = NULL;
    char* line = strtok_r(output, "\r\n", &saveptr);

    while (line) {
        cbuild__trim(line);
        if (!line[0] || line[0] == '#') {
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        char* line_copy = strdup(line);
        char* type = strtok(line_copy, " \t");
        char* name = strtok(NULL, " \t");
        char* path = strtok(NULL, "\r\n");

        if (type && name && path) {
            cbuild__trim(path);
            if (sub->target_count + 1 > sub->target_cap) {
                sub->target_cap = sub->target_cap ? sub->target_cap * 2 : 4;
                sub->targets = realloc(
                    sub->targets, sub->target_cap * sizeof(cbuild_subproject_target_t));
            }
            sub->targets[sub->target_count].name = strdup(name);
            sub->targets[sub->target_count].type = strdup(type);
            sub->targets[sub->target_count].output_path = strdup(path);
            sub->targets[sub->target_count].proxy_target = NULL;
            sub->target_count++;
        }

        free(line_copy);
        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    free(output);
    sub->manifest_loaded = 1;
}

static cbuild_subproject_target_t*
cbuild__find_subproject_target(subproject_t* sub, const char* tgt_name) {
    cbuild__parse_manifest(NULL, sub);
    for (int i = 0; i < sub->target_count; ++i) {
        if (strcmp(sub->targets[i].name, tgt_name) == 0) {
            return &sub->targets[i];
        }
    }
    return NULL;
}

subproject_t* cbuild_add_subproject(cbuild_context_t* ctx, const char* alias, const char* directory,
                                    const char* cbuild_exe) {
    subproject_t* sub = (subproject_t*)calloc(1, sizeof(subproject_t));
    sub->alias = strdup(alias);
    sub->directory = strdup(directory);
    sub->cbuild_exe = strdup(cbuild_exe);

    char* cmdline = NULL;
#ifdef _WIN32
    /* Shell commands use PowerShell on Windows. */
    append_format(&cmdline, "Set-Location -LiteralPath '%s'; & '%s'", directory, cbuild_exe);
#else
    append_format(&cmdline, "cd '%s' && '%s'", directory, cbuild_exe);
#endif
    char build_cmd_name[256];
    snprintf(build_cmd_name, sizeof(build_cmd_name), "build subproject %s",
             alias);
    sub->build_cmd = cbuild_command(ctx, build_cmd_name, cmdline);
    free(cmdline);

    if (ctx->subproject_count + 1 > ctx->subproject_cap) {
        ctx->subproject_cap = ctx->subproject_cap ? ctx->subproject_cap * 2 : 4;
        ctx->subprojects =
            realloc(ctx->subprojects, ctx->subproject_cap * sizeof(subproject_t*));
    }
    ctx->subprojects[ctx->subproject_count++] = sub;
    return sub;
}

target_t* cbuild_subproject_get_target(cbuild_context_t* ctx, subproject_t* sub,
                                       const char* tgt_name) {
    if (!sub)
        return NULL;
    cbuild_subproject_target_t* stgt =
        cbuild__find_subproject_target(sub, tgt_name);
    if (!stgt) {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Subproject '%s' has no target named '%s'", sub->alias, tgt_name);
        return NULL;
    }
    if (stgt->proxy_target)
        return stgt->proxy_target;

    cbuild_target_type type;
    if (strcmp(stgt->type, "static_lib") == 0) {
        type = TARGET_STATIC_LIB;
    } else if (strcmp(stgt->type, "shared_lib") == 0) {
        type = TARGET_SHARED_LIB;
    } else if (strcmp(stgt->type, "executable") == 0) {
        type = TARGET_EXECUTABLE;
    } else {
        cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Unknown subproject target type: %s", stgt->type);
        return NULL;
    }

    char proxy_name[256];
    snprintf(proxy_name, sizeof(proxy_name), "%s_%s", sub->alias, stgt->name);
    target_t* proxy = (target_t*)calloc(1, sizeof(target_t));
    proxy->type = type;
    proxy->name = strdup(proxy_name);
    proxy->external = 1;

    proxy->output_file = cbuild__join_path(sub->directory, stgt->output_path);
    proxy->obj_dir = NULL;  // not used

    proxy->commands = NULL;
    proxy->cmd_count = proxy->cmd_cap = 0;
    cbuild_target_add_command(ctx, proxy, sub->build_cmd);

    ensure_capacity_charpp(ctx, (char***)&ctx->targets, &ctx->target_count, &ctx->target_cap);
    ctx->targets[ctx->target_count++] = proxy;

    stgt->proxy_target = proxy;
    return proxy;
}

/* ---- src/target.c ---- */

/* target.c - target creation and per-target settings. */


static target_t* cbuild_create_target(cbuild_context_t* ctx, const char* name,
                                      cbuild_target_type type);

int cbuild_add_cflags(cbuild_context_t* ctx, target_t* target, const char* cflags) {
    if (!ctx || !target || !cflags) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_cflags");
        return -1;
    }
    char* copy = strdup(cflags);
    if (!copy) {
        cbuild__set_error(ctx, "Out of memory in cbuild_add_cflags");
        return -1;
    }

    char* p = copy;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;
        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t')) {
                break;
            } else {
                p++;
            }
        }

        if (p > start) {
            char saved = *p;
            *p = '\0';

            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            if (ensure_capacity_charpp(ctx, &target->cflags, &target->cflag_count, &target->cflag_cap) != 0) {
                free(copy);
                return -1;
            }
            target->cflags[target->cflag_count++] = strdup(token);

            *p = saved;
        }
    }

    free(copy);
    return 0;
}

int cbuild_add_ldflags(cbuild_context_t* ctx, target_t* target, const char* ldflags) {
    if (!ctx || !target || !ldflags) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_ldflags");
        return -1;
    }
    char* copy = strdup(ldflags);
    if (!copy) {
        cbuild__set_error(ctx, "Out of memory in cbuild_add_ldflags");
        return -1;
    }

    char* p = copy;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;
        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t')) {
                break;
            } else {
                p++;
            }
        }

        if (p > start) {
            char saved = *p;
            *p = '\0';

            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            if (ensure_capacity_charpp(ctx, &target->ldflags, &target->ldflag_count, &target->ldflag_cap) != 0) {
                free(copy);
                return -1;
            }
            target->ldflags[target->ldflag_count++] = strdup(token);

            *p = saved;
        }
    }

    free(copy);
    return 0;
}

target_t* cbuild_executable(cbuild_context_t* ctx, const char* name) {
    return cbuild_create_target(ctx, name, TARGET_EXECUTABLE);
}

target_t* cbuild_static_library(cbuild_context_t* ctx, const char* name) {
    return cbuild_create_target(ctx, name, TARGET_STATIC_LIB);
}

target_t* cbuild_shared_library(cbuild_context_t* ctx, const char* name) {
    return cbuild_create_target(ctx, name, TARGET_SHARED_LIB);
}

target_t* cbuild_dummy_target(cbuild_context_t* ctx, const char* name) {
    return cbuild_create_target(ctx, name, TARGET_DUMMY);
}

target_t* cbuild_file_dep_target(cbuild_context_t* ctx, const char* name, const char* file_path) {
    target_t* t = cbuild_create_target(ctx, name, TARGET_FILE_DEP);
    // repurpose output_file to mean required file
    if (t && file_path)
        t->output_file = strdup(file_path);
    return t;
}

int cbuild_add_source(cbuild_context_t* ctx, target_t* target, const char* source_file) {
    if (!ctx || !target || !source_file) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_source");
        return -1;
    }
    if (strchr(source_file, '*') || strchr(source_file, '?')) {
        char** expanded_files = NULL;
        int file_count = 0;

        if (cbuild_expand_wildcard(source_file, &expanded_files, &file_count) ==
                0 &&
            file_count > 0) {
            for (int i = 0; i < file_count; i++) {
                if (ensure_capacity_charpp(ctx, &target->sources, &target->sources_count,
                                       &target->sources_cap) != 0) {
                    for (int j = i; j < file_count; j++) free(expanded_files[j]);
                    free(expanded_files);
                    return -1;
                }
                target->sources[target->sources_count++] =
                    expanded_files[i];
            }
            free(expanded_files);
        }
    } else {
        if (ensure_capacity_charpp(ctx, &target->sources, &target->sources_count,
                               &target->sources_cap) != 0) {
            return -1;
        }
        /* Normalize path separators for consistent hashing on Windows */
        target->sources[target->sources_count++] = cbuild__normalize_path(source_file);
    }
    return 0;
}

int cbuild_add_include_dir(cbuild_context_t* ctx, target_t* target, const char* include_dir) {
    if (!ctx || !target || !include_dir) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_include_dir");
        return -1;
    }
    if (strchr(include_dir, '*') || strchr(include_dir, '?')) {
        char** expanded_dirs = NULL;
        int dir_count = 0;

        if (cbuild_expand_wildcard(include_dir, &expanded_dirs, &dir_count) == 0 &&
            dir_count > 0) {
            for (int i = 0; i < dir_count; i++) {
                if (cbuild_dir_exists(expanded_dirs[i])) {
                    if (ensure_capacity_charpp(ctx, &target->include_dirs, &target->include_count,
                                           &target->include_cap) != 0) {
                        for (int j = i; j < dir_count; j++) free(expanded_dirs[j]);
                        free(expanded_dirs);
                        return -1;
                    }
                    target->include_dirs[target->include_count++] =
                        expanded_dirs[i];
                } else {
                    free(expanded_dirs[i]);
                }
            }
            free(expanded_dirs);
        } else {
            cbuild__log(ctx, CBUILD_LOG_WARNING, "No directories found matching pattern '%s'", include_dir);
        }
    } else {
        if (ensure_capacity_charpp(ctx, &target->include_dirs, &target->include_count,
                               &target->include_cap) != 0) {
            return -1;
        }
        /* Normalize path separators for consistent signature comparison on Windows */
        target->include_dirs[target->include_count++] = cbuild__normalize_path(include_dir);
    }
    return 0;
}

int cbuild_add_library_dir(cbuild_context_t* ctx, target_t* target, const char* lib_dir) {
    if (!ctx || !target || !lib_dir) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_library_dir");
        return -1;
    }
    if (strchr(lib_dir, '*') || strchr(lib_dir, '?')) {
        char** expanded_dirs = NULL;
        int dir_count = 0;

        if (cbuild_expand_wildcard(lib_dir, &expanded_dirs, &dir_count) == 0 &&
            dir_count > 0) {
            for (int i = 0; i < dir_count; i++) {
                if (cbuild_dir_exists(expanded_dirs[i])) {
                    if (ensure_capacity_charpp(ctx, &target->lib_dirs, &target->lib_dir_count,
                                           &target->lib_dir_cap) != 0) {
                        for (int j = i; j < dir_count; j++) free(expanded_dirs[j]);
                        free(expanded_dirs);
                        return -1;
                    }
                    target->lib_dirs[target->lib_dir_count++] =
                        expanded_dirs[i];
                } else {
                    free(expanded_dirs[i]);
                }
            }
            free(expanded_dirs);
        } else {
            cbuild__log(ctx, CBUILD_LOG_WARNING, "No directories found matching pattern '%s'", lib_dir);
        }
    } else {
        if (ensure_capacity_charpp(ctx, &target->lib_dirs, &target->lib_dir_count,
                               &target->lib_dir_cap) != 0) {
            return -1;
        }
        /* Normalize path separators for consistent signature comparison on Windows */
        target->lib_dirs[target->lib_dir_count++] = cbuild__normalize_path(lib_dir);
    }
    return 0;
}

int cbuild_add_link_library(cbuild_context_t* ctx, target_t* target, const char* lib) {
    if (!ctx || !target || !lib) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_link_library");
        return -1;
    }
    if (strchr(lib, '*') || strchr(lib, '?')) {
        char** expanded_files = NULL;
        int file_count = 0;

        if (cbuild_expand_wildcard(lib, &expanded_files, &file_count) == 0 &&
            file_count > 0) {
            for (int i = 0; i < file_count; i++) {
                if (ensure_capacity_charpp(ctx, &target->link_libs, &target->link_lib_count,
                                       &target->link_lib_cap) != 0) {
                    for (int j = i; j < file_count; j++) free(expanded_files[j]);
                    free(expanded_files);
                    return -1;
                }
                target->link_libs[target->link_lib_count++] =
                    expanded_files[i];
            }
            free(expanded_files);
        } else {
            cbuild__log(ctx, CBUILD_LOG_WARNING, "No libraries found matching pattern '%s'", lib);
        }
    } else {
        if (ensure_capacity_charpp(ctx, &target->link_libs, &target->link_lib_count,
                               &target->link_lib_cap) != 0) {
            return -1;
        }
        /* Normalize path separators for consistent signature comparison on Windows */
        target->link_libs[target->link_lib_count++] = cbuild__normalize_path(lib);
    }
    return 0;
}

int cbuild_add_expose_library(cbuild_context_t* ctx, target_t* target, const char* lib_path) {
    if (!ctx || !target || !lib_path) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_expose_library");
        return -1;
    }

    /* Tokenize once and store tokens directly */
    char* copy = strdup(lib_path);
    if (!copy) {
        cbuild__set_error(ctx, "Out of memory in cbuild_add_expose_library");
        return -1;
    }

    char* p = copy;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        char* start = p;
        int in_quote = 0;

        while (*p) {
            if (*p == '"') {
                in_quote = !in_quote;
                p++;
            } else if (!in_quote && (*p == ' ' || *p == '\t')) {
                break;
            } else {
                p++;
            }
        }

        if (p > start) {
            char saved = *p;
            *p = '\0';

            char* token = start;
            size_t len = strlen(token);
            if (len >= 2 && token[0] == '"' && token[len - 1] == '"') {
                token[len - 1] = '\0';
                token++;
            }

            if (ensure_capacity_charpp(ctx, &target->exposed_libs, &target->exposed_lib_count,
                                   &target->exposed_lib_cap) != 0) {
                free(copy);
                return -1;
            }
            /* Normalize path separators for consistent signature comparison on Windows */
            target->exposed_libs[target->exposed_lib_count++] = cbuild__normalize_path(token);

            *p = saved;
        }
    }

    free(copy);
    return 0;
}

int cbuild_export_symbols(cbuild_context_t* ctx, target_t* target) {
    if (!ctx || !target) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_export_symbols");
        return -1;
    }

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
    return cbuild_add_ldflags(ctx, target, "-Wl,-E");
#elif defined(__APPLE__)
    return 0;
#elif defined(_WIN32)
    cbuild__log(ctx, CBUILD_LOG_WARNING, "cbuild_export_symbols() has limited effect on Windows. For MSVC, use /EXPORT:symbol or a .def file. For MinGW, consider cbuild_add_ldflags(ctx, target, \"-Wl,--export-all-symbols\").");
    return 0;
#else
    return 0;
#endif
}

int cbuild_add_link_target(cbuild_context_t* ctx, target_t* dependant, target_t* dependency) {
    if (!ctx || !dependant || !dependency) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_link_target");
        return -1;
    }

    if (ensure_capacity_charpp(ctx, (char***)&dependant->dependencies,
                               &dependant->dep_count, &dependant->dep_cap) != 0) {
        return -1;
    }
    dependant->dependencies[dependant->dep_count++] = dependency;
    return 0;
}

void cbuild_target_set_linker(cbuild_context_t* ctx, target_t* t, const char* linker_exe) {
    (void)ctx;
    if (!t) return;
    if (t->linker) free(t->linker);
    t->linker = linker_exe ? strdup(linker_exe) : NULL;
}

void cbuild_set_output_file(cbuild_context_t* ctx, target_t* t, const char* path) {
    (void)ctx;
    if (!t || !path) return;
    if (t->output_file) free(t->output_file);
    /* Normalize path separators for consistent path handling on Windows */
    t->output_file = cbuild__normalize_path(path);
    t->output_file_explicit = 1;
}

void cbuild_set_soname(cbuild_context_t* ctx, target_t* t, const char* soname) {
    (void)ctx;
    if (!t || !soname) return;
    if (t->soname) free(t->soname);
    t->soname = strdup(soname);
}

CBUILD_INTERNAL int cbuild__add_define_to_list(cbuild_context_t* ctx, char*** arr, int* count, int* cap,
                                                const char* macro,
                                                const char* value_optional) {
    char* entry = NULL;
    if (value_optional) {
        if (append_format(&entry, "%s=%s", macro, value_optional) != 0) {
            cbuild__set_error(ctx, "Out of memory in cbuild__add_define_to_list");
            return -1;
        }
    } else {
        entry = strdup(macro);
        if (!entry) {
            cbuild__set_error(ctx, "Out of memory in cbuild__add_define_to_list");
            return -1;
        }
    }

    if (ensure_capacity_charpp(ctx, arr, count, cap) != 0) {
        free(entry);
        return -1;
    }
    (*arr)[(*count)++] = entry;
    return 0;
}

int cbuild_add_define(cbuild_context_t* ctx, target_t* t, const char* macro) {
    if (!ctx || !t || !macro) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_define");
        return -1;
    }
    return cbuild__add_define_to_list(ctx, &t->defines, &t->define_count, &t->define_cap,
                                      macro, NULL);
}

int cbuild_add_define_val(cbuild_context_t* ctx, target_t* t, const char* macro, const char* val) {
    if (!ctx || !t || !macro) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_define_val");
        return -1;
    }
    return cbuild__add_define_to_list(ctx, &t->defines, &t->define_count, &t->define_cap,
                                      macro, val);
}

int cbuild_add_flag(cbuild_context_t* ctx, target_t* t, const char* flag, int value) {
    if (!ctx || !t || !flag) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_flag");
        return -1;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", !!value);
    return cbuild_add_define_val(ctx, t, flag, buf);
}

static target_t* cbuild_create_target(cbuild_context_t* ctx, const char* name,
                                      cbuild_target_type type) {
    target_t* t = (target_t*)calloc(1, sizeof(target_t));
    t->type = type;
    t->name = strdup(name);
    char *out = NULL, *obj = NULL;
    const char *output_dir = ctx->active_config ? (ctx->active_config->output_dir ? ctx->active_config->output_dir : ctx->output_dir) : ctx->output_dir;
    if (type == TARGET_EXECUTABLE) {
#ifdef _WIN32
        append_format(&out, "%s/%s.exe", output_dir, name);
#else
        append_format(&out, "%s/%s", output_dir, name);
#endif
    } else if (type == TARGET_STATIC_LIB) {
#ifdef _WIN32
        append_format(&out, "%s/%s.lib", output_dir, name);
#else
        append_format(&out, "%s/lib%s.a", output_dir, name);
#endif
    } else if (type == TARGET_SHARED_LIB) {
#ifdef _WIN32
        append_format(&out, "%s/%s.dll", output_dir, name);
#elif __APPLE__
        append_format(&out, "%s/lib%s.dylib", output_dir, name);
#else
        append_format(&out, "%s/lib%s.so", output_dir, name);
#endif
    }

    append_format(&obj, "%s/obj_%s", output_dir, name);
    t->output_file = out;
    t->obj_dir = obj;
    t->commands = NULL;
    t->cmd_count = t->cmd_cap = 0;
    t->post_commands = NULL;
    t->post_cmd_count = t->post_cmd_cap = 0;
    t->compiler = NULL;
    t->linker = NULL;
    t->cc_kind_override = -1;
    ensure_capacity_charpp(ctx, (char***)&ctx->targets, &ctx->target_count, &ctx->target_cap);
    ctx->targets[ctx->target_count++] = t;
    return t;
}

/* ---- src/test.c ---- */

/* test.c - test registration and the parallel test runner. */


enum { CBUILD_TEST_PENDING, CBUILD_TEST_RUNNING, CBUILD_TEST_DONE };
enum { CBUILD_TEST_PASS, CBUILD_TEST_FAIL, CBUILD_TEST_SKIP };

test_t* cbuild_add_test(cbuild_context_t* ctx, const char* name, target_t* target,
                        const char* const* args, int argc) {
    if (!ctx || !name || !*name || argc < 0 || (argc > 0 && !args)) {
        if (ctx) cbuild__set_error(ctx, "Invalid argument to cbuild_add_test");
        return NULL;
    }
    if (!(target && target->type == TARGET_EXECUTABLE) && argc == 0) {
        cbuild__set_error(ctx, "Test '%s' needs an executable target or a program in args[0]", name);
        return NULL;
    }
    test_t* test = (test_t*)calloc(1, sizeof(test_t));
    if (!test) {
        cbuild__set_error(ctx, "Out of memory in cbuild_add_test");
        return NULL;
    }
    test->name = strdup(name);
    test->target = target;
    test->skip_code = -1;
    test->argc = argc;
    test->args = (char**)calloc((size_t)argc + 1, sizeof(char*));
    for (int i = 0; i < argc; ++i) test->args[i] = strdup(args[i]);
    ensure_capacity_charpp(ctx, (char***)&ctx->tests, &ctx->test_count, &ctx->test_cap);
    ctx->tests[ctx->test_count++] = test;
    return test;
}

void cbuild_test_add_dependency(cbuild_context_t* ctx, test_t* test, target_t* target) {
    if (!test || !target) return;
    ensure_capacity_charpp(ctx, (char***)&test->deps, &test->dep_count, &test->dep_cap);
    test->deps[test->dep_count++] = target;
}

void cbuild_test_add_label(cbuild_context_t* ctx, test_t* test, const char* label) {
    if (!test || !label || !*label) return;
    ensure_capacity_charpp(ctx, &test->labels, &test->label_count, &test->label_cap);
    test->labels[test->label_count++] = strdup(label);
}

void cbuild_test_set_skip_code(cbuild_context_t* ctx, test_t* test, int exit_code) {
    (void)ctx;
    if (test) test->skip_code = exit_code;
}

void cbuild_test_set_timeout(cbuild_context_t* ctx, test_t* test, int seconds) {
    (void)ctx;
    if (test) test->timeout = seconds > 0 ? seconds : 0;
}

void cbuild_test_set_lock(cbuild_context_t* ctx, test_t* test, const char* lock) {
    (void)ctx;
    if (!test) return;
    free(test->lock);
    test->lock = (lock && *lock) ? strdup(lock) : NULL;
}

CBUILD_INTERNAL void cbuild__free_tests(cbuild_context_t* ctx) {
    for (int i = 0; i < ctx->test_count; ++i) {
        test_t* test = ctx->tests[i];
        if (!test) continue;
        free(test->name);
        for (int j = 0; j < test->argc; ++j) free(test->args[j]);
        free(test->args);
        free(test->deps);
        for (int j = 0; j < test->label_count; ++j) free(test->labels[j]);
        free(test->labels);
        free(test->lock);
        free(test);
    }
    free(ctx->tests);
    ctx->tests = NULL;
    ctx->test_count = 0;
    ctx->test_cap = 0;

    char*** lists[] = { &ctx->test_labels, &ctx->test_filters, &ctx->test_excludes };
    int* counts[] = { &ctx->test_label_count, &ctx->test_filter_count, &ctx->test_exclude_count };
    int* caps[] = { &ctx->test_label_cap, &ctx->test_filter_cap, &ctx->test_exclude_cap };
    for (int l = 0; l < 3; ++l) {
        for (int i = 0; i < *counts[l]; ++i) free((*lists[l])[i]);
        free(*lists[l]);
        *lists[l] = NULL;
        *counts[l] = 0;
        *caps[l] = 0;
    }
    ctx->run_tests = 0;
    ctx->list_tests = 0;
    ctx->test_timeout = 0;
}

/* Selected when it has one of the requested labels (if any), matches one of
 * the name filters (if any), and matches no exclusion. */
static int cbuild__test_selected(cbuild_context_t* ctx, test_t* test) {
    if (ctx->test_label_count > 0) {
        int found = 0;
        for (int i = 0; i < ctx->test_label_count && !found; ++i) {
            for (int j = 0; j < test->label_count; ++j) {
                if (strcmp(ctx->test_labels[i], test->labels[j]) == 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (!found) return 0;
    }
    if (ctx->test_filter_count > 0) {
        int found = 0;
        for (int i = 0; i < ctx->test_filter_count; ++i) {
            if (cbuild_match_wildcard(ctx->test_filters[i], test->name)) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    for (int i = 0; i < ctx->test_exclude_count; ++i) {
        if (cbuild_match_wildcard(ctx->test_excludes[i], test->name)) return 0;
    }
    return 1;
}

static const char* cbuild__test_program(test_t* test) {
    if (test->target && test->target->type == TARGET_EXECUTABLE) return test->target->output_file;
    return test->args[0];
}

static int cbuild__test_program_is_target(test_t* test) {
    return test->target && test->target->type == TARGET_EXECUTABLE;
}

CBUILD_INTERNAL void cbuild__list_tests(cbuild_context_t* ctx) {
    int shown = 0;
    cbuild__log(ctx, CBUILD_LOG_INFO, "Available tests:");
    for (int i = 0; i < ctx->test_count; ++i) {
        test_t* test = ctx->tests[i];
        if (!cbuild__test_selected(ctx, test)) continue;
        char* labels = NULL;
        for (int j = 0; j < test->label_count; ++j) {
            append_format(&labels, "%s%s", j ? "," : "", test->labels[j]);
        }
        if (labels) {
            cbuild__log(ctx, CBUILD_LOG_INFO, "  %-40s  [%s]", test->name, labels);
        } else {
            cbuild__log(ctx, CBUILD_LOG_INFO, "  %s", test->name);
        }
        free(labels);
        shown++;
    }
    if (shown == 0) cbuild__log(ctx, CBUILD_LOG_INFO, "  (no tests)");
}

/* ---------------- Running one test ---------------- */

static double cbuild__now_seconds(void) {
#ifdef _WIN32
    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

static void cbuild__sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

typedef struct {
    cbuild_context_t* ctx;
    test_t** tests;
    int* state;
    int* result;
    int count;
    int started;
    int finished;
    int passed, failed, skipped;
    char* log_dir;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CRITICAL_SECTION spawn_mutex;
#else
    pthread_mutex_t mutex;
#endif
} cbuild_test_run_t;

static void cbuild__test_run_lock(cbuild_test_run_t* run) {
#ifdef _WIN32
    EnterCriticalSection(&run->mutex);
#else
    pthread_mutex_lock(&run->mutex);
#endif
}

static void cbuild__test_run_unlock(cbuild_test_run_t* run) {
#ifdef _WIN32
    LeaveCriticalSection(&run->mutex);
#else
    pthread_mutex_unlock(&run->mutex);
#endif
}

/* Runs argv with stdin from the null device and stdout and stderr in log_path.
 * Returns the exit status, -1 if it could not be started, and sets *timed_out
 * when the process was killed for exceeding timeout seconds. */
static int cbuild__spawn_logged(cbuild_test_run_t* run, cbuild_argv_t* argv, const char* log_path,
                                int timeout, int* timed_out) {
    *timed_out = 0;
#ifdef _WIN32
    /* Handles marked inheritable leak into any process created meanwhile, so
     * they only exist inside this critical section. */
    EnterCriticalSection(&run->spawn_mutex);
    SECURITY_ATTRIBUTES sa = { 0 };
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE out = CreateFileA(log_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                            OPEN_EXISTING, 0, NULL);
    if (out == INVALID_HANDLE_VALUE || in == INVALID_HANDLE_VALUE) {
        if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
        if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
        LeaveCriticalSection(&run->spawn_mutex);
        return -1;
    }
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in;
    si.hStdOutput = out;
    si.hStdError = out;
    PROCESS_INFORMATION pi = { 0 };
    char* cmdline = cbuild_argv_to_cmdline(argv);
    BOOL ok = cmdline && CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    free(cmdline);
    CloseHandle(out);
    CloseHandle(in);
    LeaveCriticalSection(&run->spawn_mutex);
    if (!ok) return -1;

    DWORD wait = WaitForSingleObject(pi.hProcess, timeout > 0 ? (DWORD)timeout * 1000 : INFINITE);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        *timed_out = 1;
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    (void)run;
    /* O_CLOEXEC keeps these out of processes other workers start meanwhile;
     * dup2 in the child clears it on the standard descriptors. */
    int out = open(log_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    int in = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (out < 0 || in < 0) {
        if (out >= 0) close(out);
        if (in >= 0) close(in);
        return -1;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in, STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, out, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, out, STDERR_FILENO);
    pid_t pid;
    int spawn_rc = posix_spawnp(&pid, argv->args[0], &actions, NULL, argv->args, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(out);
    close(in);
    if (spawn_rc != 0) return -1;

    double deadline = timeout > 0 ? cbuild__now_seconds() + timeout : 0;
    int delay_ms = 1;
    int status = 0;
    for (;;) {
        pid_t r = waitpid(pid, &status, timeout > 0 ? WNOHANG : 0);
        if (r == pid) break;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (cbuild__now_seconds() >= deadline) {
            kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            *timed_out = 1;
            break;
        }
        cbuild__sleep_ms(delay_ms);
        if (delay_ms < 50) delay_ms *= 2;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
#endif
}

static char* cbuild__read_text_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* text = size >= 0 ? (char*)malloc((size_t)size + 1) : NULL;
    if (text) {
        size_t n = fread(text, 1, (size_t)size, f);
        text[n] = '\0';
    }
    fclose(f);
    return text;
}

static void cbuild__run_one_test(cbuild_test_run_t* run, int index) {
    cbuild_context_t* ctx = run->ctx;
    test_t* test = run->tests[index];

    char* log_path = NULL;
    append_format(&log_path, "%s/", run->log_dir);
    for (const char* p = test->name; *p; ++p) {
        char c = (isalnum((unsigned char)*p) || *p == '.' || *p == '-' || *p == '_') ? *p : '_';
        append_format(&log_path, "%c", c);
    }
    append_format(&log_path, ".log");

    cbuild_argv_t argv;
    cbuild_argv_init(&argv);
    char* program = strdup(cbuild__test_program(test));
#ifdef _WIN32
    /* CreateProcess resolves the program from the command line, which wants native separators. */
    for (char* p = program; p && *p; ++p) {
        if (*p == '/') *p = '\\';
    }
#endif
    cbuild_argv_append(&argv, program);
    free(program);
    for (int i = cbuild__test_program_is_target(test) ? 0 : 1; i < test->argc; ++i) {
        cbuild_argv_append(&argv, test->args[i]);
    }

    int timeout = test->timeout > 0 ? test->timeout : ctx->test_timeout;
    int timed_out = 0;
    double start = cbuild__now_seconds();
    int rc = cbuild__spawn_logged(run, &argv, log_path, timeout, &timed_out);
    double elapsed = cbuild__now_seconds() - start;
    cbuild_argv_free(&argv);

    int result = CBUILD_TEST_FAIL;
    char detail[128] = "";
    if (timed_out) {
        snprintf(detail, sizeof(detail), ", timed out after %ds", timeout);
    } else if (rc == 0) {
        result = CBUILD_TEST_PASS;
    } else if (test->skip_code >= 0 && rc == test->skip_code) {
        result = CBUILD_TEST_SKIP;
    } else if (rc < 0) {
        snprintf(detail, sizeof(detail), ", could not start %s", cbuild__test_program(test));
    } else {
        snprintf(detail, sizeof(detail), ", exit code %d", rc);
    }

    cbuild__test_run_lock(run);
    run->result[index] = result;
    run->state[index] = CBUILD_TEST_DONE;
    run->finished++;
    if (result == CBUILD_TEST_PASS) run->passed++;
    if (result == CBUILD_TEST_FAIL) run->failed++;
    if (result == CBUILD_TEST_SKIP) run->skipped++;
    const char* label = result == CBUILD_TEST_PASS ? "PASS" : result == CBUILD_TEST_SKIP ? "SKIP" : "FAIL";
    cbuild__log_step(ctx, label, CBUILD_COLOR_BLUE, "[%d/%d] %s (%.2fs%s)", run->finished, run->count,
                     test->name, elapsed, detail);
    if (result == CBUILD_TEST_FAIL) {
        char* text = cbuild__read_text_file(log_path);
        if (text && *text) {
            fwrite(text, 1, strlen(text), stderr);
            if (text[strlen(text) - 1] != '\n') fputc('\n', stderr);
            fflush(stderr);
        }
        free(text);
    }
    cbuild__test_run_unlock(run);
    free(log_path);
}

/* Next pending test whose lock no running test holds, or -1. Caller holds the mutex. */
static int cbuild__next_runnable_test(cbuild_test_run_t* run) {
    for (int i = 0; i < run->count; ++i) {
        if (run->state[i] != CBUILD_TEST_PENDING) continue;
        const char* lock = run->tests[i]->lock;
        int blocked = 0;
        for (int j = 0; lock && j < run->count && !blocked; ++j) {
            if (run->state[j] == CBUILD_TEST_RUNNING && run->tests[j]->lock &&
                strcmp(run->tests[j]->lock, lock) == 0) {
                blocked = 1;
            }
        }
        if (!blocked) return i;
    }
    return -1;
}

#ifdef _WIN32
static DWORD WINAPI cbuild__test_worker(void* arg) {
#else
static void* cbuild__test_worker(void* arg) {
#endif
    cbuild_test_run_t* run = (cbuild_test_run_t*)arg;
    for (;;) {
        cbuild__test_run_lock(run);
        if (run->started >= run->count) {
            cbuild__test_run_unlock(run);
            break;
        }
        int index = cbuild__next_runnable_test(run);
        if (index >= 0) {
            run->state[index] = CBUILD_TEST_RUNNING;
            run->started++;
        }
        cbuild__test_run_unlock(run);
        if (index < 0) {
            cbuild__sleep_ms(5);  /* everything left waits on a lock */
            continue;
        }
        cbuild__run_one_test(run, index);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* Builds what the selected tests need, then runs them. Returns 0 when nothing failed. */
CBUILD_INTERNAL int cbuild__run_selected_tests(cbuild_context_t* ctx) {
    cbuild_test_run_t run;
    memset(&run, 0, sizeof(run));
    run.ctx = ctx;
    run.tests = (test_t**)calloc((size_t)ctx->test_count + 1, sizeof(test_t*));
    for (int i = 0; i < ctx->test_count; ++i) {
        if (cbuild__test_selected(ctx, ctx->tests[i])) run.tests[run.count++] = ctx->tests[i];
    }
    if (run.count == 0) {
        cbuild__log(ctx, CBUILD_LOG_WARNING, "cbuild: no tests selected");
        free(run.tests);
        return 0;
    }

    /* Build first: a test can only start once everything it needs exists. */
    int error_flag = 0;
    free(ctx->visited);
    free(ctx->in_stack);
    ctx->visited = (int*)calloc((size_t)ctx->target_count + 1, sizeof(int));
    ctx->in_stack = (int*)calloc((size_t)ctx->target_count + 1, sizeof(int));
    for (int i = 0; i < run.count && !error_flag; ++i) {
        if (run.tests[i]->target) dfs_build_func(ctx, run.tests[i]->target, &error_flag);
        for (int j = 0; j < run.tests[i]->dep_count && !error_flag; ++j) {
            dfs_build_func(ctx, run.tests[i]->deps[j], &error_flag);
        }
    }
    free(ctx->visited);
    ctx->visited = NULL;
    free(ctx->in_stack);
    ctx->in_stack = NULL;
    if (error_flag) {
        cbuild__log_status(ctx, 0, "Build failed; tests not run.");
        free(run.tests);
        return -1;
    }

    const char* output_dir = ctx->active_config && ctx->active_config->output_dir
                                 ? ctx->active_config->output_dir
                                 : ctx->output_dir;
    append_format(&run.log_dir, "%s/tests", output_dir);
    ensure_dir_exists(run.log_dir);

    run.state = (int*)calloc((size_t)run.count, sizeof(int));
    run.result = (int*)calloc((size_t)run.count, sizeof(int));
    int thread_count = ctx->parallel_jobs < run.count ? ctx->parallel_jobs : run.count;
    if (thread_count < 1) thread_count = 1;
#ifdef _WIN32
    /* WaitForMultipleObjects joins at most this many threads. */
    if (thread_count > MAXIMUM_WAIT_OBJECTS) thread_count = MAXIMUM_WAIT_OBJECTS;
#endif

    cbuild__log_step(ctx, "TEST", CBUILD_COLOR_BLUE, "Running %d test%s with %d job%s", run.count,
                     run.count == 1 ? "" : "s", thread_count, thread_count == 1 ? "" : "s");
    double start = cbuild__now_seconds();

#ifdef _WIN32
    InitializeCriticalSection(&run.mutex);
    InitializeCriticalSection(&run.spawn_mutex);
    HANDLE* threads = (HANDLE*)malloc(sizeof(HANDLE) * thread_count);
    for (int i = 0; i < thread_count; ++i) {
        threads[i] = CreateThread(NULL, 0, cbuild__test_worker, &run, 0, NULL);
    }
    WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
    for (int i = 0; i < thread_count; ++i) CloseHandle(threads[i]);
    DeleteCriticalSection(&run.spawn_mutex);
    DeleteCriticalSection(&run.mutex);
#else
    pthread_mutex_init(&run.mutex, NULL);
    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    for (int i = 0; i < thread_count; ++i) {
        pthread_create(&threads[i], NULL, cbuild__test_worker, &run);
    }
    for (int i = 0; i < thread_count; ++i) pthread_join(threads[i], NULL);
    pthread_mutex_destroy(&run.mutex);
#endif
    free(threads);

    double elapsed = cbuild__now_seconds() - start;
    if (run.failed > 0) {
        cbuild__log(ctx, CBUILD_LOG_INFO, "Failed tests (logs in %s):", run.log_dir);
        for (int i = 0; i < run.count; ++i) {
            if (run.result[i] == CBUILD_TEST_FAIL) cbuild__log(ctx, CBUILD_LOG_INFO, "  %s", run.tests[i]->name);
        }
    }
    cbuild__log_status(ctx, run.failed == 0, "Tests: %d passed, %d failed, %d skipped (%.1fs)", run.passed,
                       run.failed, run.skipped, elapsed);

    int failed = run.failed;
    free(run.state);
    free(run.result);
    free(run.tests);
    free(run.log_dir);
    return failed ? -1 : 0;
}

int cbuild_run_tests(cbuild_context_t* ctx, const char* label, const char* pattern) {
    if (!ctx) return -1;
    cbuild_init(ctx);
    cbuild__resolve_target_paths(ctx);

    /* Narrow the selection for this call only. */
    int added_label = 0, added_filter = 0;
    if (label && *label) {
        ensure_capacity_charpp(ctx, &ctx->test_labels, &ctx->test_label_count, &ctx->test_label_cap);
        ctx->test_labels[ctx->test_label_count++] = strdup(label);
        added_label = 1;
    }
    if (pattern && *pattern) {
        ensure_capacity_charpp(ctx, &ctx->test_filters, &ctx->test_filter_count, &ctx->test_filter_cap);
        ctx->test_filters[ctx->test_filter_count++] = strdup(pattern);
        added_filter = 1;
    }
    int rc = cbuild__run_selected_tests(ctx);
    if (added_label) free(ctx->test_labels[--ctx->test_label_count]);
    if (added_filter) free(ctx->test_filters[--ctx->test_filter_count]);
    return rc;
}

/* ---- src/toolchain.c ---- */

/* toolchain.c - compiler, linker, and archiver detection. */


CBUILD_INTERNAL cbuild_cc_kind_t detect_cc_kind(const char* exe) {
    if (!exe) return CBUILD_CC_GCC_CLANG;
    const char* base = strrchr(exe, '/');
    if (!base) base = exe;
    const char* b2 = strrchr(base, '\\');
    if (b2) base = b2 + 1;
    if (strstr(base, "clang")) return CBUILD_CC_GCC_CLANG;
    if (strstr(base, "cl")) return CBUILD_CC_MSVC;
    if (strstr(base, "icl")) return CBUILD_CC_MSVC;
    return CBUILD_CC_GCC_CLANG;
}

/* Detect if archiver is MSVC-style (lib.exe) or Unix-style (ar, zig ar, llvm-ar, etc.) */
CBUILD_INTERNAL cbuild_cc_kind_t detect_ar_kind(const char* ar) {
    if (!ar) return CBUILD_CC_GCC_CLANG;
    const char* base = strrchr(ar, '/');
    if (!base) base = ar;
    const char* b2 = strrchr(base, '\\');
    if (b2) base = b2 + 1;
    /* MSVC archiver is "lib" or "lib.exe" */
    if (strcmp(base, "lib") == 0 || strcmp(base, "lib.exe") == 0) return CBUILD_CC_MSVC;
    /* Everything else (ar, zig ar, llvm-ar, etc.) is Unix-style */
    return CBUILD_CC_GCC_CLANG;
}

/* Helper to resolve compiler for a target */
CBUILD_INTERNAL const char* cbuild_target_compiler(cbuild_context_t* ctx, target_t* t) {
    return t->compiler ? t->compiler : ctx->cc;
}

CBUILD_INTERNAL cbuild_cc_kind_t cbuild_target_cc_kind(cbuild_context_t* ctx, target_t* t) {
    return (t->cc_kind_override != -1) ? (cbuild_cc_kind_t)t->cc_kind_override : ctx->cc_kind;
}

CBUILD_INTERNAL const char* cbuild_target_linker(cbuild_context_t* ctx, target_t* t) {
    return t->linker ? t->linker : ctx->ld;
}

void cbuild_guess_compiler(cbuild_context_t* ctx) {
    #if defined(_WIN32)
        cbuild_set_compiler(ctx, "cl");
    #elif defined(__APPLE__)
        cbuild_set_compiler(ctx, "clang");
    #else
        cbuild_set_compiler(ctx, "cc");
    #endif
}

/* ---- src/util.c ---- */

/* util.c - small string and array helpers. */


CBUILD_INTERNAL void cbuild__trim(char* s) {
    char* end;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
        s++;
    end = s + strlen(s) - 1;
    while (end > s &&
           (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        *end-- = 0;
}

CBUILD_INTERNAL int ensure_capacity_charpp(cbuild_context_t* ctx, char*** arr, int* count, int* capacity) {
    if (*count + 1 > *capacity) {
        int newcap = *capacity ? *capacity * 2 : 4;
        char** newarr = (char**)realloc(*arr, newcap * sizeof(char*));
        if (!newarr) {
            cbuild__set_error(ctx, "Out of memory allocating %d pointers", newcap);
            cbuild__log(ctx, CBUILD_LOG_ERROR, "cbuild: Out of memory");
            return -1;
        }
        *arr = newarr;
        *capacity = newcap;
    }
    return 0;
}

CBUILD_INTERNAL int append_format(char** dst, const char* fmt, ...) {
    va_list args;
    va_list args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    int add_len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (add_len < 0) {
        va_end(args);
        return -1;
    }
    size_t old_len = *dst ? strlen(*dst) : 0;
    char* newptr = (char*)realloc(*dst, old_len + add_len + 1);
    if (!newptr) {
        va_end(args);
        return -1;
    }
    *dst = newptr;
    vsnprintf(*dst + old_len, add_len + 1, fmt, args);
    va_end(args);
    return 0;
}

// --- small helpers for file suffix checks ------------------------------------
CBUILD_INTERNAL int cbuild__has_suffix(const char* s, const char* suf) {
    if (!s || !suf) return 0;
    size_t ls = strlen(s), lt = strlen(suf);
    if (lt > ls) return 0;
    return strcasecmp(s + (ls - lt), suf) == 0;
}

CBUILD_INTERNAL unsigned cbuild__hash_path(const char* s) {
    unsigned h = 5381u;
    int c;
    while ((c = (unsigned char)*s++)) {
        h = ((h << 5) + h) ^ (unsigned)c; /* h*33 ^ c */
    }
    return h;
}

CBUILD_INTERNAL void fprint_json_string(FILE* f, const char* s) {
    fputc('"', f);
    for (; *s; ++s) {
        switch (*s) {
            case '\\':
                fputs("\\\\", f);
                break;
            case '"':
                fputs("\\\"", f);
                break;
            case '\b':
                fputs("\\b", f);
                break;
            case '\f':
                fputs("\\f", f);
                break;
            case '\n':
                fputs("\\n", f);
                break;
            case '\r':
                fputs("\\r", f);
                break;
            case '\t':
                fputs("\\t", f);
                break;
            default:
                if ((unsigned char)*s < 0x20) {
                    fprintf(f, "\\u%04x", (unsigned char)*s);
                } else {
                    fputc(*s, f);
                }
        }
    }
    fputc('"', f);
}

#endif /* CBUILD_IMPLEMENTATION */
