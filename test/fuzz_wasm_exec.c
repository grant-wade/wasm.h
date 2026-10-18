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

static wasm_error_t wasm_fuzz_stub_func(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results, void* userdata) {
    const wasm_functype_t* type = (const wasm_functype_t*)userdata;
    uint32_t i;

    (void)mod;
    (void)args;
    (void)num_args;
    for (i = 0; i < num_results && i < type->num_results; i++) {
        if (!wasm_fuzz_default_value(type->results[i], type->result_reftypes ? &type->result_reftypes[i] : NULL,
                                     &results[i]))
            return WASM_ERR_TRAP;
    }
    return WASM_OK;
}

static void wasm_fuzz_stubs_free(wasm_fuzz_stubs_t* stubs) {
    uint32_t i;

    for (i = 0; i < stubs->count; i++) {
        if (stubs->memories[i]) {
            WASM_FREE(stubs->memories[i]->data);
            WASM_FREE(stubs->memories[i]);
        }
        if (stubs->tables[i]) {
            WASM_FREE(stubs->tables[i]->elems);
            WASM_FREE(stubs->tables[i]);
        }
        WASM_FREE(stubs->globals[i]);
    }
    WASM_FREE(stubs->memories);
    WASM_FREE(stubs->tables);
    WASM_FREE(stubs->globals);
    memset(stubs, 0, sizeof(*stubs));
}

static void wasm_fuzz_stub_memory(wasm_runtime_t* rt, const wasm_module_t* compiled, const wasm_import_info_t* info,
                                  wasm_fuzz_stubs_t* stubs, uint32_t slot) {
    const wasm_memory_t* declared = &compiled->memories[info->index];
    uint32_t page_size = declared->page_size ? declared->page_size : WASM_PAGE_SIZE;
    wasm_memory_t* memory;
    wasm_memory_import_t imp;

    if (declared->pages > WASM_FUZZ_MAX_MEMORY_BYTES / page_size) return;
    memory = (wasm_memory_t*)WASM_CALLOC(1, sizeof(*memory));
    if (!memory) return;
    memory->pages = declared->pages;
    memory->max_pages = declared->max_pages;
    memory->is_64 = declared->is_64;
    memory->page_size = declared->page_size;
    if (memory->pages > 0) {
        memory->data = (uint8_t*)WASM_CALLOC(1, (size_t)(memory->pages * page_size));
        if (!memory->data) {
            WASM_FREE(memory);
            return;
        }
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
    uint32_t i;

    if (declared->size > WASM_FUZZ_MAX_TABLE_ELEMENTS ||
        !wasm_fuzz_default_value(declared->reftype, &declared->reftype_info, &fill))
        return;
    table = (wasm_table_t*)WASM_CALLOC(1, sizeof(*table));
    if (!table) return;
    table->reftype = declared->reftype;
    table->reftype_info = declared->reftype_info;
    table->default_value = fill;
    table->size = declared->size;
    table->max_size = declared->max_size;
    table->is_64 = declared->is_64;
    if (table->size > 0) {
        table->elems = (wasm_value_t*)WASM_MALLOC((size_t)table->size * sizeof(wasm_value_t));
        if (!table->elems) {
            WASM_FREE(table);
            return;
        }
        for (i = 0; i < table->size; i++) table->elems[i] = fill;
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

    stubs->memories = (wasm_memory_t**)WASM_CALLOC(count ? count : 1u, sizeof(*stubs->memories));
    stubs->tables = (wasm_table_t**)WASM_CALLOC(count ? count : 1u, sizeof(*stubs->tables));
    stubs->globals = (wasm_value_t**)WASM_CALLOC(count ? count : 1u, sizeof(*stubs->globals));
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
                wasm_value_t* value = (wasm_value_t*)WASM_MALLOC(sizeof(*value));

                if (!value) break;
                if (!wasm_fuzz_default_value(info->type, &info->ref_type, value)) {
                    WASM_FREE(value);
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

static void wasm_fuzz_call_exports(wasm_runtime_t* rt, wasm_module_t* instance) {
    uint32_t calls = 0;
    uint32_t i;

    for (i = 0; i < wasm_export_count(instance) && calls < WASM_FUZZ_MAX_CALLS; i++) {
        uint32_t func_idx = wasm_export_index(instance, i);
        const wasm_functype_t* type;
        wasm_value_t* args;
        wasm_value_t* results;
        uint32_t p;
        int callable = 1;

        if (wasm_export_kind(instance, i) != WASM_EXPORT_FUNC) continue;
        type = wasm_func_functype(instance, func_idx);
        if (!type) continue;
        args = (wasm_value_t*)WASM_CALLOC(type->num_params ? type->num_params : 1u, sizeof(*args));
        results = (wasm_value_t*)WASM_CALLOC(type->num_results ? type->num_results : 1u, sizeof(*results));
        for (p = 0; args && p < type->num_params; p++) {
            if (!wasm_fuzz_default_value(type->params[p], type->param_reftypes ? &type->param_reftypes[p] : NULL,
                                         &args[p]))
                callable = 0;
        }
        if (args && results && callable) {
            wasm_runtime_set_fuel(rt, WASM_FUZZ_FUEL);
            (void)wasm_call_index(instance, func_idx, args, type->num_params, results, type->num_results);
            calls++;
        }
        WASM_FREE(args);
        WASM_FREE(results);
    }
}

static void wasm_fuzz_exec_one(const uint8_t* data, size_t size) {
    wasm_config_t config;
    wasm_runtime_t* runtime;
    wasm_module_t* compiled;
    wasm_module_t* instance;
    wasm_fuzz_stubs_t stubs;

    wasm_config_default(&config);
    config.max_stack_values = 4096u;
    config.max_call_depth = 128u;
    config.max_labels = 1024u;
    config.initial_gc_heap_size = 256u * 1024u;
    config.frame_arena_size = 512u * 1024u;
    config.max_memory_bytes = WASM_FUZZ_MAX_MEMORY_BYTES;
    config.max_table_elements = WASM_FUZZ_MAX_TABLE_ELEMENTS;
    config.max_gc_heap_bytes = WASM_FUZZ_MAX_GC_HEAP_BYTES;

    runtime = wasm_runtime_new(&config);
    if (!runtime || wasm_runtime_last_error(runtime) != WASM_OK) {
        wasm_runtime_free(runtime);
        return;
    }
    /* Threads stay off: an atomic wait on a stubbed shared memory would block forever. */
    wasm_runtime_enable_all_features(runtime);
    wasm_runtime_disable_feature(runtime, WASM_FEATURE_THREADS);

    memset(&stubs, 0, sizeof(stubs));
    compiled = wasm_compile(runtime, data, size);
    if (compiled) {
        wasm_fuzz_stub_imports(runtime, compiled, &stubs);
        wasm_runtime_set_fuel(runtime, WASM_FUZZ_FUEL);
        instance = wasm_instantiate(compiled);
        if (instance) {
            wasm_fuzz_call_exports(runtime, instance);
            wasm_gc_collect(runtime);
            wasm_module_free(instance);
        }
        wasm_module_free(compiled);
    }
    wasm_runtime_free(runtime);
    wasm_fuzz_stubs_free(&stubs);
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
