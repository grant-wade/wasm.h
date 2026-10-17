/* Bootstrap once with: cc -o cbuild build.c (Windows: cl /nologo /Fe:cbuild.exe build.c) */
#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define EXE_SUFFIX ".exe"
#define PATH_LIST_SEPARATOR ';'
#else
#define EXE_SUFFIX ""
#define PATH_LIST_SEPARATOR ':'
#endif

static const char* const wasm_runtime_sources[] = {
    "src/call.c",
    "src/decode.c",
    "src/exec/calls.c",
    "src/exec/control.c",
    "src/exec/exceptions.c",
    "src/exec/frames.c",
    "src/exec/gc.c",
    "src/exec/bulk.c",
    "src/exec/simd.c",
    "src/exec/numeric.c",
    "src/exec/interpreter.c",
    "src/gc.c",
    "src/init_expr.c",
    "src/instructions.c",
    "src/inspect.c",
    "src/load.c",
    "src/memory.c",
    "src/memory_internal.c",
    "src/module.c",
    "src/platform/disabled.c",
    "src/platform/emscripten.c",
    "src/platform/host.c",
    "src/platform/wasi_bind.c",
    "src/platform/wasi_fd.c",
    "src/platform/wasi_io.c",
    "src/platform/wasi_path.c",
    "src/platform/wasi_state.c",
    "src/portable.c",
    "src/reader.c",
    "src/runtime.c",
    "src/simd/compare.c",
    "src/simd/float.c",
    "src/simd/integer_small.c",
    "src/simd/integer_wide.c",
    "src/simd/lanes.c",
    "src/simd/native.c",
    "src/support.c",
    "src/types/canonical.c",
    "src/types/cross_module.c",
    "src/types/decode.c",
    "src/types/references.c",
    "src/validate/function.c",
    "src/validate/gc.c",
    "src/validate/module.c",
    "src/validate/numeric.c",
    "src/validate/simd.c",
    "src/validate/stack.c",
    "src/values.c",
};

/* test/fixtures/<name>.c, fixture_runner arguments, and emcc flags. */
typedef struct {
    const char* name;
    const char* runner_args[4];
    const char* emcc_flags[3];
} emcc_case_t;

static const emcc_case_t emcc_cases[] = {
    { "basic_add", { "call-i32", "wl_case", "10" }, { "-O0" } },
    { "function_pointer", { "call-i32", "wl_case", "37" }, { "-O1" } },
    { "bulk_memory", { "call-i32", "wl_case", "410" }, { "-O0", "-mbulk-memory" } },
    { "sign_extension", { "call-i32", "wl_case", "-32895" }, { "-O2" } },
    { "switch_table", { "call-i32", "wl_case", "40" }, { "-O2" } },
    { "start_constructor", { "call-i32", "wl_case", "77" }, { "-O0", "-Wl,--export=__wasm_call_ctors" } },
    { "i64_return", { "call-i64", "wl_case", "1099511627785" }, { "-O1" } },
    { "trunc_sat", { "call-i32", "wl_case", "255" }, { "-O2" } },
    { "tail_call", { "call-i32", "wl_case", "29" }, { "-O1", "-mtail-call" } },
    { "stdout_print", { "call-stdout", "wl_case", "stdout-fixture-ok" }, { "-O0" } },
    { "unresolved_import", { "load-fail" }, { "-O0", "-Wl,--allow-undefined" } },
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

typedef struct {
    int release;
    int platform;
    int werror;
    int emcc;
    int fuzzer;
    int spec_proposals; /* also run proposal suites the runtime does not implement */
    int announce; /* report missing optional tools */
    int msvc;
    int x86_32;
    const char* cc;
    const char* build_dir;
    char* cflags;  /* $CFLAGS then --cflags */
    char* ldflags; /* $LDFLAGS then --ldflags */
} options_t;

static options_t opt;
static cbuild_context_t* ctx;

static char* fmt(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);
    char* s = (char*)malloc((size_t)n + 1);
    if (!s) exit(1);
    va_start(args, format);
    vsnprintf(s, (size_t)n + 1, format, args);
    va_end(args);
    return s;
}

static char* join_flags(const char* a, const char* b) {
    if (!a || !*a) return b ? fmt("%s", b) : NULL;
    if (!b || !*b) return fmt("%s", a);
    return fmt("%s %s", a, b);
}

static char* out_path(const char* name) {
    return fmt("%s/%s", opt.build_dir, name);
}

static char* exe_path(const char* name) {
    return fmt("%s/%s" EXE_SUFFIX, opt.build_dir, name);
}

static void make_dirs(const char* path) {
    char* p = fmt("%s", path);
    for (char* c = p + 1;; ++c) {
        if (*c == '/' || *c == '\\' || *c == '\0') {
            char saved = *c;
            *c = '\0';
#ifdef _WIN32
            _mkdir(p);
#else
            mkdir(p, 0755);
#endif
            *c = saved;
            if (!saved) break;
        }
    }
    free(p);
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* text = (char*)malloc(size > 0 ? (size_t)size + 1 : 1);
    size_t n = text ? fread(text, 1, size > 0 ? (size_t)size : 0, f) : 0;
    if (text) text[n] = '\0';
    fclose(f);
    return text;
}

static int write_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = strlen(text);
    int ok = fwrite(text, 1, n, f) == n;
    return fclose(f) == 0 && ok ? 0 : -1;
}

static void write_if_changed(const char* path, const char* text) {
    char* old = read_file(path);
    if (!old || strcmp(old, text) != 0) write_file(path, text);
    free(old);
}

static int split_flags(const char* flags, const char** argv, int max) {
    int n = 0;
    if (!flags) return 0;
    const char* p = flags;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        argv[n++] = fmt("%.*s", (int)(p - start), start);
    }
    return n;
}

static int is_msvc_compiler(const char* cc) {
    const char* base = cc;
    for (const char* p = cc; *p && *p != ' '; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    if (strstr(base, "clang")) return 0;
    return strncmp(base, "cl", 2) == 0 && (base[2] == '\0' || base[2] == ' ' || base[2] == '.');
}

static char* find_program(const char* const* names, int name_count, const char* const* hints, int hint_count) {
    static const char* const suffixes[] = {
#ifdef _WIN32
        ".exe", ".bat", ".cmd",
#endif
        "",
    };
    const char* path_env = getenv("PATH");
    char* dirs = fmt("%s", path_env ? path_env : "");
    for (int pass = 0; pass < 2; ++pass) {
        const char* p = dirs;
        int hint = 0;
        for (;;) {
            char* dir;
            if (pass == 0) {
                if (hint >= hint_count) break;
                if (!hints[hint] || !*hints[hint]) {
                    hint++;
                    continue;
                }
                dir = fmt("%s", hints[hint++]);
            } else {
                if (!*p) break;
                const char* end = strchr(p, PATH_LIST_SEPARATOR);
                size_t len = end ? (size_t)(end - p) : strlen(p);
                dir = fmt("%.*s", (int)len, p);
                p += len + (end ? 1 : 0);
            }
            for (int n = 0; n < name_count; ++n) {
                for (int s = 0; s < COUNT(suffixes); ++s) {
                    char* candidate = fmt("%s/%s%s", *dir ? dir : ".", names[n], suffixes[s]);
                    if (cbuild_file_exists(candidate)) {
                        free(dir);
                        free(dirs);
                        return candidate;
                    }
                    free(candidate);
                }
            }
            free(dir);
        }
    }
    free(dirs);
    return NULL;
}

static int program_succeeds(const char* const* argv, int argc) {
    char* output = NULL;
    int rc = cbuild_spawn(ctx, argv, argc, &output);
    free(output);
    return rc == 0;
}

/* Wasm needs binary32/binary64 rounding, not x87 extended precision. Probe the
 * target so -m32 on a 64-bit host counts but 32-bit ARM does not. */
static int compiler_targets_x86_32(void) {
    if (opt.msvc) return 0;
    char* dir = out_path("probe");
    make_dirs(dir);
    char* source = fmt("%s/x86_32.c", dir);
    char* object = fmt("%s/x86_32.o", dir);
    write_if_changed(source, "#ifndef __i386__\n#error Not a 32-bit x86 target\n#endif\nint main(void) { return 0; }\n");
    const char* argv[128];
    int argc = split_flags(opt.cc, argv, 16);
    argc += split_flags(opt.cflags, argv + argc, 100);
    argv[argc++] = "-c";
    argv[argc++] = source;
    argv[argc++] = "-o";
    argv[argc++] = object;
    return program_succeeds(argv, argc);
}

static target_t* native_targets[64];
static int native_target_count;

static void base_options(target_t* t) {
    native_targets[native_target_count++] = t;
    if (opt.msvc) {
        cbuild_add_cflags(ctx, t, opt.release ? "/O2 /DNDEBUG /MD" : "/Z7 /Od /RTC1 /MDd");
    } else {
        cbuild_add_cflags(ctx, t, "-std=c99");
        cbuild_add_cflags(ctx, t, opt.release ? "-O3 -DNDEBUG" : "-g");
        if (opt.x86_32) cbuild_add_cflags(ctx, t, "-msse2 -mfpmath=sse");
        cbuild_add_link_library(ctx, t, "m");
    }
}

static void warning_options(target_t* t) {
    if (opt.msvc) {
        cbuild_add_cflags(ctx, t, opt.werror ? "/std:c11 /W4 /WX" : "/std:c11 /W4");
    } else {
        cbuild_add_cflags(ctx, t, opt.werror ? "-Wall -Wextra -Werror" : "-Wall -Wextra");
    }
}

static void project_options(target_t* t) {
    base_options(t);
    warning_options(t);
    cbuild_add_include_dir(ctx, t, ".");
    cbuild_add_define(ctx, t, "_POSIX_C_SOURCE=200809L");
    cbuild_add_define(ctx, t, opt.platform ? "WASM_ENABLE_PLATFORM=1" : "WASM_ENABLE_PLATFORM=0");
    cbuild_add_define(ctx, t, opt.platform ? "WL_ENABLE_PLATFORM=1" : "WL_ENABLE_PLATFORM=0");
    if (opt.msvc) {
        cbuild_add_define(ctx, t, "_CRT_SECURE_NO_WARNINGS");
        cbuild_add_define(ctx, t, "_CRT_RAND_S");
    }
}

static target_t* runtime_library(const char* name) {
    target_t* t = cbuild_static_library(ctx, name);
    for (int i = 0; i < COUNT(wasm_runtime_sources); ++i) cbuild_add_source(ctx, t, wasm_runtime_sources[i]);
    project_options(t);
    cbuild_add_include_dir(ctx, t, "src");
    if (opt.msvc) {
        /* SIMD fast paths and platform branches keep unreachable portable fallbacks. */
        cbuild_add_cflags(ctx, t, "/wd4702");
    } else {
        cbuild_add_cflags(ctx, t, "-Wmissing-prototypes");
    }
    return t;
}

static target_t* native_executable(const char* name, const char* source, target_t* header) {
    target_t* t = cbuild_executable(ctx, name);
    cbuild_add_source(ctx, t, source);
    project_options(t);
    cbuild_add_link_target(ctx, t, header);
    return t;
}

static test_t* add_test(const char* name, target_t* target, const char* const* args, int argc,
                        const char* label, const char* label2) {
    test_t* test = cbuild_add_test(ctx, name, target, args, argc);
    cbuild_test_add_label(ctx, test, label);
    if (label2) cbuild_test_add_label(ctx, test, label2);
    return test;
}

typedef struct {
    const char* from;
    const char* to;
} copy_job_t;

static void copy_executable(void* user_data) {
    copy_job_t* job = (copy_job_t*)user_data;
    FILE* in = fopen(job->from, "rb");
    if (!in) return;
    char* dir = fmt("%s", job->to);
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        make_dirs(dir);
    }
    free(dir);
    FILE* out = fopen(job->to, "wb");
    if (out) {
        char buffer[65536];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) fwrite(buffer, 1, n, out);
        fclose(out);
#ifndef _WIN32
        chmod(job->to, 0755);
#endif
    }
    fclose(in);
}

static void touch_stamp(void* user_data) {
    write_file((const char*)user_data, "generated\n");
}

static void make_dir_command(void* user_data) {
    make_dirs((const char*)user_data);
}

typedef struct {
    const char* wasm_tools;
    const char* wast;
    const char* json;
    const char* status;
    const char* log;
    const char* dir;
} wast_job_t;

/* A failed conversion leaves an empty command list and a status the runner reports as a skip. */
static void json_from_wast(void* user_data) {
    wast_job_t* job = (wast_job_t*)user_data;
    make_dirs(job->dir);
    const char* argv[] = { job->wasm_tools, "json-from-wast", job->wast, "-o", job->json, "--wasm-dir", job->dir };
    char* output = NULL;
    int rc = cbuild_spawn(ctx, argv, COUNT(argv), &output);
    write_file(job->log, output ? output : "");
    if (rc == 0) {
        write_file(job->status, "ok\n");
    } else {
        remove(job->json);
        char* stub = fmt("{\n  \"source_filename\": \"%s\",\n  \"commands\": []\n}\n", job->wast);
        char* status = fmt("skip\njson-from-wast failed for %s\n%s", job->wast, output ? output : "");
        write_file(job->json, stub);
        write_file(job->status, status);
        free(stub);
        free(status);
    }
    free(output);
}

/* Each header compiles alone, so transitive includes cannot hide missing prerequisites. */
static void private_header_check(void) {
    target_t* t = cbuild_static_library(ctx, "wasm_private_header_check");
    char* dir = out_path("header-check");
    make_dirs(dir);
    char** headers = NULL;
    int header_count = 0;
    cbuild_expand_wildcard("src/**/*.h", &headers, &header_count);
    for (int i = 0; i < header_count; ++i) {
        const char* relative = headers[i] + strlen("src/");
        if (strcmp(relative, "amalgamation.h") == 0) continue;
        char* id = fmt("%s", relative);
        for (char* c = id; *c; ++c) {
            if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))) *c = '_';
        }
        char* source = fmt("%s/%s.c", dir, id);
        char* text = fmt("#include \"%s\"\n", relative);
        write_if_changed(source, text);
        cbuild_add_source(ctx, t, source);
        free(id);
        free(source);
        free(text);
        free(headers[i]);
    }
    free(headers);
    project_options(t);
    cbuild_add_include_dir(ctx, t, "src");
}

typedef struct {
    target_t* runner;
    target_t* modular_runner;
    target_t* support_build;
    const char* wasm_tools;
    const char* support;
} spec_env_t;

/* Proposal directories under test/spec/proposals whose suites run by default. */
static const char* const implemented_proposals[] = { NULL };

static int proposal_implemented(const char* proposal) {
    for (int i = 0; implemented_proposals[i]; ++i) {
        if (strcmp(implemented_proposals[i], proposal) == 0) return 1;
    }
    return 0;
}

/* skip_status, when set, replaces the conversion status so the runner skips the test.
   features, when set, is passed to wasm-tools validate. */
static void add_spectest(const spec_env_t* env, const char* wast, const char* out_dir, const char* stem,
                         const char* test_name, const char* skip_status, const char* features) {
    wast_job_t* job = (wast_job_t*)calloc(1, sizeof(*job));
    job->wasm_tools = env->wasm_tools;
    job->wast = wast;
    job->json = fmt("%s/%s.json", out_dir, stem);
    job->status = fmt("%s/%s.status", out_dir, stem);
    job->log = fmt("%s/%s.json-from-wast.log", out_dir, stem);
    job->dir = out_dir;

    target_t* json = cbuild_file_dep_target(ctx, fmt("spectest-json.%s", test_name), job->json);
    cbuild_add_source(ctx, json, wast);
    cbuild_target_add_command(ctx, json,
                              cbuild_command_function(ctx, fmt("json-from-wast %s", wast), json_from_wast, job));

    /* Both runners compile the same text fixtures to the same paths. */
    const char* args[] = { job->json, env->wasm_tools, env->support, skip_status ? skip_status : job->status,
                           features };
    int argc = features ? 5 : 4;
    const char* label2 = strncmp(test_name, "proposal.", 9) == 0 ? "proposal" : NULL;
    char* lock = fmt("spectest_%s", test_name);
    test_t* single = add_test(fmt("spectest.%s", test_name), env->runner, args, argc, "spectest", label2);
    test_t* modular = add_test(fmt("spectest.modular.%s", test_name), env->modular_runner, args, argc, "spectest",
                               "modular");
    if (label2) cbuild_test_add_label(ctx, modular, label2);
    test_t* pair[] = { single, modular };
    for (int p = 0; p < 2; ++p) {
        if (!skip_status) cbuild_test_add_dependency(ctx, pair[p], json);
        cbuild_test_add_dependency(ctx, pair[p], env->support_build);
        cbuild_test_set_skip_code(ctx, pair[p], 125);
        cbuild_test_set_lock(ctx, pair[p], lock);
    }
    free(lock);
}

static void spectests(target_t* runner, target_t* modular_runner) {
    static const char* const names[] = { "wasm-tools" };
    char* wasm_tools = find_program(names, 1, NULL, 0);
    if (!wasm_tools) {
        if (opt.announce) printf("wasm-tools not found; skipping specification tests.\n");
        return;
    }
    const char* json_help[] = { wasm_tools, "json-from-wast", "--help" };
    const char* parse_help[] = { wasm_tools, "parse", "--help" };
    if (!program_succeeds(json_help, 3) || !program_succeeds(parse_help, 3)) {
        if (opt.announce) {
            printf("wasm-tools found at %s but required subcommands are unavailable; skipping specification tests.\n",
                   wasm_tools);
        }
        return;
    }

    char* support = out_path("spectest_support.wasm");
    target_t* support_build = cbuild_file_dep_target(ctx, "spectest-support", support);
    cbuild_add_source(ctx, support_build, "test/spectest_support.wat");
    char* parse_argv[] = { wasm_tools, "parse", "test/spectest_support.wat", "-o", support };
    cbuild_target_add_command(ctx, support_build, cbuild_command_argv(ctx, "wasm-tools parse spectest_support.wat",
                                                                      parse_argv, COUNT(parse_argv)));

    spec_env_t env = { runner, modular_runner, support_build, wasm_tools, support };
    char* spec_dir = out_path("spec");
    char** wasts = NULL;
    int wast_count = 0;
    cbuild_expand_wildcard("test/spec/*.wast", &wasts, &wast_count);
    for (int i = 0; i < wast_count; ++i) {
        const char* base = strrchr(wasts[i], '/') + 1;
        char* stem = fmt("%.*s", (int)(strlen(base) - strlen(".wast")), base);
        add_spectest(&env, wasts[i], spec_dir, stem, stem, NULL, NULL);
        free(stem);
    }
    free(wasts);

    const char* proposals_root = "test/spec/proposals/";
    char** proposal_wasts = NULL;
    int proposal_count = 0;
    cbuild_expand_wildcard("test/spec/proposals/**/*.wast", &proposal_wasts, &proposal_count);
    for (int i = 0; i < proposal_count; ++i) {
        const char* relative = proposal_wasts[i] + strlen(proposals_root);
        const char* slash = strchr(relative, '/');
        if (strncmp(proposal_wasts[i], proposals_root, strlen(proposals_root)) != 0 || !slash) continue;
        char* proposal = fmt("%.*s", (int)(slash - relative), relative);
        char* stem = fmt("%.*s", (int)(strlen(slash + 1) - strlen(".wast")), slash + 1);
        for (char* c = stem; *c; ++c) {
            if (*c == '/') *c = '.';
        }
        char* out_dir = fmt("%s/proposals/%s", spec_dir, proposal);
        char* test_name = fmt("proposal.%s.%s", proposal, stem);
        char* skip_status = NULL;
        if (!proposal_implemented(proposal) && !opt.spec_proposals) {
            make_dirs(out_dir);
            skip_status = fmt("%s/not-implemented.status", out_dir);
            char* text = fmt("skip\nproposal %s is not implemented; pass --spec-proposals to run it\n", proposal);
            write_if_changed(skip_status, text);
            free(text);
        }
        add_spectest(&env, proposal_wasts[i], out_dir, stem, test_name, skip_status, "all");
        free(proposal);
        free(stem);
        free(test_name);
    }
    free(proposal_wasts);
}

static char* find_emcc(void) {
    static const char* const names[] = { "emcc" };
    const char* emsdk = getenv("EMSDK");
    const char* hints[] = { emsdk, emsdk ? fmt("%s/upstream/emscripten", emsdk) : NULL, getenv("EMSCRIPTEN") };
    return find_program(names, 1, hints, COUNT(hints));
}

static void emcc_fixtures(const char* emcc, target_t* runner) {
    char* dir = out_path("out");
    command_t* mkdir_out = cbuild_command_function(ctx, "create fixture output directory", make_dir_command, dir);
    for (int i = 0; i < COUNT(emcc_cases); ++i) {
        const emcc_case_t* c = &emcc_cases[i];
        char* source = fmt("test/fixtures/%s.c", c->name);
        char* wasm = fmt("%s/%s.wasm", dir, c->name);
        const char* argv[16];
        int argc = 0;
        argv[argc++] = emcc;
        argv[argc++] = source;
        argv[argc++] = "-sSTANDALONE_WASM=1";
        argv[argc++] = "-Wl,--no-entry";
        argv[argc++] = "-Wl,--export=wl_case";
        for (int f = 0; f < COUNT(c->emcc_flags) && c->emcc_flags[f]; ++f) argv[argc++] = c->emcc_flags[f];
        argv[argc++] = "-o";
        argv[argc++] = wasm;

        target_t* fixture = cbuild_file_dep_target(ctx, fmt("emcc-fixture.%s", c->name), wasm);
        cbuild_add_source(ctx, fixture, source);
        cbuild_target_add_command(ctx, fixture, mkdir_out);
        cbuild_target_add_command(ctx, fixture, cbuild_command_argv(ctx, fmt("emcc %s.c", c->name), (char**)argv, argc));

        const char* args[5] = { wasm };
        int nargs = 1;
        for (int a = 0; a < COUNT(c->runner_args) && c->runner_args[a]; ++a) args[nargs++] = c->runner_args[a];
        test_t* test = add_test(fmt("emcc.%s", c->name), runner, args, nargs, "emcc", NULL);
        cbuild_test_add_dependency(ctx, test, fixture);
    }
}

static void session_math_example(const char* emcc, target_t* wasm2api, target_t* header) {
    char* dir = out_path("examples");
    char* wasm = fmt("%s/session_math.wasm", dir);
    char* stamp = fmt("%s/session_math.stamp", dir);

    char* emcc_argv[] = {
        (char*)emcc, "examples/session_math_wasm.c", "-sSTANDALONE_WASM=1", "-Wl,--no-entry",
        "-Wl,--export=add", "-Wl,--export=sub", "-Wl,--export=init_state", "-Wl,--export=ready",
        "-Wl,--export=add_scaled", "-Wl,--export=set_bias", "-Wl,--export=get_bias",
        "-Wl,--export=get_total", "-Wl,--export=reset_total", "-O1", "-o", wasm,
    };
    target_t* module = cbuild_file_dep_target(ctx, "session_math_wasm", wasm);
    cbuild_add_source(ctx, module, "examples/session_math_wasm.c");
    cbuild_add_source(ctx, module, "build.c");
    cbuild_target_add_command(ctx, module, cbuild_command_function(ctx, "create example output directory",
                                                                   make_dir_command, dir));
    cbuild_target_add_command(ctx, module, cbuild_command_argv(ctx, "emcc session_math_wasm.c", emcc_argv,
                                                               COUNT(emcc_argv)));

    /* The wrappers are committed, so a stamp stands in for them and --clean leaves them alone. */
    char* generate_argv[] = {
        exe_path("wasm2api"), "--singleton", "--embed", "--init-func", "init_state", wasm, "examples/session_math",
    };
    target_t* generate = cbuild_file_dep_target(ctx, "session_math_generate", stamp);
    cbuild_add_link_target(ctx, generate, wasm2api);
    cbuild_add_link_target(ctx, generate, module);
    cbuild_target_add_command(ctx, generate, cbuild_command_argv(ctx, "wasm2api session_math", generate_argv,
                                                                 COUNT(generate_argv)));
    cbuild_target_add_command(ctx, generate, cbuild_command_function(ctx, "stamp session_math wrappers",
                                                                     touch_stamp, stamp));

    target_t* demo = native_executable("session_math_demo", "examples/session_math_demo.c", header);
    cbuild_add_source(ctx, demo, "examples/session_math.c");
    cbuild_add_link_target(ctx, demo, generate);
    add_test("session_math_demo", demo, NULL, 0, "unit", NULL);
}

static void read_options(int argc, char** argv) {
    opt.release = cbuild_has_flag(argc, argv, "release", 0);
    opt.platform = !cbuild_has_flag(argc, argv, "no-platform", 0);
    opt.werror = !cbuild_has_flag(argc, argv, "no-werror", 0);
    opt.emcc = !cbuild_has_flag(argc, argv, "no-emcc", 0);
    opt.fuzzer = cbuild_has_flag(argc, argv, "fuzzer", 0);
    opt.spec_proposals = cbuild_has_flag(argc, argv, "spec-proposals", 0);
    opt.announce = cbuild_has_flag(argc, argv, "test", 0) || cbuild_has_flag(argc, argv, "list-tests", 0);

    opt.cc = cbuild_get_flag_value(argc, argv, "cc", 0);
    if (!opt.cc || !*opt.cc) opt.cc = getenv("CC");
#ifdef _WIN32
    if (!opt.cc || !*opt.cc) opt.cc = "cl";
#else
    if (!opt.cc || !*opt.cc) opt.cc = "cc";
#endif
    opt.msvc = is_msvc_compiler(opt.cc);

    opt.build_dir = cbuild_get_flag_value(argc, argv, "build-dir", 0);
    if (!opt.build_dir || !*opt.build_dir) opt.build_dir = opt.release ? "build/release" : "build/debug";
    opt.cflags = join_flags(getenv("CFLAGS"), cbuild_get_flag_value(argc, argv, "cflags", 0));
    opt.ldflags = join_flags(getenv("LDFLAGS"), cbuild_get_flag_value(argc, argv, "ldflags", 0));

    /* Registered so cbuild_run accepts them; the values were read above. */
    static int bound_bool;
    static const char* bound_str;
    cbuild_register_flag_bool(ctx, "release", 0, CBUILD_FLAG_PRE,
                              "Optimized build (-O3 -DNDEBUG, or /O2) in build/release", &bound_bool);
    cbuild_register_flag_str(ctx, "build-dir", 0, CBUILD_FLAG_PRE,
                             "Output directory (default build/debug or build/release)", &bound_str);
    cbuild_register_flag_str(ctx, "cc", 0, CBUILD_FLAG_PRE, "C compiler (default $CC, else cc or cl)", &bound_str);
    cbuild_register_flag_str(ctx, "cflags", 0, CBUILD_FLAG_PRE,
                             "Extra compile flags, after $CFLAGS and all built-in flags", &bound_str);
    cbuild_register_flag_str(ctx, "ldflags", 0, CBUILD_FLAG_PRE, "Extra link flags, after $LDFLAGS", &bound_str);
    cbuild_register_flag_bool(ctx, "no-platform", 0, CBUILD_FLAG_PRE,
                              "Build without platform-backed runtime and test helpers", &bound_bool);
    cbuild_register_flag_bool(ctx, "no-werror", 0, CBUILD_FLAG_PRE, "Do not treat warnings as errors", &bound_bool);
    cbuild_register_flag_bool(ctx, "no-emcc", 0, CBUILD_FLAG_PRE,
                              "Skip the Emscripten fixture tests even when emcc is available", &bound_bool);
    cbuild_register_flag_bool(ctx, "fuzzer", 0, CBUILD_FLAG_PRE,
                              "Also build the libFuzzer harness wasm_load_fuzzer (Clang)", &bound_bool);
    cbuild_register_flag_bool(ctx, "spec-proposals", 0, CBUILD_FLAG_PRE,
                              "Run spec proposal suites the runtime does not implement instead of skipping them",
                              &bound_bool);
}

int main(int argc, char** argv) {
    ctx = cbuild_context_new();
    if (!ctx) return 1;
    CBUILD_SELF_REBUILD(ctx, argc, argv, "build.c", "cbuild.h");

    read_options(argc, argv);
    cbuild_set_compiler(ctx, opt.cc);
    cbuild_set_output_dir(ctx, opt.build_dir);
    if (opt.fuzzer && (opt.msvc || !strstr(opt.cc, "clang"))) {
        fprintf(stderr, "--fuzzer requires Clang with libFuzzer support (try --cc=clang).\n");
        return 1;
    }
    opt.x86_32 = compiler_targets_x86_32();

    char cwd[4096];
    if (cbuild_get_cwd(cwd, sizeof(cwd)) != 0) return 1;
    for (char* c = cwd; *c; ++c) {
        if (*c == '\\') *c = '/';
    }

    target_t* amalgamate = cbuild_executable(ctx, "amalgamate");
    cbuild_add_source(ctx, amalgamate, "tools/amalgamate.c");
    base_options(amalgamate);
    warning_options(amalgamate);

    char* header_stamp = out_path("wasm-header.stamp");
    target_t* header = cbuild_file_dep_target(ctx, "wasm-header", header_stamp);
    cbuild_add_source(ctx, header, "src/**/*.h");
    cbuild_add_source(ctx, header, "src/**/*.c");
    cbuild_add_source(ctx, header, "src/**/*.inc");
    cbuild_add_source(ctx, header, "wasm.h"); /* regenerate a header edited or checked out by hand */
    cbuild_add_link_target(ctx, header, amalgamate);
    char* amalgamate_argv[] = { exe_path("amalgamate"), "--stamp", header_stamp };
    cbuild_target_add_command(ctx, header, cbuild_command_argv(ctx, "generate wasm.h", amalgamate_argv, 3));

    target_t* runtime = runtime_library("wasm_runtime");
    target_t* test_runtime = runtime_library("wasm_test_runtime");
    /* GCC and Clang resolve -include from the working directory; MSVC finds /FI through -I. */
    cbuild_add_cflags(ctx, test_runtime, opt.msvc ? "/FItest/wasm_test_alloc.h" : "-include test/wasm_test_alloc.h");
    private_header_check();

    target_t* wasm2api = native_executable("wasm2api", "wasm2api.c", header);
    native_executable("wasm", "wasm.c", header);

    target_t* wl_test = native_executable("wl_test", "test/wl_test.c", header);
    target_t* wasm_test = native_executable("wasm_test", "test/wasm_test.c", header);
    target_t* wasm_modular_test = native_executable("wasm_modular_test", "test/wasm_test.c", header);
    cbuild_add_define(ctx, wasm_modular_test, "WASM_TEST_MODULAR=1");
    cbuild_add_link_target(ctx, wasm_modular_test, test_runtime);
    target_t* runner = native_executable("wasm_runner", "test/fixture_runner.c", header);
    target_t* spectest_runner = native_executable("spectest_runner", "test/spectest_runner.c", header);
    target_t* spectest_modular_runner =
        native_executable("spectest_modular_runner", "test/spectest_runner.c", header);
    cbuild_add_define(ctx, spectest_modular_runner, "WASM_TEST_MODULAR=1");
    cbuild_add_include_dir(ctx, spectest_modular_runner, "src");
    cbuild_add_link_target(ctx, spectest_modular_runner, runtime);

    /* No project_options: strict C99 without platform feature definitions. */
    target_t* core_test = cbuild_executable(ctx, "wasm_core_test");
    cbuild_add_source(ctx, core_test, "test/wasm_core_test.c");
    base_options(core_test);
    warning_options(core_test);
    cbuild_add_include_dir(ctx, core_test, ".");
    cbuild_add_link_target(ctx, core_test, header);
    target_t* modular_core_test = cbuild_executable(ctx, "wasm_modular_core_test");
    cbuild_add_source(ctx, modular_core_test, "test/wasm_core_test.c");
    base_options(modular_core_test);
    cbuild_add_include_dir(ctx, modular_core_test, "src");
    cbuild_add_define(ctx, modular_core_test, "WASM_TEST_MODULAR=1");
    cbuild_add_link_target(ctx, modular_core_test, header);
    cbuild_add_link_target(ctx, modular_core_test, runtime);

    target_t* amalgamate_test = cbuild_executable(ctx, "wasm_amalgamate_test");
    cbuild_add_source(ctx, amalgamate_test, "test/amalgamate_test.c");
    project_options(amalgamate_test);

    add_test("wl_test", wl_test, NULL, 0, "unit", NULL);
    add_test("wasm_test", wasm_test, NULL, 0, "unit", NULL);
    add_test("wasm_modular_test", wasm_modular_test, NULL, 0, "unit", "modular");
    add_test("wasm_core_test", core_test, NULL, 0, "unit", NULL);
    add_test("wasm_modular_core_test", modular_core_test, NULL, 0, "unit", "modular");

    const char* amalgamate_test_args[] = { exe_path("amalgamate"), out_path("amalgamate-fixture") };
    test_t* amalgamate_check = add_test("wasm_amalgamate_test", amalgamate_test, amalgamate_test_args, 2, "unit", NULL);
    cbuild_test_add_dependency(ctx, amalgamate_check, amalgamate);
#ifndef _WIN32
    static const char* const git_names[] = { "git" };
    if (find_program(git_names, 1, NULL, 0)) {
        const char* hook_args[] = { "sh", "test/amalgamate_hook_test.sh", cwd };
        add_test("wasm_amalgamate_hook_test", NULL, hook_args, 3, "unit", NULL);
    }
#endif

    /* A copy under a path with spaces exercises the runners' shell quoting. */
    target_t* runners[] = { spectest_runner, spectest_modular_runner };
    const char* runner_names[] = { "spectest_runner", "spectest_modular_runner" };
    const char* capture_tests[] = { "spectest_capture", "spectest_modular_capture" };
    for (int i = 0; i < 2; ++i) {
        copy_job_t* job = (copy_job_t*)calloc(1, sizeof(*job));
        job->from = exe_path(runner_names[i]);
        job->to = fmt("%s/%s/command capture/%s" EXE_SUFFIX, cwd, opt.build_dir, runner_names[i]);
        target_t* copy = cbuild_file_dep_target(ctx, fmt("%s-capture-copy", runner_names[i]), job->to);
        cbuild_add_link_target(ctx, copy, runners[i]);
        cbuild_target_add_command(ctx, copy, cbuild_command_function(ctx, fmt("copy %s for command capture",
                                                                              runner_names[i]),
                                                                     copy_executable, job));
        const char* args[] = { "--test-command-capture", job->to };
        test_t* test = add_test(capture_tests[i], runners[i], args, 2, "unit", i ? "modular" : NULL);
        cbuild_test_add_dependency(ctx, test, copy);
    }

    spectests(spectest_runner, spectest_modular_runner);

    char* emcc = find_emcc();
    if (emcc) {
        if (opt.emcc && opt.platform) emcc_fixtures(emcc, runner);
        session_math_example(emcc, wasm2api, header);
    } else if (opt.announce) {
        printf("emcc not found; skipping Emscripten fixtures and the session_math example.\n");
    }

    if (opt.fuzzer) {
        target_t* fuzzer = cbuild_executable(ctx, "wasm_load_fuzzer");
        cbuild_add_source(ctx, fuzzer, "test/fuzz_wasm_load.c");
        base_options(fuzzer);
        cbuild_add_include_dir(ctx, fuzzer, ".");
        cbuild_add_cflags(ctx, fuzzer, "-fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer");
        cbuild_add_ldflags(ctx, fuzzer, "-fsanitize=fuzzer,address,undefined");
        cbuild_add_link_target(ctx, fuzzer, header);
    }

    /* Last, so caller flags override the build type. */
    for (int i = 0; i < native_target_count; ++i) {
        if (opt.cflags) cbuild_add_cflags(ctx, native_targets[i], opt.cflags);
        if (opt.ldflags) cbuild_add_ldflags(ctx, native_targets[i], opt.ldflags);
    }

    int result = cbuild_run(ctx, argc, argv);
    cbuild_context_free(ctx);
    return result;
}
