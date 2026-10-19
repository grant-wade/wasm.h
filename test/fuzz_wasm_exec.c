#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WASM_IMPL
#include "wasm.h"

#define WASM_FUZZ_FUEL UINT64_C(10000)
#define WASM_FUZZ_MAX_CALLS 32u
#define WASM_FUZZ_MAX_MEMORY_BYTES (1u * 1024u * 1024u)
#define WASM_FUZZ_MAX_TABLE_ELEMENTS 4096u
#define WASM_FUZZ_MAX_GC_HEAP_BYTES (16u * 1024u * 1024u)

/* Host objects created to satisfy one input's imports. */
typedef struct wasm_fuzz_stubs_t {
    wasm_memory_t** memories;
    wasm_table_t** tables;
    wasm_value_t** globals;
    uint32_t count;
} wasm_fuzz_stubs_t;

static int wasm_fuzz_reftype_is_nullable(wasm_valtype_t type, const wasm_reftype_t* ref_type) {
    return !wasm__is_ref_type(type) || !ref_type || !wasm__has_reftype_info(ref_type) || ref_type->nullable;
}

/* Zero or null for the type; fails for non-nullable references, which have no default. */
static int wasm_fuzz_default_value(wasm_valtype_t type, const wasm_reftype_t* ref_type, wasm_value_t* out) {
    if (!wasm_fuzz_reftype_is_nullable(type, ref_type)) return 0;
    *out = wasm__default_value(type);
    return 1;
}

/* Fills results with defaults for type's results; fails if one has no default. */
static int wasm_fuzz_default_results(const wasm_functype_t* type, wasm_value_t* results, uint32_t num_results) {
    uint32_t i;

    for (i = 0; i < num_results && i < type->num_results; i++) {
        if (!wasm_fuzz_default_value(type->results[i], type->result_reftypes ? &type->result_reftypes[i] : NULL,
                                     &results[i]))
            return 0;
    }
    return 1;
}

/* Set while the stepped runtime calls exports, whose stubs then yield instead of returning. */
static wasm_runtime_t* wasm_fuzz_yielding_runtime;

static wasm_error_t wasm_fuzz_stub_func(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results, void* userdata) {
    const wasm_functype_t* type = (const wasm_functype_t*)userdata;
    wasm_runtime_t* rt = wasm_module_runtime(mod);

    (void)args;
    (void)num_args;
    if (!wasm_fuzz_default_results(type, results, num_results)) return WASM_ERR_TRAP;
    if (rt == wasm_fuzz_yielding_runtime && wasm_runtime_call_depth(rt) > 0) return WASM_YIELD;
    return WASM_OK;
}

static void wasm_fuzz_stubs_free(wasm_fuzz_stubs_t* stubs) {
    uint32_t i;

    for (i = 0; i < stubs->count; i++) {
        if (stubs->memories[i]) {
            wasm__free_memory(stubs->memories[i]);
            free(stubs->memories[i]);
        }
        if (stubs->tables[i]) {
            wasm__free_table(stubs->tables[i]);
            free(stubs->tables[i]);
        }
        free(stubs->globals[i]);
    }
    free(stubs->memories);
    free(stubs->tables);
    free(stubs->globals);
    memset(stubs, 0, sizeof(*stubs));
}

static void wasm_fuzz_stub_memory(wasm_runtime_t* rt, const wasm_module_t* compiled, const wasm_import_info_t* info,
                                  wasm_fuzz_stubs_t* stubs, uint32_t slot) {
    const wasm_memory_t* declared = &compiled->memories[info->index];
    uint32_t page_size = declared->page_size ? declared->page_size : WASM_PAGE_SIZE;
    wasm_memory_t* memory;
    wasm_memory_import_t imp;

    if (declared->pages > WASM_FUZZ_MAX_MEMORY_BYTES / page_size) return;
    memory = (wasm_memory_t*)calloc(1, sizeof(*memory));
    if (!memory) return;
    memory->allocator = *wasm_libc_allocator();
    memory->pages = declared->pages;
    memory->max_pages = declared->max_pages;
    memory->is_64 = declared->is_64;
    memory->page_size = declared->page_size;
    if (wasm__init_memory_storage(memory, 0) != WASM_OK) {
        free(memory);
        return;
    }
    stubs->memories[slot] = memory;
    imp.module = info->module;
    imp.name = info->name;
    imp.memory = memory;
    (void)wasm_register_memory_import(rt, &imp);
}

static void wasm_fuzz_stub_table(wasm_runtime_t* rt, const wasm_module_t* compiled, const wasm_import_info_t* info,
                                 wasm_fuzz_stubs_t* stubs, uint32_t slot) {
    const wasm_table_t* declared = &compiled->tables[info->index];
    wasm_table_t* table;
    wasm_table_import_t imp;
    wasm_value_t fill = wasm_i32(0);

    if (declared->size > WASM_FUZZ_MAX_TABLE_ELEMENTS ||
        !wasm_fuzz_default_value(declared->reftype, &declared->reftype_info, &fill))
        return;
    table = (wasm_table_t*)calloc(1, sizeof(*table));
    if (!table) return;
    table->allocator = *wasm_libc_allocator();
    table->reftype = declared->reftype;
    table->reftype_info = declared->reftype_info;
    table->default_value = fill;
    table->size = declared->size;
    table->max_size = declared->max_size;
    table->is_64 = declared->is_64;
    if (wasm__init_table_storage(table) != WASM_OK) {
        free(table);
        return;
    }
    stubs->tables[slot] = table;
    imp.module = info->module;
    imp.name = info->name;
    imp.table = table;
    (void)wasm_register_table_import(rt, &imp);
}

static void wasm_fuzz_stub_imports(wasm_runtime_t* rt, wasm_module_t* compiled, wasm_fuzz_stubs_t* stubs) {
    uint32_t count = wasm_import_count(compiled);
    uint32_t i;

    stubs->memories = (wasm_memory_t**)calloc(count ? count : 1u, sizeof(*stubs->memories));
    stubs->tables = (wasm_table_t**)calloc(count ? count : 1u, sizeof(*stubs->tables));
    stubs->globals = (wasm_value_t**)calloc(count ? count : 1u, sizeof(*stubs->globals));
    if (!stubs->memories || !stubs->tables || !stubs->globals) return;
    stubs->count = count;

    for (i = 0; i < count; i++) {
        const wasm_import_info_t* info = wasm_import_info(compiled, i);

        if (!info) continue;
        switch (info->kind) {
            case WASM_IMPORT_FUNC: {
                const wasm_functype_t* type = wasm_func_functype(compiled, info->index);
                wasm_import_t imp;

                if (!type) break;
                memset(&imp, 0, sizeof(imp));
                imp.module = info->module;
                imp.name = info->name;
                imp.type = *type;
                imp.type_module = compiled;
                imp.type_index = info->type_index;
                imp.has_type_context = 1;
                imp.func = wasm_fuzz_stub_func;
                imp.userdata = (void*)type;
                (void)wasm_register_import(rt, &imp);
                break;
            }
            case WASM_IMPORT_MEM:
                wasm_fuzz_stub_memory(rt, compiled, info, stubs, i);
                break;
            case WASM_IMPORT_TABLE:
                wasm_fuzz_stub_table(rt, compiled, info, stubs, i);
                break;
            case WASM_IMPORT_GLOBAL: {
                wasm_global_import_t imp;
                wasm_value_t* value = (wasm_value_t*)malloc(sizeof(*value));

                if (!value) break;
                if (!wasm_fuzz_default_value(info->type, &info->ref_type, value)) {
                    free(value);
                    break;
                }
                stubs->globals[i] = value;
                memset(&imp, 0, sizeof(imp));
                imp.module = info->module;
                imp.name = info->name;
                imp.type = info->type;
                imp.ref_type = info->ref_type;
                imp.is_mutable = info->is_mutable;
                imp.value = value;
                (void)wasm_register_global_import(rt, &imp);
                break;
            }
            case WASM_IMPORT_TAG: {
                const wasm_functype_t* type = wasm_tag_functype(compiled, info->index);
                wasm_tag_import_t imp;

                if (!type) break;
                memset(&imp, 0, sizeof(imp));
                imp.module = info->module;
                imp.name = info->name;
                imp.type = *type;
                imp.type_module = compiled;
                imp.type_index = info->type_index;
                imp.has_type_context = 1;
                imp.identity = wasm__alloc_tag_identity(rt);
                (void)wasm_register_tag_import(rt, &imp);
                break;
            }
            default:
                break;
        }
    }
}

/* One runtime with an instance of the input and the host objects backing its imports. */
typedef struct wasm_fuzz_instance_t {
    wasm_runtime_t* rt;
    wasm_module_t* compiled;
    wasm_module_t* instance;
    wasm_fuzz_stubs_t stubs;
} wasm_fuzz_instance_t;

static void wasm_fuzz_instance_open(wasm_fuzz_instance_t* inst, const uint8_t* data, size_t size) {
    wasm_config_t config;

    memset(inst, 0, sizeof(*inst));
    wasm_config_default(&config);
    config.max_stack_values = 4096u;
    config.max_call_depth = 128u;
    config.max_labels = 1024u;
    config.initial_gc_heap_size = 256u * 1024u;
    config.frame_arena_size = 512u * 1024u;
    config.max_memory_bytes = WASM_FUZZ_MAX_MEMORY_BYTES;
    config.max_table_elements = WASM_FUZZ_MAX_TABLE_ELEMENTS;
    config.max_gc_heap_bytes = WASM_FUZZ_MAX_GC_HEAP_BYTES;

    inst->rt = wasm_runtime_new(wasm_libc_allocator(), &config);
    if (!inst->rt || wasm_runtime_last_error(inst->rt) != WASM_OK) return;
    wasm_runtime_enable_all_features(inst->rt);
    wasm_runtime_disable_feature(inst->rt, WASM_FEATURE_THREADS);

    inst->compiled = wasm_compile(inst->rt, data, size);
    if (!inst->compiled) return;
    wasm_fuzz_stub_imports(inst->rt, inst->compiled, &inst->stubs);
    wasm_runtime_set_fuel(inst->rt, WASM_FUZZ_FUEL);
    inst->instance = wasm_instantiate(inst->rt, inst->compiled, NULL);
}

static void wasm_fuzz_instance_close(wasm_fuzz_instance_t* inst) {
    if (inst->instance) {
        wasm_gc_collect(inst->rt);
        wasm_module_free(inst->instance);
    }
    wasm_module_free(inst->compiled);
    wasm_runtime_free(inst->rt);
    wasm_fuzz_stubs_free(&inst->stubs);
}

static int wasm_fuzz_values_match(const wasm_value_t* a, const wasm_value_t* b) {
    if (a->type != b->type) return 0;
    switch (a->type) {
        case WASM_TYPE_I32:
            return a->of.i32 == b->of.i32;
        case WASM_TYPE_I64:
            return a->of.i64 == b->of.i64;
        case WASM_TYPE_F32:
            return memcmp(&a->of.f32, &b->of.f32, sizeof(a->of.f32)) == 0;
        case WASM_TYPE_F64:
            return memcmp(&a->of.f64, &b->of.f64, sizeof(a->of.f64)) == 0;
        case WASM_TYPE_V128:
            return memcmp(&a->of.v128, &b->of.v128, sizeof(a->of.v128)) == 0;
        default:
            return 1;
    }
}

/* Calls func_idx with step units of fuel at a time, resuming every stop, until the call finishes. */
static wasm_error_t wasm_fuzz_call_in_steps(wasm_fuzz_instance_t* inst, uint32_t func_idx, const wasm_value_t* args,
                                            uint32_t num_args, wasm_value_t* results, uint32_t num_results,
                                            uint64_t step) {
    wasm_runtime_t* rt = inst->rt;
    const uint32_t* last_pc = NULL;
    uint32_t last_depth = 0;
    uint32_t steps = 0;
    uint64_t fuel = step;
    wasm_error_t err;

    wasm_runtime_set_fuel(rt, fuel);
    wasm_fuzz_yielding_runtime = rt;
    err = wasm_call_index(inst->instance, func_idx, args, num_args, results, num_results);
    while (wasm_runtime_suspended(rt) != WASM_OK && steps++ < 1000000u) {
        wasm_value_t host_results[16];
        uint32_t num_host_results = 0;
        const wasm__call_frame_t* top = &rt->call_frames[rt->call_frame_sp - 1u];

        if (err == WASM_YIELD) {
            num_host_results = rt->yield_type->num_results;
            if (num_host_results > 16u ||
                !wasm_fuzz_default_results(rt->yield_type, host_results, num_host_results))
                abort();
        } else if (top->pc == last_pc && rt->call_frame_sp == last_depth) {
            fuel *= 2u;
        } else {
            fuel = step;
        }
        last_pc = top->pc;
        last_depth = rt->call_frame_sp;
        wasm_runtime_set_fuel(rt, fuel);
        err = wasm_runtime_resume(rt, host_results, num_host_results, results, num_results);
    }
    wasm_fuzz_yielding_runtime = NULL;
    if (wasm_runtime_suspended(rt) != WASM_OK) abort();
    return err;
}

/* Calls each export in ref with a fixed budget, and in stepped in small fuel steps that resume after
 * every fuel stop and host yield, and aborts if the two disagree. */
static void wasm_fuzz_call_exports(wasm_fuzz_instance_t* ref, wasm_fuzz_instance_t* stepped, uint64_t step) {
    uint32_t calls = 0;
    uint32_t i;

    for (i = 0; i < wasm_export_count(ref->instance) && calls < WASM_FUZZ_MAX_CALLS; i++) {
        uint32_t func_idx = wasm_export_index(ref->instance, i);
        const wasm_functype_t* type;
        wasm_value_t* args;
        wasm_value_t* results;
        wasm_value_t* stepped_results;
        uint32_t p;
        int callable = 1;
        int stop = 0;

        if (wasm_export_kind(ref->instance, i) != WASM_EXPORT_FUNC) continue;
        type = wasm_func_functype(ref->instance, func_idx);
        if (!type) continue;
        args = (wasm_value_t*)calloc(type->num_params ? type->num_params : 1u, sizeof(*args));
        results = (wasm_value_t*)calloc(type->num_results ? type->num_results : 1u, sizeof(*results));
        stepped_results = (wasm_value_t*)calloc(type->num_results ? type->num_results : 1u, sizeof(*results));
        for (p = 0; args && p < type->num_params; p++) {
            if (!wasm_fuzz_default_value(type->params[p], type->param_reftypes ? &type->param_reftypes[p] : NULL,
                                         &args[p]))
                callable = 0;
        }
        if (args && results && stepped_results && callable) {
            wasm_error_t err;

            wasm_runtime_set_fuel(ref->rt, WASM_FUZZ_FUEL);
            err = wasm_call_index(ref->instance, func_idx, args, type->num_params, results, type->num_results);
            calls++;
            if (wasm_runtime_suspended(ref->rt) != WASM_OK) {
                wasm_runtime_cancel(ref->rt);
                stop = 1;
            } else {
                wasm_error_t stepped_err = wasm_fuzz_call_in_steps(stepped, func_idx, args, type->num_params,
                                                                   stepped_results, type->num_results, step);

                if (stepped_err != err) abort();
                for (p = 0; err == WASM_OK && p < type->num_results; p++) {
                    if (!wasm_fuzz_values_match(&results[p], &stepped_results[p])) abort();
                }
                if (wasm_memory_count(ref->instance) > 0) {
                    uint64_t bytes = wasm_memory_size(ref->instance, 0);

                    if (bytes != wasm_memory_size(stepped->instance, 0) ||
                        (bytes > 0 && memcmp(wasm_memory_data(ref->instance, 0),
                                             wasm_memory_data(stepped->instance, 0), (size_t)bytes) != 0))
                        abort();
                }
            }
        }
        free(args);
        free(results);
        free(stepped_results);
        if (stop) break;
    }
}

static void wasm_fuzz_exec_one(const uint8_t* data, size_t size) {
    wasm_fuzz_instance_t ref;
    wasm_fuzz_instance_t stepped;

    wasm_fuzz_instance_open(&ref, data, size);
    wasm_fuzz_instance_open(&stepped, data, size);
    if (!ref.instance != !stepped.instance) abort();
    if (ref.instance) wasm_fuzz_call_exports(&ref, &stepped, 1u + size % 5u);
    wasm_fuzz_instance_close(&stepped);
    wasm_fuzz_instance_close(&ref);
}

#ifdef WASM_FUZZ_REPLAY
/* Replays files given on the command line, so a corpus runs without libFuzzer. */
int main(int argc, char** argv) {
    int i;

    for (i = 1; i < argc; i++) {
        FILE* file = fopen(argv[i], "rb");
        uint8_t* data = NULL;
        long size;

        if (!file) {
            fprintf(stderr, "cannot open %s\n", argv[i]);
            return 1;
        }
        if (fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) >= 0 && fseek(file, 0, SEEK_SET) == 0) {
            data = (uint8_t*)malloc(size ? (size_t)size : 1u);
            if (data && fread(data, 1, (size_t)size, file) == (size_t)size) wasm_fuzz_exec_one(data, (size_t)size);
        }
        free(data);
        fclose(file);
    }
    printf("replayed %d input%s\n", argc - 1, argc == 2 ? "" : "s");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    wasm_fuzz_exec_one(data, size);
    return 0;
}
#endif
