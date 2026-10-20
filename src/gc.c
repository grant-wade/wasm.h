#include "gc.h"
#include "wasm.h"
#include "handles.h"
#include "exec/cont.h"
#include "exec/frames.h"
#include "exec/stack.h"
#include "exec/state.h"
#include "values.h"
#include "support.h"
#include "runtime.h"
#include "types/canonical.h"
#include "memory_internal.h"
#include "module.h"
#include "internal/gc_types.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>

/* Objects marked but not yet scanned; explicit stacks keep deep object graphs off the C stack. */
typedef struct wasm__gc_mark_stack_t {
    wasm_runtime_t* rt;
    wasm_gc_header_t** items;
    size_t count;
    size_t capacity;
    wasm__exception_object_t** exceptions;
    size_t exception_count;
    size_t exception_capacity;
    wasm__cont_object_t** conts;
    size_t cont_count;
    size_t cont_capacity;
    int failed;
} wasm__gc_mark_stack_t;

/* Doubles a full mark stack of pointers; returns NULL and records the failure when out of memory. */
static void* wasm__gc_grow(wasm__gc_mark_stack_t* stack, void* items, size_t* capacity) {
    size_t new_capacity = *capacity ? *capacity * 2u : 64u;
    void* grown = wasm__realloc_array(&stack->rt->allocator, items, new_capacity, sizeof(void*));

    if (!grown) {
        stack->failed = 1;
        return NULL;
    }
    *capacity = new_capacity;
    return grown;
}

static void wasm__gc_mark_value(wasm__gc_mark_stack_t* stack, const wasm_value_t* value);

static void wasm__gc_mark_box(wasm__gc_mark_stack_t* stack, uintptr_t handle) {
    while (handle && wasm__handle_set_contains(&stack->rt->externref_boxes, (const void*)handle)) {
        wasm__externref_box_t* box = (wasm__externref_box_t*)handle;

        if (box->marked) return;
        box->marked = 1;
        if (!wasm__uses_externref_storage(box->value.type)) {
            wasm__gc_mark_value(stack, &box->value);
            return;
        }
        handle = box->value.of.externref;
    }
}

static void wasm__gc_mark_handle(wasm__gc_mark_stack_t* stack, const wasm_value_t* value) {
    void* handle = (void*)value->of.externref;

    if (!handle) return;
    if (wasm__uses_externref_storage(value->type)) {
        wasm__gc_mark_box(stack, value->of.externref);
    } else if ((value->of.externref & 1u) != 0) {
        wasm__cont_object_t* k = wasm__cont_mark(stack->rt, value->of.externref);

        if (!k) return;
        if (stack->cont_count == stack->cont_capacity) {
            wasm__cont_object_t** conts =
                (wasm__cont_object_t**)wasm__gc_grow(stack, stack->conts, &stack->cont_capacity);

            if (!conts) return;
            stack->conts = conts;
        }
        stack->conts[stack->cont_count++] = k;
    } else {
        wasm__exception_object_t* exception;

        if (!wasm__handle_set_contains(&stack->rt->exception_objects, handle)) return;
        exception = (wasm__exception_object_t*)handle;
        if (exception->marked) return;
        if (stack->exception_count == stack->exception_capacity) {
            wasm__exception_object_t** exceptions = (wasm__exception_object_t**)wasm__gc_grow(
                stack, stack->exceptions, &stack->exception_capacity);

            if (!exceptions) return;
            stack->exceptions = exceptions;
        }
        exception->marked = 1;
        stack->exceptions[stack->exception_count++] = exception;
    }
}

static void wasm__gc_mark_value(wasm__gc_mark_stack_t* stack, const wasm_value_t* value) {
    wasm_gc_header_t* object;

    if (!value) return;
    if (wasm__uses_externref_storage(value->type) || wasm__uses_exnref_storage(value->type) ||
        wasm__uses_contref_storage(value->type)) {
        wasm__gc_mark_handle(stack, value);
        return;
    }
    if (!wasm__is_heap_gc_ref_value(value)) return;
    object = (wasm_gc_header_t*)value->of.gc_obj;
    if (!wasm__gc_ptr_in_heap(stack->rt, object)) {
        wasm__gc_mark_box(stack, value->of.gc_ref);
        return;
    }
    if (object->marked) return;
    if (stack->count == stack->capacity) {
        wasm_gc_header_t** items = (wasm_gc_header_t**)wasm__gc_grow(stack, stack->items, &stack->capacity);

        if (!items) return;
        stack->items = items;
    }
    object->marked = 1;
    stack->items[stack->count++] = object;
}

static void wasm__gc_mark_values(wasm__gc_mark_stack_t* stack, const wasm_value_t* values, uint32_t count) {
    uint32_t i;

    if (!values) return;
    for (i = 0; i < count; i++) wasm__gc_mark_value(stack, &values[i]);
}

static void wasm__gc_mark_descriptor(wasm__gc_mark_stack_t* stack, wasm_gc_header_t* object) {
    wasm_gc_header_t** slot = wasm__gc_descriptor_slot(object);
    wasm_value_t value;

    if (!slot || !*slot) return;
    memset(&value, 0, sizeof(value));
    value.type = WASM_TYPE_STRUCTREF;
    value.of.gc_obj = *slot;
    wasm__gc_mark_value(stack, &value);
}

static void wasm__gc_scan_object(wasm__gc_mark_stack_t* stack, wasm_gc_header_t* object) {
    const wasm_module_t* mod = object->module;
    const wasm_comptype_t* type;
    uint32_t i;

    if (object->kind == WASM_GC_OBJ_STRUCT) wasm__gc_mark_descriptor(stack, object);
    if (!mod || object->type_index >= mod->num_types) return;
    type = &mod->types[object->type_index];
    if (object->kind == WASM_GC_OBJ_STRUCT && type->kind == WASM_COMP_STRUCT) {
        const wasm_structtype_t* st = &type->of.struct_;

        for (i = 0; i < st->num_ref_fields; i++)
            wasm__gc_mark_value(stack, (const wasm_value_t*)((const uint8_t*)object + st->ref_offsets[i]));
    } else if (object->kind == WASM_GC_OBJ_ARRAY && type->kind == WASM_COMP_ARRAY && type->of.array.elems_hold_refs) {
        wasm__gc_mark_values(stack, (const wasm_value_t*)((const uint8_t*)object + type->of.array.data_offset),
                             ((const wasm_gc_array_t*)object)->length);
    }
}

static void wasm__gc_forward_value(wasm_runtime_t* rt, wasm_value_t* value);

static void wasm__gc_forward_values(wasm_runtime_t* rt, wasm_value_t* values, uint32_t count);

/* Marks the reference of kind in s if stack is not NULL, else forwards it. */
static void wasm__gc_frame_slot(wasm__gc_mark_stack_t* stack, wasm_runtime_t* rt, wasm__slot_t* s, uint32_t kind) {
    wasm_value_t value;

    if (kind == WASM__SLOT_KIND_NONE || wasm__slot_u64(s) == 0) return;
    memset(&value, 0, sizeof(value));
    value.type = kind == WASM__SLOT_KIND_EXTERN ? WASM_TYPE_EXTERNREF
                 : kind == WASM__SLOT_KIND_HANDLE ? WASM_TYPE_EXNREF
                                                : WASM_TYPE_ANYREF;
    value.of.gc_ref = wasm__slot_ref(s);
    if (stack) {
        wasm__gc_mark_value(stack, &value);
    } else {
        wasm__gc_forward_value(rt, &value);
        wasm__slot_set_ref(s, value.of.gc_ref);
    }
}

/* Marks the references in s's operand slots, frame locals, and caught exceptions if stack is not
 * NULL, else forwards them. */
static void wasm__gc_stack_slots(wasm__gc_mark_stack_t* stack, wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    const wasm_functype_t* start_type = wasm__stack_start_type(s);
    uint32_t i;

    for (i = 0; start_type && i < start_type->num_params; i++) {
        uint32_t slot = wasm__valtypes_slots(start_type->params, i);

        if (slot >= s->sp) break;
        wasm__gc_frame_slot(stack, rt, &s->stack[slot], wasm__slot_kind(start_type->params[i]));
    }
    for (i = 0; i < s->call_frame_sp; i++) {
        wasm__call_frame_t* cf = &s->call_frames[i];
        uint32_t end = i + 1u < s->call_frame_sp ? s->call_frames[i + 1u].sp_base : s->sp;
        uint32_t map_slots;
        const uint32_t* map;
        uint32_t j;

        for (j = 0; j < cf->lsp; j++) {
            if (stack)
                wasm__gc_mark_values(stack, cf->labels[j].caught_values, cf->labels[j].caught_count);
            else
                wasm__gc_forward_values(rt, cf->labels[j].caught_values, cf->labels[j].caught_count);
        }
        if (!cf->func || !cf->func->ops) continue;
        map = wasm__frame_stack_map(s, i, &map_slots);
        for (j = 0; map && j < map_slots && cf->sp_base + j < end; j++)
            wasm__gc_frame_slot(stack, rt, &s->stack[cf->sp_base + j], wasm__stack_map_kind(map, j));
        for (j = 0; cf->locals && j < cf->func->num_locals; j++)
            wasm__gc_frame_slot(stack, rt, &cf->locals[cf->func->local_slots[j]],
                                wasm__slot_kind(cf->func->locals[j]));
    }
}

/* Marks the references among k's bound arguments if stack is not NULL, else forwards them. */
static void wasm__gc_cont_args(wasm__gc_mark_stack_t* stack, wasm_runtime_t* rt, wasm__cont_object_t* k) {
    const wasm_functype_t* start_type = wasm__cont_start_type(k);
    uint32_t i;

    for (i = 0; start_type && i < start_type->num_params; i++) {
        uint32_t slot = wasm__valtypes_slots(start_type->params, i);

        if (slot >= k->num_args) break;
        wasm__gc_frame_slot(stack, rt, &k->args[slot], wasm__slot_kind(start_type->params[i]));
    }
}

static void wasm__gc_drain_mark_stack(wasm__gc_mark_stack_t* stack) {
    while (!stack->failed) {
        if (stack->count > 0u) {
            wasm__gc_scan_object(stack, stack->items[--stack->count]);
        } else if (stack->exception_count > 0u) {
            wasm__exception_object_t* exception = stack->exceptions[--stack->exception_count];

            wasm__gc_mark_values(stack, exception->values, exception->num_values);
        } else if (stack->cont_count > 0u) {
            wasm__cont_object_t* k = stack->conts[--stack->cont_count];
            wasm__exec_stack_t* s;

            for (s = k->top; s; s = s == k->bottom ? NULL : s->parent) wasm__gc_stack_slots(stack, stack->rt, s);
            wasm__gc_cont_args(stack, stack->rt, k);
        } else {
            break;
        }
    }
}

static void wasm__gc_mark_runtime_roots(wasm__gc_mark_stack_t* stack, wasm_runtime_t* rt) {
    wasm__exec_stack_t* s;
    wasm_module_t* mod;
    wasm__held_block_t* held;
    wasm__gc_root_t* root;
    uint32_t i;

    if (!rt) return;

    for (s = rt->exec; s; s = s->parent) wasm__gc_stack_slots(stack, rt, s);
    for (root = rt->gc_roots; root; root = root->prev) wasm__gc_mark_values(stack, *root->values, *root->count);
    wasm__gc_mark_values(stack, rt->pending_exception.values, rt->pending_exception.num_values);
    for (i = 0; i < rt->num_global_imports; i++) {
        if (rt->global_imports[i].value) wasm__gc_mark_value(stack, rt->global_imports[i].value);
    }
    for (held = rt->held_values; held; held = held->next) {
        for (i = 0; i < WASM__HELD_BLOCK_SLOTS; i++) {
            if (held->slots[i].in_use) wasm__gc_mark_value(stack, &held->slots[i].value);
        }
    }
    for (mod = rt->gc_modules; mod; mod = mod->gc_next) {
        for (i = 0; i < mod->num_globals; i++) {
            if (mod->globals[i].is_import && mod->globals[i].import_value)
                wasm__gc_mark_value(stack, mod->globals[i].import_value);
            else
                wasm__gc_mark_value(stack, &mod->globals[i].value);
        }
        for (i = 0; i < mod->num_tables; i++) {
            wasm_table_t* table = wasm__table_at(mod, i);

            if (!table) continue;
            wasm__gc_mark_value(stack, &table->default_value);
            if (table->elems) wasm__gc_mark_values(stack, table->elems, table->size);
        }
        for (i = 0; i < mod->num_elem_segments; i++) {
            wasm_elem_segment_t* segment = &mod->elem_segments[i];

            if (segment->is_declarative || segment->is_dropped) continue;
            wasm__gc_mark_values(stack, segment->elems, segment->num_elems);
        }
    }
}

static void wasm__gc_forward_value(wasm_runtime_t* rt, wasm_value_t* value) {
    wasm_gc_header_t* old_object;

    if (!wasm__is_heap_gc_ref_value(value)) return;
    old_object = value->of.gc_obj;
    if (!wasm__gc_ptr_in_heap(rt, old_object) || !old_object->next_alloc) return;

    value->of.gc_obj = old_object->next_alloc;
    value->of.gc_ref = (uintptr_t)old_object->next_alloc;
}

static void wasm__gc_forward_values(wasm_runtime_t* rt, wasm_value_t* values, uint32_t count) {
    uint32_t i;

    if (!values) return;
    for (i = 0; i < count; i++) wasm__gc_forward_value(rt, &values[i]);
}

static void wasm__gc_forward_root(wasm_runtime_t* rt, const wasm__gc_root_t* root) {
    wasm_value_t* values = *root->values;
    uint32_t i;

    if (!root->mirror) {
        wasm__gc_forward_values(rt, values, *root->count);
        return;
    }
    for (i = 0; i < *root->count; i++) {
        uintptr_t bits = values[i].of.gc_ref;

        wasm__gc_forward_value(rt, &values[i]);
        if (values[i].of.gc_ref != bits) root->mirror[i] = values[i];
    }
}

static void wasm__gc_forward_object_refs(wasm_runtime_t* rt, wasm_gc_header_t* object) {
    const wasm_module_t* mod = object->module;
    const wasm_comptype_t* type;
    uint32_t i;

    if (object->kind == WASM_GC_OBJ_STRUCT) {
        wasm_gc_header_t** slot = wasm__gc_descriptor_slot(object);

        if (slot && wasm__gc_ptr_in_heap(rt, *slot) && (*slot)->next_alloc) *slot = (*slot)->next_alloc;
    }
    if (!mod || object->type_index >= mod->num_types) return;
    type = &mod->types[object->type_index];
    if (object->kind == WASM_GC_OBJ_STRUCT && type->kind == WASM_COMP_STRUCT) {
        const wasm_structtype_t* st = &type->of.struct_;

        for (i = 0; i < st->num_ref_fields; i++)
            wasm__gc_forward_value(rt, (wasm_value_t*)((uint8_t*)object + st->ref_offsets[i]));
    } else if (object->kind == WASM_GC_OBJ_ARRAY && type->kind == WASM_COMP_ARRAY && type->of.array.elems_hold_refs) {
        wasm__gc_forward_values(rt, (wasm_value_t*)((uint8_t*)object + type->of.array.data_offset),
                                ((wasm_gc_array_t*)object)->length);
    }
}

static void wasm__gc_forward_runtime_roots(wasm_runtime_t* rt) {
    wasm__exec_stack_t* s;
    wasm_module_t* mod;
    wasm__held_block_t* held;
    wasm__gc_root_t* root;
    uint32_t i;

    if (!rt) return;

    for (s = wasm__next_live_stack(rt, NULL); s; s = wasm__next_live_stack(rt, s)) wasm__gc_stack_slots(NULL, rt, s);
    for (i = 0; i < rt->conts.count; i++) {
        wasm__cont_object_t* k = wasm__cont_at(&rt->conts, i);

        if (k->in_use) wasm__gc_cont_args(NULL, rt, k);
    }
    for (root = rt->gc_roots; root; root = root->prev) wasm__gc_forward_root(rt, root);
    wasm__gc_forward_values(rt, rt->pending_exception.values, rt->pending_exception.num_values);
    for (i = 0; i < rt->num_global_imports; i++) {
        if (rt->global_imports[i].value) wasm__gc_forward_value(rt, rt->global_imports[i].value);
    }
    for (i = 0; i < rt->externref_boxes.capacity; i++) {
        wasm__externref_box_t* box = (wasm__externref_box_t*)rt->externref_boxes.slots[i];

        if (box) wasm__gc_forward_value(rt, &box->value);
    }
    for (held = rt->held_values; held; held = held->next) {
        for (i = 0; i < WASM__HELD_BLOCK_SLOTS; i++) {
            if (held->slots[i].in_use) wasm__gc_forward_value(rt, &held->slots[i].value);
        }
    }
    for (i = 0; i < rt->exception_objects.capacity; i++) {
        wasm__exception_object_t* exception = (wasm__exception_object_t*)rt->exception_objects.slots[i];

        if (exception) wasm__gc_forward_values(rt, exception->values, exception->num_values);
    }
    for (mod = rt->gc_modules; mod; mod = mod->gc_next) {
        for (i = 0; i < mod->num_globals; i++) {
            if (mod->globals[i].is_import && mod->globals[i].import_value)
                wasm__gc_forward_value(rt, mod->globals[i].import_value);
            else
                wasm__gc_forward_value(rt, &mod->globals[i].value);
        }
        for (i = 0; i < mod->num_tables; i++) {
            wasm_table_t* table = wasm__table_at(mod, i);

            if (!table) continue;
            wasm__gc_forward_value(rt, &table->default_value);
            if (table->elems) wasm__gc_forward_values(rt, table->elems, table->size);
        }
        for (i = 0; i < mod->num_elem_segments; i++) {
            wasm_elem_segment_t* segment = &mod->elem_segments[i];

            if (segment->is_declarative || segment->is_dropped) continue;
            wasm__gc_forward_values(rt, segment->elems, segment->num_elems);
        }
    }
}

static int wasm__gc_box_is_marked(void* handle) {
    return ((wasm__externref_box_t*)handle)->marked;
}

static int wasm__gc_exception_is_marked(void* handle) {
    return ((wasm__exception_object_t*)handle)->marked;
}

static void wasm__gc_free_box(const wasm_allocator_t* a, void* handle) {
    wasm__free(a, handle);
}

static void wasm__gc_free_exception(const wasm_allocator_t* a, void* handle) {
    wasm__exception_object_t* exception = (wasm__exception_object_t*)handle;

    wasm__free(a, exception->values);
    wasm__free(a, exception);
}

static void wasm__gc_clear_handle_marks(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->externref_boxes.capacity; i++) {
        wasm__externref_box_t* box = (wasm__externref_box_t*)rt->externref_boxes.slots[i];

        if (box) box->marked = 0;
    }
    for (i = 0; i < rt->exception_objects.capacity; i++) {
        wasm__exception_object_t* exception = (wasm__exception_object_t*)rt->exception_objects.slots[i];

        if (exception) exception->marked = 0;
    }
    wasm__cont_clear_marks(rt);
}

static int wasm__gc_live_bytes(const wasm_runtime_t* rt, size_t* out_live) {
    const wasm_gc_header_t* object;
    size_t live = 0;

    for (object = rt->gc_allocations; object; object = object->next_alloc) {
        if (!object->marked) continue;
        live = wasm__align_up_size(live, (size_t)WASM__ALIGNOF(wasm_value_t));
        if (live == (size_t)-1 || object->object_size > SIZE_MAX - live) return 0;
        live += object->object_size;
    }
    *out_live = live;
    return 1;
}

static void wasm__gc_free_type_owner(wasm_runtime_t* rt, wasm_module_t* owner) {
    uint32_t i;

    wasm__release_type_groups(rt, owner->rec_groups, owner->num_rec_groups);
    for (i = 0; i < owner->num_types; i++) wasm__free_comptype(&rt->allocator, &owner->types[i]);
    wasm__free(&rt->allocator, owner->types);
    wasm__free(&rt->allocator, owner->rec_groups);
    wasm__free(&rt->allocator, owner);
}

/* Every GC object and continuation of rt must be live. */
static void wasm__gc_free_unused_types(wasm_runtime_t* rt) {
    wasm_module_t** link;
    wasm_gc_header_t* object;
    uint32_t i;

    for (link = &rt->type_owners; *link; link = &(*link)->gc_next) (*link)->retired_types_used = 0;
    for (object = rt->gc_allocations; object; object = object->next_alloc) {
        if (object->module && object->module->retired_types) object->module->retired_types_used = 1;
    }
    for (i = 0; i < rt->conts.count; i++) {
        wasm__cont_object_t* k = wasm__cont_at(&rt->conts, i);

        if (k->in_use && k->module && k->module->retired_types) k->module->retired_types_used = 1;
    }
    link = &rt->type_owners;
    while (*link) {
        wasm_module_t* owner = *link;

        if (owner->retired_types_used) {
            link = &owner->gc_next;
            continue;
        }
        *link = owner->gc_next;
        wasm__gc_free_type_owner(rt, owner);
    }
}

WASM_PRIVATE wasm_error_t wasm__gc_collect(wasm_runtime_t* rt, size_t request) {
    wasm_gc_header_t* object;
    wasm_gc_header_t* new_allocations = NULL;
    wasm_gc_header_t* new_tail = NULL;
    uint8_t* new_heap;
    size_t new_offset = 0;
    size_t live;
    size_t needed;
    size_t new_size;
    wasm__gc_mark_stack_t stack;

    if (!rt) return WASM_ERR_MALFORMED;
#ifndef NDEBUG
    wasm__check_stack_maps(rt);
#endif

    for (object = rt->gc_allocations; object; object = object->next_alloc) object->marked = 0;
    wasm__gc_clear_handle_marks(rt);

    memset(&stack, 0, sizeof(stack));
    stack.rt = rt;
    wasm__gc_mark_runtime_roots(&stack, rt);
    wasm__gc_drain_mark_stack(&stack);
    wasm__free(&rt->allocator, stack.items);
    wasm__free(&rt->allocator, stack.exceptions);
    wasm__free(&rt->allocator, stack.conts);
    if (stack.failed) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory marking live GC objects");
        return WASM_ERR_OOM;
    }

    wasm__handle_set_filter(&rt->allocator, &rt->externref_boxes, wasm__gc_box_is_marked, wasm__gc_free_box);
    wasm__handle_set_filter(&rt->allocator, &rt->exception_objects, wasm__gc_exception_is_marked, wasm__gc_free_exception);
    wasm__cont_sweep(rt);

    if (!wasm__gc_live_bytes(rt, &live)) live = SIZE_MAX;
    needed = wasm__align_up_size(live, (size_t)WASM__ALIGNOF(wasm_value_t));
    needed = needed == (size_t)-1 || request > SIZE_MAX - needed ? SIZE_MAX : needed + request;
    if (needed == SIZE_MAX) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "GC heap cannot fit %zu more bytes", request);
        return WASM_ERR_OOM;
    }
    new_size = live > SIZE_MAX - needed ? needed : needed + live;
    if (new_size < rt->gc_heap_min_size) new_size = rt->gc_heap_min_size;
    if (rt->max_gc_heap_bytes != 0 && new_size > rt->max_gc_heap_bytes) new_size = (size_t)rt->max_gc_heap_bytes;
    if (needed > new_size) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "GC heap would exceed its limit of %llu bytes",
                      (unsigned long long)rt->max_gc_heap_bytes);
        return WASM_ERR_OOM;
    }

    if (live == 0 && new_size <= rt->gc_heap_size) {
        new_heap = new_size > 0 && new_size < rt->gc_heap_size
                       ? (uint8_t*)wasm__alloc_sized(&rt->allocator, new_size, WASM__BLOCK_ALIGN)
                       : NULL;
        if (new_heap || new_size == 0) {
            wasm__free_sized(&rt->allocator, rt->gc_heap, rt->gc_heap_size, WASM__BLOCK_ALIGN);
            rt->gc_heap = new_heap;
            rt->gc_heap_size = new_size;
        }
        rt->gc_allocations = NULL;
        rt->gc_heap_offset = 0;
        if (rt->type_owners) wasm__gc_free_unused_types(rt);
        return WASM_OK;
    }

    new_heap = (uint8_t*)wasm__alloc_sized(&rt->allocator, new_size, WASM__BLOCK_ALIGN);
    if (!new_heap) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "failed to allocate a %zu-byte GC heap", new_size);
        return WASM_ERR_OOM;
    }

    for (object = rt->gc_allocations; object;) {
        wasm_gc_header_t* next = object->next_alloc;

        if (object->marked) {
            size_t aligned_offset = wasm__align_up_size(new_offset, (size_t)WASM__ALIGNOF(wasm_value_t));
            wasm_gc_header_t* moved = (wasm_gc_header_t*)(new_heap + aligned_offset);

            memcpy(moved, object, object->object_size);
            moved->marked = 0;
            moved->next_alloc = NULL;

            if (!new_allocations)
                new_allocations = moved;
            else
                new_tail->next_alloc = moved;
            new_tail = moved;

            object->next_alloc = moved;
            new_offset = aligned_offset + object->object_size;
        }

        object = next;
    }

    wasm__gc_forward_runtime_roots(rt);
    for (object = new_allocations; object; object = object->next_alloc) {
        wasm__gc_forward_object_refs(rt, object);
    }

    wasm__free_sized(&rt->allocator, rt->gc_heap, rt->gc_heap_size, WASM__BLOCK_ALIGN);
    rt->gc_heap = new_heap;
    rt->gc_heap_size = new_size;
    rt->gc_heap_offset = new_offset;
    rt->gc_allocations = new_allocations;
    if (rt->type_owners) wasm__gc_free_unused_types(rt);
    return WASM_OK;
}

WASM_PRIVATE int wasm__gc_types_used(const wasm_runtime_t* rt, const wasm_module_t* mod) {
    const wasm_gc_header_t* object;
    uint32_t i;

    for (object = rt->gc_allocations; object; object = object->next_alloc) {
        if (object->module == mod) return 1;
    }
    for (i = 0; i < rt->conts.count; i++) {
        const wasm__cont_object_t* k = wasm__cont_at(&rt->conts, i);

        if (k->in_use && k->module == mod) return 1;
    }
    return 0;
}

WASM_PRIVATE void wasm__gc_retire_types(wasm_runtime_t* rt, wasm_module_t* mod) {
    wasm_module_t owner;

    memset(&owner, 0, sizeof(owner));
    owner.rt = rt;
    owner.identity = mod->identity;
    owner.types = mod->types;
    owner.num_types = mod->num_types;
    owner.rec_groups = mod->rec_groups;
    owner.num_rec_groups = mod->num_rec_groups;
    owner.retired_types = 1;
    owner.gc_next = rt->type_owners;
    *mod = owner;
    rt->type_owners = mod;
}

WASM_PRIVATE void wasm__gc_free_retired_types(wasm_runtime_t* rt) {
    while (rt->type_owners) {
        wasm_module_t* owner = rt->type_owners;

        rt->type_owners = owner->gc_next;
        wasm__gc_free_type_owner(rt, owner);
    }
}

void wasm_gc_collect(wasm_runtime_t* rt) {
    (void)wasm__gc_collect(rt, 0);
}

wasm_value_t* wasm_ref_hold(wasm_runtime_t* rt, wasm_value_t value) {
    wasm__held_block_t* block;
    uint32_t i;

    if (!rt) return NULL;
    for (block = rt->held_values; block; block = block->next) {
        for (i = 0; i < WASM__HELD_BLOCK_SLOTS; i++) {
            if (!block->slots[i].in_use) goto found;
        }
    }
    block = (wasm__held_block_t*)wasm__calloc(&rt->allocator, 1u, sizeof(*block));
    if (!block) return NULL;
    block->next = rt->held_values;
    rt->held_values = block;
    i = 0;
found:
    block->slots[i].value = value;
    block->slots[i].in_use = 1;
    return &block->slots[i].value;
}

void wasm_ref_release(wasm_runtime_t* rt, wasm_value_t* held) {
    wasm__held_block_t* block;
    uint32_t i;

    if (!rt || !held) return;
    for (block = rt->held_values; block; block = block->next) {
        for (i = 0; i < WASM__HELD_BLOCK_SLOTS; i++) {
            if (&block->slots[i].value != held) continue;
            block->slots[i].in_use = 0;
            block->slots[i].value = wasm_i32(0);
            return;
        }
    }
}
