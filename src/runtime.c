#include "runtime.h"
#include "values.h"
#include "module.h"
#include "types/canonical.h"
#include "memory_internal.h"
#include "reader.h"
#include "exec/frames.h"
#include "platform/wasi_state.h"
#include "platform/emscripten.h"
#include "internal/gc_types.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>
#include <stdio.h>

WASM_PRIVATE wasm_global_import_t* wasm__find_global_import(wasm_runtime_t* rt,
                                                      const char* module,
                                                      const char* name) {
    uint32_t i = rt ? rt->num_global_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->global_imports[i].module, module) == 0 &&
            strcmp(rt->global_imports[i].name, name) == 0)
            return &rt->global_imports[i];
    }

    return NULL;
}

WASM_PRIVATE int wasm__has_import_named(const wasm_runtime_t* rt,
                                  const char* module,
                                  const char* name) {
    uint32_t i;

    if (!rt || !module || !name) return 0;

    for (i = 0; i < rt->num_imports; i++) {
        if (strcmp(rt->imports[i].module, module) == 0 &&
            strcmp(rt->imports[i].name, name) == 0)
            return 1;
    }
    for (i = 0; i < rt->num_global_imports; i++) {
        if (strcmp(rt->global_imports[i].module, module) == 0 &&
            strcmp(rt->global_imports[i].name, name) == 0)
            return 1;
    }
    for (i = 0; i < rt->num_table_imports; i++) {
        if (strcmp(rt->table_imports[i].module, module) == 0 &&
            strcmp(rt->table_imports[i].name, name) == 0)
            return 1;
    }
    for (i = 0; i < rt->num_memory_imports; i++) {
        if (strcmp(rt->memory_imports[i].module, module) == 0 &&
            strcmp(rt->memory_imports[i].name, name) == 0)
            return 1;
    }
    for (i = 0; i < rt->num_tag_imports; i++) {
        if (strcmp(rt->tag_imports[i].module, module) == 0 &&
            strcmp(rt->tag_imports[i].name, name) == 0)
            return 1;
    }

    return 0;
}

WASM_PRIVATE wasm_table_import_t* wasm__find_table_import(wasm_runtime_t* rt,
                                                    const char* module,
                                                    const char* name) {
    uint32_t i = rt ? rt->num_table_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->table_imports[i].module, module) == 0 &&
            strcmp(rt->table_imports[i].name, name) == 0)
            return &rt->table_imports[i];
    }

    return NULL;
}

WASM_PRIVATE wasm_memory_import_t* wasm__find_memory_import(wasm_runtime_t* rt,
                                                      const char* module,
                                                      const char* name) {
    uint32_t i = rt ? rt->num_memory_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->memory_imports[i].module, module) == 0 &&
            strcmp(rt->memory_imports[i].name, name) == 0)
            return &rt->memory_imports[i];
    }

    return NULL;
}

WASM_PRIVATE wasm_tag_import_t* wasm__find_tag_import(wasm_runtime_t* rt,
                                                const char* module,
                                                const char* name) {
    uint32_t i = rt ? rt->num_tag_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->tag_imports[i].module, module) == 0 &&
            strcmp(rt->tag_imports[i].name, name) == 0)
            return &rt->tag_imports[i];
    }

    return NULL;
}

static void wasm__gc_heap_destroy(wasm_runtime_t* rt) {
    WASM_GC_FREE(rt->gc_heap);
    rt->gc_heap = NULL;
    rt->gc_heap_size = 0;
    rt->gc_heap_offset = 0;
    rt->gc_allocations = NULL;
}

WASM_PRIVATE void wasm__gc_register_module(wasm_runtime_t* rt, wasm_module_t* mod) {
    if (!rt || !mod) return;
    if (mod->gc_prev || mod->gc_next || rt->gc_modules == mod) return;

    mod->gc_prev = NULL;
    mod->gc_next = rt->gc_modules;
    if (rt->gc_modules) rt->gc_modules->gc_prev = mod;
    rt->gc_modules = mod;
}

WASM_PRIVATE void wasm__gc_unregister_module(wasm_runtime_t* rt, wasm_module_t* mod) {
    if (!rt || !mod) return;

    if (mod->gc_prev)
        mod->gc_prev->gc_next = mod->gc_next;
    else if (rt->gc_modules == mod)
        rt->gc_modules = mod->gc_next;

    if (mod->gc_next) mod->gc_next->gc_prev = mod->gc_prev;
    mod->gc_prev = NULL;
    mod->gc_next = NULL;
}

WASM_PRIVATE int wasm__gc_ptr_in_heap(const wasm_runtime_t* rt, const void* ptr) {
    const uint8_t* addr = (const uint8_t*)ptr;

    if (!rt || !rt->gc_heap || !ptr) return 0;
    return addr >= rt->gc_heap && addr < rt->gc_heap + rt->gc_heap_size;
}

WASM_PRIVATE wasm_error_t wasm__gc_heap_init(wasm_runtime_t* rt, size_t size) {
    wasm__gc_heap_destroy(rt);
    if (size == 0) return WASM_OK;

    rt->gc_heap = (uint8_t*)WASM_GC_ALLOC(size);
    if (!rt->gc_heap) return WASM_ERR_OOM;

    rt->gc_heap_size = size;
    rt->gc_heap_offset = 0;
    rt->gc_allocations = NULL;
    return WASM_OK;
}

static void* wasm__gc_bump_alloc(wasm_runtime_t* rt, size_t size, size_t alignment) {
    size_t aligned_offset = wasm__align_up_size(rt->gc_heap_offset, alignment);

    if (!rt->gc_heap) return NULL;
    if (aligned_offset == (size_t)-1) return NULL;
    if (aligned_offset > rt->gc_heap_size || size > rt->gc_heap_size - aligned_offset) return NULL;

    rt->gc_heap_offset = aligned_offset + size;
    return rt->gc_heap + aligned_offset;
}

static wasm_gc_header_t* wasm__gc_alloc_object(wasm_runtime_t* rt,
                                               const wasm_module_t* mod,
                                               uint32_t type_index,
                                               wasm_gc_object_kind_t kind,
                                               size_t object_size) {
    wasm_gc_header_t* object;

    object = (wasm_gc_header_t*)wasm__gc_bump_alloc(rt, object_size, (size_t)WASM__ALIGNOF(wasm_value_t));
    if (!object) {
        wasm_gc_collect(rt);
        object = (wasm_gc_header_t*)wasm__gc_bump_alloc(rt, object_size, (size_t)WASM__ALIGNOF(wasm_value_t));
        if (!object) return NULL;
    }

    memset(object, 0, object_size);
    object->next_alloc = rt->gc_allocations;
    object->object_size = object_size;
    object->module = (wasm_module_t*)mod;
    object->type_index = type_index;
    object->kind = (uint8_t)kind;
    object->marked = 0;
    rt->gc_allocations = object;
    return object;
}

WASM_PRIVATE wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type) {
    size_t object_size;

    if (!wasm__gc_struct_size(type, &object_size)) return NULL;
    return (wasm_gc_struct_t*)wasm__gc_alloc_object(rt, mod, type_index, WASM_GC_OBJ_STRUCT, object_size);
}

WASM_PRIVATE wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length) {
    size_t object_size;
    wasm_gc_array_t* object;

    if (!wasm__gc_array_total_size(type, length, &object_size)) return NULL;
    object = (wasm_gc_array_t*)wasm__gc_alloc_object(rt, mod, type_index, WASM_GC_OBJ_ARRAY, object_size);
    if (!object) return NULL;
    object->length = length;
    return object;
}

WASM_PRIVATE void wasm__arena_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->frame_arena);
    rt->frame_arena = NULL;
    rt->frame_arena_size = 0;
    rt->frame_arena_offset = 0;
}

WASM_PRIVATE wasm_error_t wasm__arena_init(wasm_runtime_t* rt, size_t size) {
    wasm__arena_destroy(rt);
    if (size == 0) return WASM_OK;

    rt->frame_arena = (uint8_t*)WASM_MALLOC(size);
    if (!rt->frame_arena) return WASM_ERR_OOM;

    rt->frame_arena_size = size;
    rt->frame_arena_offset = 0;
    return WASM_OK;
}

WASM_PRIVATE void* wasm__arena_alloc(wasm_runtime_t* rt, size_t size) {
    size_t alignment = sizeof(void*);
    size_t aligned_offset = rt->frame_arena_offset;

    if (!rt->frame_arena) return NULL;
    if (alignment > 1) {
        size_t remainder = aligned_offset % alignment;
        if (remainder != 0) aligned_offset += alignment - remainder;
    }
    if (aligned_offset > rt->frame_arena_size || size > rt->frame_arena_size - aligned_offset)
        return NULL;

    rt->frame_arena_offset = aligned_offset + size;
    return rt->frame_arena + aligned_offset;
}

WASM_PRIVATE void wasm__arena_reset(wasm_runtime_t* rt, size_t saved_offset) {
    if (saved_offset <= rt->frame_arena_size) rt->frame_arena_offset = saved_offset;
}

void wasm_config_default(wasm_config_t* config) {
    if (!config) return;

    config->max_stack_values = WASM_MAX_STACK;
    config->max_call_depth = WASM_MAX_CALL_DEPTH;
    config->max_labels = WASM_MAX_LABELS;
    config->initial_gc_heap_size = (size_t)WASM_GC_HEAP_SIZE;
    config->frame_arena_size = (size_t)WASM_FRAME_ARENA_SIZE;
    config->lazy_validation = 0;
    config->max_function_locals = WASM_MAX_FUNCTION_LOCALS;
}

static wasm_error_t wasm__stack_init(wasm_runtime_t* rt, uint32_t max_stack) {
    if (max_stack == 0) return WASM_ERR_MALFORMED;

    rt->stack = (wasm_value_t*)wasm__malloc_array((size_t)max_stack, sizeof(*rt->stack));
    if (!rt->stack) return WASM_ERR_OOM;

    rt->max_stack = max_stack;
    rt->sp = 0;
    return WASM_OK;
}

static void wasm__stack_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->stack);
    rt->stack = NULL;
    rt->max_stack = 0;
    rt->sp = 0;
}

static wasm_error_t wasm__backtrace_init(wasm_runtime_t* rt, uint32_t max_depth) {
    if (max_depth == 0) return WASM_ERR_MALFORMED;

    rt->backtrace_func_indices = (uint32_t*)wasm__malloc_array((size_t)max_depth, sizeof(*rt->backtrace_func_indices));
    if (!rt->backtrace_func_indices) return WASM_ERR_OOM;

    rt->backtrace_offsets = (uint32_t*)wasm__malloc_array((size_t)max_depth, sizeof(*rt->backtrace_offsets));
    if (!rt->backtrace_offsets) {
        WASM_FREE(rt->backtrace_func_indices);
        rt->backtrace_func_indices = NULL;
        return WASM_ERR_OOM;
    }

    rt->backtrace_depth = 0;
    return WASM_OK;
}

static void wasm__backtrace_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->backtrace_func_indices);
    WASM_FREE(rt->backtrace_offsets);
    rt->backtrace_func_indices = NULL;
    rt->backtrace_offsets = NULL;
    rt->backtrace_depth = 0;
}

wasm_runtime_t* wasm_runtime_new(const wasm_config_t* config) {
    wasm_runtime_t* rt = (wasm_runtime_t*)WASM_MALLOC(sizeof(*rt));

    if (!rt) return NULL;
    (void)wasm_init(rt, config);
    return rt;
}

void wasm_runtime_free(wasm_runtime_t* rt) {
    if (!rt) return;
    wasm_destroy(rt);
    WASM_FREE(rt);
}

wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config) {
    wasm_config_t cfg;
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;

    memset(rt, 0, sizeof(*rt));
    rt->next_tag_identity = 1;

    if (config)
        cfg = *config;
    else
        wasm_config_default(&cfg);

    if (cfg.max_stack_values == 0 || cfg.max_call_depth == 0 || cfg.max_labels == 0) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "invalid runtime config");
        return WASM_ERR_MALFORMED;
    }

    rt->enabled_features = WASM__DEFAULT_FEATURES;
    rt->max_call_frames = cfg.max_call_depth;
    rt->max_labels = cfg.max_labels;
    rt->lazy_validation = cfg.lazy_validation != 0;
    rt->max_function_locals = cfg.max_function_locals;

    err = wasm__stack_init(rt, cfg.max_stack_values);
    if (err != WASM_OK) {
        WASM__SET_ERR(rt, err, "%s", err == WASM_ERR_OOM ? "failed to allocate value stack" : "invalid runtime config");
        return err;
    }

    if (wasm__gc_heap_init(rt, cfg.initial_gc_heap_size) != WASM_OK) {
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate GC heap");
        return WASM_ERR_OOM;
    }

    if (wasm__arena_init(rt, cfg.frame_arena_size) != WASM_OK) {
        wasm__gc_heap_destroy(rt);
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate frame arena");
        return WASM_ERR_OOM;
    }

    if (wasm__call_frame_stack_init(rt) != WASM_OK) {
        wasm__arena_destroy(rt);
        wasm__gc_heap_destroy(rt);
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate call frame stack");
        return WASM_ERR_OOM;
    }

    if (wasm__backtrace_init(rt, cfg.max_call_depth) != WASM_OK) {
        wasm__call_frame_stack_destroy(rt);
        wasm__arena_destroy(rt);
        wasm__gc_heap_destroy(rt);
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate backtrace buffers");
        return WASM_ERR_OOM;
    }

    return WASM_OK;
}

wasm_error_t wasm_runtime_last_error(const wasm_runtime_t* rt) {
    return rt ? rt->last_error : WASM_OK;
}

const char* wasm_runtime_error_message(const wasm_runtime_t* rt) {
    if (!rt) return "unknown error";
    if (rt->error_msg[0] != '\0') return rt->error_msg;
    return wasm_error_string(rt->last_error);
}

void wasm_runtime_clear_error(wasm_runtime_t* rt) {
    if (!rt) return;
    rt->last_error = WASM_OK;
    rt->error_msg[0] = '\0';
}

int32_t wasm_runtime_exit_code(const wasm_runtime_t* rt) {
    return rt ? rt->exit_code : 0;
}

void wasm_runtime_set_error(wasm_runtime_t* rt, wasm_error_t err, const char* message) {
    if (!rt) return;
    rt->last_error = err;
    if (message)
        wasm__snprintf(rt->error_msg, sizeof(rt->error_msg), "%s", message);
    else
        rt->error_msg[0] = '\0';
}

void wasm_runtime_set_enabled_features(wasm_runtime_t* rt, uint32_t enabled_features) {
    if (!rt) return;
    rt->enabled_features = enabled_features;
}

uint32_t wasm_runtime_enabled_features(const wasm_runtime_t* rt) {
    return rt ? rt->enabled_features : 0u;
}

void wasm_destroy(wasm_runtime_t* rt) {
    uint32_t i;
    wasm__externref_box_t* box;
    wasm__exception_object_t* exception_object;

    if (!rt) return;

    while (rt->failed_modules) {
        wasm_module_t* failed = rt->failed_modules;
        rt->failed_modules = failed->failed_next;
        failed->failed_next = NULL;
        wasm_free_module(failed);
    }

    for (i = 0; i < rt->num_imports; i++) {
        WASM_FREE((void*)rt->imports[i].module);
        WASM_FREE((void*)rt->imports[i].name);
        wasm__free_functype(&rt->imports[i].type);
    }
    for (i = 0; i < rt->num_global_imports; i++) {
        WASM_FREE((void*)rt->global_imports[i].module);
        WASM_FREE((void*)rt->global_imports[i].name);
    }
    for (i = 0; i < rt->num_table_imports; i++) {
        WASM_FREE((void*)rt->table_imports[i].module);
        WASM_FREE((void*)rt->table_imports[i].name);
    }
    for (i = 0; i < rt->num_memory_imports; i++) {
        WASM_FREE((void*)rt->memory_imports[i].module);
        WASM_FREE((void*)rt->memory_imports[i].name);
    }
    for (i = 0; i < rt->num_tag_imports; i++) {
        WASM_FREE((void*)rt->tag_imports[i].module);
        WASM_FREE((void*)rt->tag_imports[i].name);
        wasm__free_functype(&rt->tag_imports[i].type);
    }
    WASM_FREE(rt->imports);
    WASM_FREE(rt->func_refs);
    WASM_FREE(rt->global_imports);
    WASM_FREE(rt->table_imports);
    WASM_FREE(rt->memory_imports);
    WASM_FREE(rt->tag_imports);
    box = rt->externref_boxes;
    while (box) {
        wasm__externref_box_t* next = box->next;
        WASM_FREE(box);
        box = next;
    }
    exception_object = rt->exception_objects;
    while (exception_object) {
        wasm__exception_object_t* next = exception_object->next;
        WASM_FREE(exception_object->values);
        WASM_FREE(exception_object);
        exception_object = next;
    }
    rt->exception_objects = NULL;
    rt->externref_boxes = NULL;
    WASM_FREE(rt->validator_stack);
    WASM_FREE(rt->validator_stack_reftypes);
    WASM_FREE(rt->validator_frames);
    WASM_FREE(rt->validator_local_init_bits);
    WASM_FREE(rt->validator_local_init_entry_bits);
    wasm__free_exception_state(&rt->pending_exception);
    wasm__backtrace_destroy(rt);
    wasm__call_frame_stack_destroy(rt);
    wasm__arena_destroy(rt);
    rt->gc_modules = NULL;
    wasm__gc_heap_destroy(rt);
#if WASM_ENABLE_PLATFORM
    if (rt->wasi_state) wasm__wasi_state_destroy(rt);
    wasm__emscripten_state_destroy(rt);
#endif
    wasm__stack_destroy(rt);
    memset(rt, 0, sizeof(*rt));
}

WASM_PRIVATE wasm_error_t wasm__register_import_internal(wasm_runtime_t* rt,
                                                   const wasm_import_t* imp,
                                                   int zeroed_type_is_unspecified) {
    wasm_import_t copy;
    wasm_error_t err;

    if (!rt || !imp || !imp->module || !imp->name || !imp->func) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid function import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_imports >= rt->cap_imports) {
        uint32_t new_cap = rt->cap_imports ? rt->cap_imports * 2 : 16;
        wasm_import_t* a = (wasm_import_t*)wasm__realloc_array(rt->imports, new_cap, sizeof(wasm_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->imports = a;
        rt->cap_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.func = imp->func;
    copy.userdata = imp->userdata;
    copy.type_module = imp->type_module;
    copy.type_index = imp->type_index;
    copy.has_type_context = imp->has_type_context;
    err = wasm__copy_functype(&copy.type, &imp->type);
    if (err != WASM_OK) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return err;
    }
    if (zeroed_type_is_unspecified && wasm__functype_is_zeroed(&imp->type))
        wasm__functype_mark_unspecified(&copy.type);

    rt->imports[rt->num_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp) {
    return wasm__register_import_internal(rt, imp, 1);
}

wasm_error_t wasm_register_global_import(wasm_runtime_t* rt, const wasm_global_import_t* imp) {
    wasm_global_import_t copy;

    if (!rt || !imp || !imp->module || !imp->name || !imp->value) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid global import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_global_imports >= rt->cap_global_imports) {
        uint32_t new_cap = rt->cap_global_imports ? rt->cap_global_imports * 2 : 16;
        wasm_global_import_t* a =
            (wasm_global_import_t*)wasm__realloc_array(rt->global_imports, new_cap, sizeof(wasm_global_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->global_imports = a;
        rt->cap_global_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.type = imp->type;
    copy.ref_type = imp->ref_type;
    copy.is_mutable = imp->is_mutable;
    copy.value = imp->value;
    rt->global_imports[rt->num_global_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_table_import(wasm_runtime_t* rt, const wasm_table_import_t* imp) {
    wasm_table_import_t copy;

    if (!rt || !imp || !imp->module || !imp->name || !imp->table) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid table import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_table_imports >= rt->cap_table_imports) {
        uint32_t new_cap = rt->cap_table_imports ? rt->cap_table_imports * 2 : 16;
        wasm_table_import_t* a =
            (wasm_table_import_t*)wasm__realloc_array(rt->table_imports, new_cap, sizeof(wasm_table_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->table_imports = a;
        rt->cap_table_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.table = wasm__table_actual(imp->table);
    if (!copy.table) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid table import");
        return WASM_ERR_MALFORMED;
    }

    rt->table_imports[rt->num_table_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_memory_import(wasm_runtime_t* rt, const wasm_memory_import_t* imp) {
    wasm_memory_import_t copy;

    if (!rt || !imp || !imp->module || !imp->name || !imp->memory) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid memory import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_memory_imports >= rt->cap_memory_imports) {
        uint32_t new_cap = rt->cap_memory_imports ? rt->cap_memory_imports * 2 : 16;
        wasm_memory_import_t* a =
            (wasm_memory_import_t*)wasm__realloc_array(rt->memory_imports, new_cap, sizeof(wasm_memory_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->memory_imports = a;
        rt->cap_memory_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.memory = wasm__memory_actual(imp->memory);
    if (!copy.memory) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid memory import");
        return WASM_ERR_MALFORMED;
    }

    rt->memory_imports[rt->num_memory_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_tag_import(wasm_runtime_t* rt, const wasm_tag_import_t* imp) {
    wasm_tag_import_t copy;
    wasm_error_t err;

    if (!rt || !imp || !imp->module || !imp->name) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid tag import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_tag_imports >= rt->cap_tag_imports) {
        uint32_t new_cap = rt->cap_tag_imports ? rt->cap_tag_imports * 2 : 16;
        wasm_tag_import_t* a =
            (wasm_tag_import_t*)wasm__realloc_array(rt->tag_imports, new_cap, sizeof(wasm_tag_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->tag_imports = a;
        rt->cap_tag_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }

    err = wasm__copy_functype(&copy.type, &imp->type);
    if (err != WASM_OK) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return err;
    }
    copy.type_module = imp->type_module;
    copy.type_index = imp->type_index;
    copy.has_type_context = imp->has_type_context;
    copy.identity = imp->identity ? imp->identity : wasm__alloc_tag_identity(rt);

    rt->tag_imports[rt->num_tag_imports++] = copy;
    return WASM_OK;
}

void wasm_enable_feature(wasm_runtime_t* rt, uint32_t flag) {
    rt->enabled_features |= flag;
}

void wasm_disable_feature(wasm_runtime_t* rt, uint32_t flag) {
    rt->enabled_features &= ~flag;
}

void wasm_enable_all_features(wasm_runtime_t* rt) {
    rt->enabled_features = WASM__IMPLEMENTED_FEATURES;
}
