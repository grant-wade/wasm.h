#include "runtime.h"
#include "handles.h"
#include "atomics.h"
#include "platform/threads.h"
#include "values.h"
#include "module.h"
#include "types/canonical.h"
#include "types/cross_module.h"
#include "types/references.h"
#include "memory_internal.h"
#include "reader.h"
#include "exec/cont.h"
#include "exec/interpreter.h"
#include "gc.h"
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
    wasm__free_sized(&rt->allocator, rt->gc_heap, rt->gc_heap_size, WASM__BLOCK_ALIGN);
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
    if (rt->max_gc_heap_bytes != 0 && size > rt->max_gc_heap_bytes) size = (size_t)rt->max_gc_heap_bytes;
    rt->gc_heap_min_size = size;
    if (size == 0) return WASM_OK;

    rt->gc_heap = (uint8_t*)wasm__alloc_sized(&rt->allocator, size, WASM__BLOCK_ALIGN);
    if (!rt->gc_heap) return WASM_ERR_OOM;

    rt->gc_heap_size = size;
    rt->gc_heap_offset = 0;
    rt->gc_allocations = NULL;
    return WASM_OK;
}

WASM_PRIVATE void wasm__gc_push_root(wasm_runtime_t* rt, wasm__gc_root_t* root, wasm_value_t* const* values,
                                     const uint32_t* count) {
    root->prev = rt->gc_roots;
    root->values = values;
    root->count = count;
    root->mirror = NULL;
    rt->gc_roots = root;
}

WASM_PRIVATE void wasm__gc_pop_root(wasm_runtime_t* rt, wasm__gc_root_t* root) {
    rt->gc_roots = root->prev;
}

void wasm_runtime_interrupt(wasm_runtime_t* rt) {
    if (!rt) return;
    wasm__atomic_store_size(&rt->interrupt_requested, 1u);
    wasm__platform_atomic_wake_all();
}

WASM_PRIVATE int wasm__runtime_take_interrupt(wasm_runtime_t* rt) {
    if (!rt || wasm__atomic_load_size(&rt->interrupt_requested) == 0u) return 0;
    wasm__atomic_store_size(&rt->interrupt_requested, 0u);
    WASM__SET_ERR(rt, WASM_ERR_INTERRUPTED, "%s", "execution interrupted by the host");
    return 1;
}

#define WASM__HANDLE_GC_MIN_THRESHOLD 1024u

WASM_PRIVATE void wasm__gc_collect_handles_if_needed(wasm_runtime_t* rt) {
    uint32_t threshold;
    uint32_t live;

    if (!rt) return;
    threshold = rt->handle_gc_threshold ? rt->handle_gc_threshold : WASM__HANDLE_GC_MIN_THRESHOLD;
    if (rt->externref_boxes.count + rt->exception_objects.count + rt->cont_objects.count < threshold) return;

    wasm_gc_collect(rt);
    live = rt->externref_boxes.count + rt->exception_objects.count + rt->cont_objects.count;
    rt->handle_gc_threshold = live > UINT32_MAX / 2u ? UINT32_MAX : live * 2u;
    if (rt->handle_gc_threshold < WASM__HANDLE_GC_MIN_THRESHOLD)
        rt->handle_gc_threshold = WASM__HANDLE_GC_MIN_THRESHOLD;
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

#ifdef WASM_GC_STRESS
    if (rt->gc_heap && wasm__gc_collect(rt, object_size) != WASM_OK) return NULL;
#endif
    object = (wasm_gc_header_t*)wasm__gc_bump_alloc(rt, object_size, (size_t)WASM__ALIGNOF(wasm_value_t));
    if (!object) {
        if (wasm__gc_collect(rt, object_size) != WASM_OK) return NULL;
        object = (wasm_gc_header_t*)wasm__gc_bump_alloc(rt, object_size, (size_t)WASM__ALIGNOF(wasm_value_t));
        if (!object) {
            WASM__SET_ERR(rt, WASM_ERR_OOM, "no room for a %zu-byte GC object", object_size);
            return NULL;
        }
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
    wasm_gc_struct_t* object;

    if (type->object_size == 0) return NULL;
    object = (wasm_gc_struct_t*)wasm__gc_alloc_object(rt, mod, type_index, WASM_GC_OBJ_STRUCT, type->object_size);
    if (object) object->header.has_descriptor = (uint8_t)(type->descriptor_offset != 0);
    return object;
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

void wasm_config_default(wasm_config_t* config) {
    if (!config) return;

    config->max_stack_values = WASM_MAX_STACK;
    config->max_call_depth = WASM_MAX_CALL_DEPTH;
    config->max_labels = WASM_MAX_LABELS;
    config->initial_gc_heap_size = (size_t)WASM_GC_HEAP_SIZE;
    config->frame_arena_size = (size_t)WASM_FRAME_ARENA_SIZE;
    config->cont_stack_values = WASM_CONT_STACK_VALUES;
    config->cont_call_depth = WASM_CONT_CALL_DEPTH;
    config->cont_frame_arena_size = (size_t)WASM_CONT_FRAME_ARENA_SIZE;
    config->lazy_validation = 0;
    config->max_function_locals = WASM_MAX_FUNCTION_LOCALS;
    config->max_memory_bytes = (uint64_t)WASM_MAX_MEMORY_BYTES;
    config->max_total_memory_bytes = 0;
    config->max_table_elements = (uint64_t)WASM_MAX_TABLE_ELEMENTS;
    config->max_gc_heap_bytes = (uint64_t)WASM_MAX_GC_HEAP_BYTES;
    config->max_cont_stacks = WASM_MAX_CONT_STACKS;
    config->features = WASM_FEATURES_ALL;
}

static wasm_error_t wasm__backtrace_init(wasm_runtime_t* rt, uint32_t max_depth) {
    if (max_depth == 0) return WASM_ERR_MALFORMED;

    rt->backtrace_func_indices = (uint32_t*)wasm__malloc_array(&rt->allocator, (size_t)max_depth, sizeof(*rt->backtrace_func_indices));
    if (!rt->backtrace_func_indices) return WASM_ERR_OOM;

    rt->backtrace_offsets = (uint32_t*)wasm__malloc_array(&rt->allocator, (size_t)max_depth, sizeof(*rt->backtrace_offsets));
    rt->backtrace_modules = (wasm_module_t**)wasm__malloc_array(&rt->allocator, (size_t)max_depth, sizeof(*rt->backtrace_modules));
    if (!rt->backtrace_offsets || !rt->backtrace_modules) {
        wasm__free(&rt->allocator, rt->backtrace_func_indices);
        wasm__free(&rt->allocator, rt->backtrace_offsets);
        wasm__free(&rt->allocator, rt->backtrace_modules);
        rt->backtrace_func_indices = NULL;
        rt->backtrace_offsets = NULL;
        rt->backtrace_modules = NULL;
        return WASM_ERR_OOM;
    }

    rt->backtrace_depth = 0;
    rt->backtrace_capacity = max_depth;
    return WASM_OK;
}

static void wasm__backtrace_destroy(wasm_runtime_t* rt) {
    wasm__free(&rt->allocator, rt->backtrace_func_indices);
    wasm__free(&rt->allocator, rt->backtrace_offsets);
    wasm__free(&rt->allocator, rt->backtrace_modules);
    rt->backtrace_func_indices = NULL;
    rt->backtrace_offsets = NULL;
    rt->backtrace_modules = NULL;
    rt->backtrace_depth = 0;
    rt->backtrace_capacity = 0;
}

wasm_runtime_t* wasm_runtime_new(const wasm_allocator_t* allocator, const wasm_config_t* config) {
    wasm_runtime_t* rt;

    if (!wasm__allocator_valid(allocator)) return NULL;
    rt = (wasm_runtime_t*)wasm__alloc_sized(allocator, sizeof(*rt), WASM__ALIGNOF(wasm_runtime_t));
    if (!rt) return NULL;
    if (wasm__runtime_init(rt, allocator, config) != WASM_OK) {
        wasm_runtime_free(rt);
        return NULL;
    }
    return rt;
}

void wasm_runtime_free(wasm_runtime_t* rt) {
    wasm_allocator_t allocator;

    if (!rt) return;
    allocator = rt->allocator;
    wasm__runtime_destroy(rt);
    wasm__free_sized(&allocator, rt, sizeof(*rt), WASM__ALIGNOF(wasm_runtime_t));
}

const wasm_allocator_t* wasm_runtime_allocator(const wasm_runtime_t* rt) {
    return rt ? &rt->allocator : NULL;
}

WASM_PRIVATE wasm_error_t wasm__runtime_init(wasm_runtime_t* rt, const wasm_allocator_t* allocator,
                                             const wasm_config_t* config) {
    wasm_config_t cfg;
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;

    memset(rt, 0, sizeof(*rt));
    if (!wasm__allocator_valid(allocator)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "invalid allocator");
        return WASM_ERR_MALFORMED;
    }
    rt->allocator = *allocator;
    rt->next_tag_identity = 1;

    if (config)
        cfg = *config;
    else
        wasm_config_default(&cfg);

    if (cfg.max_stack_values == 0 || cfg.max_call_depth == 0 || cfg.max_labels == 0 || cfg.cont_stack_values == 0 ||
        cfg.cont_call_depth == 0 || (cfg.features & ~WASM_FEATURES_ALL) != 0) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "invalid runtime config");
        return WASM_ERR_MALFORMED;
    }

    rt->features = cfg.features;
    rt->max_labels = cfg.max_labels;
    rt->lazy_validation = cfg.lazy_validation != 0;
    rt->max_function_locals = cfg.max_function_locals;
    rt->max_memory_bytes = cfg.max_memory_bytes;
    rt->max_total_memory_bytes = cfg.max_total_memory_bytes;
    rt->max_table_elements = cfg.max_table_elements;
    rt->max_gc_heap_bytes = cfg.max_gc_heap_bytes;
    rt->max_cont_stacks = cfg.max_cont_stacks;
    rt->cont_stack_values = cfg.cont_stack_values;
    rt->cont_call_depth = cfg.cont_call_depth;
    rt->cont_frame_arena_size = cfg.cont_frame_arena_size;

    err = wasm__exec_stack_init(rt, &rt->main_stack, cfg.max_stack_values, cfg.max_call_depth, cfg.frame_arena_size);
    if (err != WASM_OK) {
        WASM__SET_ERR(rt, err, "%s", err == WASM_ERR_OOM ? "failed to allocate execution stack" : "invalid runtime config");
        return err;
    }
    rt->exec = &rt->main_stack;

    if (wasm__gc_heap_init(rt, cfg.initial_gc_heap_size) != WASM_OK) {
        wasm__exec_stack_destroy(rt, &rt->main_stack);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate GC heap");
        return WASM_ERR_OOM;
    }

    if (wasm__backtrace_init(rt, cfg.max_call_depth) != WASM_OK) {
        wasm__gc_heap_destroy(rt);
        wasm__exec_stack_destroy(rt, &rt->main_stack);
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

uint64_t wasm_runtime_features(const wasm_runtime_t* rt) {
    return rt ? rt->features : 0u;
}

WASM_PRIVATE void wasm__runtime_destroy(wasm_runtime_t* rt) {
    uint32_t i;

    if (!rt) return;

    wasm__interp_cancel(rt);
    while (rt->failed_modules) {
        wasm_module_t* failed = rt->failed_modules;
        rt->failed_modules = failed->failed_next;
        failed->failed_next = NULL;
        wasm_module_free(failed);
    }

    for (i = 0; i < rt->num_imports; i++) {
        wasm__free(&rt->allocator, (void*)rt->imports[i].module);
        wasm__free(&rt->allocator, (void*)rt->imports[i].name);
        wasm__free_functype(&rt->allocator, &rt->imports[i].type);
    }
    for (i = 0; i < rt->num_global_imports; i++) {
        wasm__free(&rt->allocator, (void*)rt->global_imports[i].module);
        wasm__free(&rt->allocator, (void*)rt->global_imports[i].name);
    }
    for (i = 0; i < rt->num_table_imports; i++) {
        wasm__free(&rt->allocator, (void*)rt->table_imports[i].module);
        wasm__free(&rt->allocator, (void*)rt->table_imports[i].name);
    }
    for (i = 0; i < rt->num_memory_imports; i++) {
        wasm__free(&rt->allocator, (void*)rt->memory_imports[i].module);
        wasm__free(&rt->allocator, (void*)rt->memory_imports[i].name);
    }
    for (i = 0; i < rt->num_tag_imports; i++) {
        wasm__free(&rt->allocator, (void*)rt->tag_imports[i].module);
        wasm__free(&rt->allocator, (void*)rt->tag_imports[i].name);
        wasm__free_functype(&rt->allocator, &rt->tag_imports[i].type);
    }
    wasm__free(&rt->allocator, rt->imports);
    wasm__free(&rt->allocator, rt->func_refs);
    wasm__free(&rt->allocator, rt->global_imports);
    wasm__free(&rt->allocator, rt->table_imports);
    wasm__free(&rt->allocator, rt->memory_imports);
    wasm__free(&rt->allocator, rt->tag_imports);
    for (i = 0; i < rt->externref_boxes.capacity; i++) wasm__free(&rt->allocator, rt->externref_boxes.slots[i]);
    for (i = 0; i < rt->exception_objects.capacity; i++) {
        wasm__exception_object_t* exception_object = (wasm__exception_object_t*)rt->exception_objects.slots[i];

        if (!exception_object) continue;
        wasm__free(&rt->allocator, exception_object->values);
        wasm__free(&rt->allocator, exception_object);
    }
    while (rt->linked_funcs) {
        wasm__linked_func_t* next = rt->linked_funcs->next;

        wasm__free(&rt->allocator, rt->linked_funcs);
        rt->linked_funcs = next;
    }
    while (rt->held_values) {
        wasm__held_block_t* next = rt->held_values->next;

        wasm__free(&rt->allocator, rt->held_values);
        rt->held_values = next;
    }
    wasm__cont_free_all(rt);
    wasm__gc_free_retired_types(rt);
    wasm__free_type_groups(rt);
    wasm__handle_set_free(&rt->allocator, &rt->exception_objects);
    wasm__handle_set_free(&rt->allocator, &rt->externref_boxes);
    rt->handle_gc_threshold = 0;
    wasm__free(&rt->allocator, rt->validator_stack);
    wasm__free(&rt->allocator, rt->validator_stack_reftypes);
    wasm__free(&rt->allocator, rt->validator_frames);
    wasm__free(&rt->allocator, rt->validator_local_init_bits);
    wasm__free(&rt->allocator, rt->validator_local_init_entry_bits);
    wasm__free(&rt->allocator, rt->validator_stack_slots);
    wasm__free(&rt->allocator, rt->validator_stack_refs);
    wasm__free(&rt->allocator, rt->validator_heights);
    wasm__free(&rt->allocator, rt->validator_operand_types);
    wasm__free(&rt->allocator, rt->validator_maps);
    wasm__free(&rt->allocator, rt->validator_map_bits);
    wasm__free_exception_state(&rt->allocator, &rt->pending_exception);
    wasm__backtrace_destroy(rt);
    wasm__exec_stack_destroy(rt, &rt->main_stack);
    wasm__arena_free_spare(rt);
    rt->gc_modules = NULL;
    wasm__gc_heap_destroy(rt);
#if WASM_ENABLE_PLATFORM
    if (rt->wasi_state) wasm__wasi_state_destroy(rt);
    wasm__emscripten_state_destroy(rt);
#endif
    memset(rt, 0, sizeof(*rt));
}

static int wasm__functype_has_type_index(const wasm_functype_t* ft) {
    uint32_t i;

    for (i = 0; ft->param_reftypes && i < ft->num_params; i++) {
        if (ft->param_reftypes[i].has_type_index) return 1;
    }
    for (i = 0; ft->result_reftypes && i < ft->num_results; i++) {
        if (ft->result_reftypes[i].has_type_index) return 1;
    }
    return 0;
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
    if (imp->has_type_context && imp->type_module && imp->type_module->rt != rt) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "a function type context must be a module of the same runtime");
        return WASM_ERR_MALFORMED;
    }
    if (!imp->has_type_context && wasm__functype_has_type_index(&imp->type)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "a function type with type indices needs a type context");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_imports >= rt->cap_imports) {
        uint32_t new_cap = rt->cap_imports ? rt->cap_imports * 2 : 16;
        wasm_import_t* a = (wasm_import_t*)wasm__realloc_array(&rt->allocator, rt->imports, new_cap, sizeof(wasm_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->imports = a;
        rt->cap_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(&rt->allocator, imp->module);
    copy.name = wasm__strdup(&rt->allocator, imp->name);
    if (!copy.module || !copy.name) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.func = imp->func;
    copy.userdata = imp->userdata;
    copy.type_module = imp->type_module;
    copy.type_index = imp->type_index;
    copy.has_type_context = imp->has_type_context;
    err = wasm__copy_functype(&rt->allocator, &copy.type, &imp->type);
    if (err != WASM_OK) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return err;
    }
    if (zeroed_type_is_unspecified && wasm__functype_is_zeroed(&imp->type))
        wasm__functype_mark_unspecified(&rt->allocator, &copy.type);

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
    if (imp->ref_type.has_type_index &&
        (!imp->type_module || imp->type_module->rt != rt || imp->ref_type.type_index >= imp->type_module->num_types)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "a global type with a type index needs its module");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_global_imports >= rt->cap_global_imports) {
        uint32_t new_cap = rt->cap_global_imports ? rt->cap_global_imports * 2 : 16;
        wasm_global_import_t* a =
            (wasm_global_import_t*)wasm__realloc_array(&rt->allocator, rt->global_imports, new_cap, sizeof(wasm_global_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->global_imports = a;
        rt->cap_global_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(&rt->allocator, imp->module);
    copy.name = wasm__strdup(&rt->allocator, imp->name);
    if (!copy.module || !copy.name) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.type = imp->type;
    copy.ref_type = imp->ref_type;
    copy.type_module = imp->ref_type.has_type_index ? imp->type_module : NULL;
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
            (wasm_table_import_t*)wasm__realloc_array(&rt->allocator, rt->table_imports, new_cap, sizeof(wasm_table_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->table_imports = a;
        rt->cap_table_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(&rt->allocator, imp->module);
    copy.name = wasm__strdup(&rt->allocator, imp->name);
    if (!copy.module || !copy.name) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.table = wasm__table_actual(imp->table);
    if (!copy.table) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
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
            (wasm_memory_import_t*)wasm__realloc_array(&rt->allocator, rt->memory_imports, new_cap, sizeof(wasm_memory_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->memory_imports = a;
        rt->cap_memory_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(&rt->allocator, imp->module);
    copy.name = wasm__strdup(&rt->allocator, imp->name);
    if (!copy.module || !copy.name) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.memory = wasm__memory_actual(imp->memory);
    if (!copy.memory) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid memory import");
        return WASM_ERR_MALFORMED;
    }

    rt->memory_imports[rt->num_memory_imports++] = copy;
    return WASM_OK;
}

/* Returns 1 if the type of imp equals type type_index of mod. */
static int wasm__tag_import_type_equal(const wasm_tag_import_t* imp, const wasm_module_t* mod, uint32_t type_index) {
    if (imp->has_type_context)
        return imp->type_module && wasm__type_equal_across_modules(imp->type_module, imp->type_index, mod, type_index);
    return wasm__host_functype_equal(mod, &imp->type, type_index);
}

/* Fails unless imp's identity is that of a tag of a live module or of a registered tag import, and imp's type
 * equals that tag's type. */
static wasm_error_t wasm__check_tag_identity(wasm_runtime_t* rt, const wasm_tag_import_t* imp) {
    const wasm_module_t* mod;
    uint32_t i;
    int equal = -1;

    for (mod = rt->gc_modules; mod && equal < 0; mod = mod->gc_next) {
        for (i = 0; i < mod->num_tags; i++) {
            if (mod->tags[i].identity != imp->identity) continue;
            equal = wasm__tag_import_type_equal(imp, mod, mod->tags[i].type_idx);
            break;
        }
    }
    for (i = 0; i < rt->num_tag_imports && equal < 0; i++) {
        const wasm_tag_import_t* other = &rt->tag_imports[i];

        if (other->identity != imp->identity) continue;
        if (other->has_type_context && other->type_module)
            equal = wasm__tag_import_type_equal(imp, other->type_module, other->type_index);
        else if (!other->has_type_context && !imp->has_type_context)
            equal = wasm__functype_equal(&imp->type, &other->type);
        else if (!other->has_type_context && imp->type_module)
            equal = wasm__tag_import_type_equal(other, imp->type_module, imp->type_index);
    }
    if (equal < 0) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown tag identity %llu", (unsigned long long)imp->identity);
        return WASM_ERR_MALFORMED;
    }
    if (!equal) {
        WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "tag identity %llu belongs to a tag of another type",
                      (unsigned long long)imp->identity);
        return WASM_ERR_TYPE_MISMATCH;
    }
    return WASM_OK;
}

wasm_error_t wasm_register_tag_import(wasm_runtime_t* rt, const wasm_tag_import_t* imp) {
    wasm_tag_import_t copy;
    wasm_error_t err;

    if (!rt || !imp || !imp->module || !imp->name) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid tag import");
        return WASM_ERR_MALFORMED;
    }
    if (!imp->has_type_context && wasm__functype_has_type_index(&imp->type)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "a tag type with type indices needs a type context");
        return WASM_ERR_MALFORMED;
    }
    if (imp->has_type_context && imp->type_module && imp->type_module->rt != rt) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "a tag type context must be a module of the same runtime");
        return WASM_ERR_MALFORMED;
    }
    if (imp->identity) {
        err = wasm__check_tag_identity(rt, imp);
        if (err != WASM_OK) return err;
    }

    if (rt->num_tag_imports >= rt->cap_tag_imports) {
        uint32_t new_cap = rt->cap_tag_imports ? rt->cap_tag_imports * 2 : 16;
        wasm_tag_import_t* a =
            (wasm_tag_import_t*)wasm__realloc_array(&rt->allocator, rt->tag_imports, new_cap, sizeof(wasm_tag_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->tag_imports = a;
        rt->cap_tag_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(&rt->allocator, imp->module);
    copy.name = wasm__strdup(&rt->allocator, imp->name);
    if (!copy.module || !copy.name) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return WASM_ERR_OOM;
    }

    err = wasm__copy_functype(&rt->allocator, &copy.type, &imp->type);
    if (err != WASM_OK) {
        wasm__free(&rt->allocator, (void*)copy.module);
        wasm__free(&rt->allocator, (void*)copy.name);
        return err;
    }
    copy.type_module = imp->type_module;
    copy.type_index = imp->type_index;
    copy.has_type_context = imp->has_type_context;
    copy.identity = imp->identity ? imp->identity : wasm__alloc_tag_identity(rt);

    rt->tag_imports[rt->num_tag_imports++] = copy;
    return WASM_OK;
}
