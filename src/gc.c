#include "internal.h"

WASM_PRIVATE void wasm__gc_mark_value(const wasm_value_t* value) {
    if (!wasm__is_heap_gc_ref_value(value)) return;
    wasm__gc_mark_object((wasm_gc_header_t*)value->of.gc_obj);
}

WASM_PRIVATE void wasm__gc_mark_values(const wasm_value_t* values, uint32_t count) {
    uint32_t i;

    if (!values) return;
    for (i = 0; i < count; i++) wasm__gc_mark_value(&values[i]);
}

WASM_PRIVATE void wasm__gc_mark_struct_fields(wasm_gc_header_t* object,
                                        const wasm_structtype_t* type) {
    uint32_t i;

    for (i = 0; i < type->num_fields; i++) {
        size_t offset;

        if (type->fields[i].storage.kind == WASM_STORAGE_PACKED) continue;
        offset = wasm__gc_struct_field_offset(type, i);
        if (offset == (size_t)-1) continue;
        wasm__gc_mark_value((const wasm_value_t*)(((const uint8_t*)object) + offset));
    }
}

WASM_PRIVATE void wasm__gc_mark_array_elements(wasm_gc_header_t* object,
                                         const wasm_arraytype_t* type) {
    wasm_gc_array_t* array_object;
    size_t data_offset;
    wasm_value_t* elems;
    uint32_t i;

    if (type->field.storage.kind == WASM_STORAGE_PACKED) return;
    data_offset = wasm__gc_array_data_offset(type);
    if (data_offset == (size_t)-1) return;

    array_object = (wasm_gc_array_t*)object;
    elems = (wasm_value_t*)(((uint8_t*)array_object) + data_offset);
    for (i = 0; i < array_object->length; i++) wasm__gc_mark_value(&elems[i]);
}

WASM_PRIVATE void wasm__gc_mark_object(wasm_gc_header_t* object) {
    const wasm_module_t* mod;

    if (!object || object->marked) return;
    object->marked = 1;

    mod = object->module;
    if (!mod || object->type_index >= mod->num_types) return;

    switch (object->kind) {
        case WASM_GC_OBJ_STRUCT: {
            const wasm_structtype_t* type = wasm__module_const_structtype(mod, object->type_index);

            if (type) wasm__gc_mark_struct_fields(object, type);
            break;
        }
        case WASM_GC_OBJ_ARRAY: {
            const wasm_arraytype_t* type = wasm__module_const_arraytype(mod, object->type_index);

            if (type) wasm__gc_mark_array_elements(object, type);
            break;
        }
        default:
            break;
    }
}

WASM_PRIVATE void wasm__gc_mark_runtime_roots(wasm_runtime_t* rt) {
    wasm_module_t* mod;
    wasm__exception_object_t* exception_object;
    wasm__externref_box_t* box;
    uint32_t i;

    if (!rt) return;

    wasm__gc_mark_values(rt->stack, rt->sp);
    wasm__gc_mark_values(rt->pending_exception.values, rt->pending_exception.num_values);
    for (i = 0; i < rt->num_global_imports; i++) {
        if (rt->global_imports[i].value) wasm__gc_mark_value(rt->global_imports[i].value);
    }
    for (box = rt->externref_boxes; box; box = box->next) {
        wasm__gc_mark_value(&box->value);
    }
    for (exception_object = rt->exception_objects; exception_object; exception_object = exception_object->next) {
        wasm__gc_mark_values(exception_object->values, exception_object->num_values);
    }
    for (i = 0; i < rt->call_frame_sp; i++) {
        wasm__call_frame_t* cf = &rt->call_frames[i];
        uint32_t j;

        if (cf->func && cf->locals) wasm__gc_mark_values(cf->locals, cf->func->num_locals);
        for (j = 0; j < cf->lsp; j++) {
            wasm__gc_mark_values(cf->labels[j].caught_values, cf->labels[j].caught_count);
        }
    }
    for (mod = rt->gc_modules; mod; mod = mod->gc_next) {
        for (i = 0; i < mod->num_globals; i++) {
            if (mod->globals[i].is_import && mod->globals[i].import_value)
                wasm__gc_mark_value(mod->globals[i].import_value);
            else
                wasm__gc_mark_value(&mod->globals[i].value);
        }
        for (i = 0; i < mod->num_tables; i++) {
            wasm_table_t* table = wasm__table_at(mod, i);
            if (table && table->elems) wasm__gc_mark_values(table->elems, table->size);
        }
        for (i = 0; i < mod->num_elem_segments; i++) {
            wasm_elem_segment_t* segment = &mod->elem_segments[i];

            if (!segment->is_passive || segment->is_declarative || segment->is_dropped) continue;
            wasm__gc_mark_values(segment->elems, segment->num_elems);
        }
    }
}

WASM_PRIVATE void wasm__gc_forward_value(wasm_runtime_t* rt, wasm_value_t* value) {
    wasm_gc_header_t* old_object;

    if (!wasm__is_heap_gc_ref_value(value)) return;
    old_object = value->of.gc_obj;
    if (!wasm__gc_ptr_in_heap(rt, old_object) || !old_object->next_alloc) return;

    value->of.gc_obj = old_object->next_alloc;
    value->of.gc_ref = (uintptr_t)old_object->next_alloc;
}

WASM_PRIVATE void wasm__gc_forward_values(wasm_runtime_t* rt, wasm_value_t* values, uint32_t count) {
    uint32_t i;

    if (!values) return;
    for (i = 0; i < count; i++) wasm__gc_forward_value(rt, &values[i]);
}

WASM_PRIVATE void wasm__gc_forward_object_refs(wasm_runtime_t* rt, wasm_gc_header_t* object) {
    const wasm_module_t* mod;

    if (!object) return;
    mod = object->module;
    if (!mod || object->type_index >= mod->num_types) return;

    switch (object->kind) {
        case WASM_GC_OBJ_STRUCT: {
            const wasm_structtype_t* type = wasm__module_const_structtype(mod, object->type_index);
            uint32_t i;

            if (!type) return;
            for (i = 0; i < type->num_fields; i++) {
                size_t offset;

                if (type->fields[i].storage.kind == WASM_STORAGE_PACKED) continue;
                offset = wasm__gc_struct_field_offset(type, i);
                if (offset == (size_t)-1) continue;
                wasm__gc_forward_value(rt, (wasm_value_t*)(((uint8_t*)object) + offset));
            }
            break;
        }
        case WASM_GC_OBJ_ARRAY: {
            const wasm_arraytype_t* type = wasm__module_const_arraytype(mod, object->type_index);
            size_t data_offset;

            if (!type || type->field.storage.kind == WASM_STORAGE_PACKED) return;
            data_offset = wasm__gc_array_data_offset(type);
            if (data_offset == (size_t)-1) return;
            wasm__gc_forward_values(rt,
                                    (wasm_value_t*)(((uint8_t*)object) + data_offset),
                                    ((wasm_gc_array_t*)object)->length);
            break;
        }
        default:
            break;
    }
}

WASM_PRIVATE void wasm__gc_forward_runtime_roots(wasm_runtime_t* rt) {
    wasm_module_t* mod;
    wasm__exception_object_t* exception_object;
    wasm__externref_box_t* box;
    uint32_t i;

    if (!rt) return;

    wasm__gc_forward_values(rt, rt->stack, rt->sp);
    wasm__gc_forward_values(rt, rt->pending_exception.values, rt->pending_exception.num_values);
    for (i = 0; i < rt->num_global_imports; i++) {
        if (rt->global_imports[i].value) wasm__gc_forward_value(rt, rt->global_imports[i].value);
    }
    for (box = rt->externref_boxes; box; box = box->next) {
        wasm__gc_forward_value(rt, &box->value);
    }
    for (exception_object = rt->exception_objects; exception_object; exception_object = exception_object->next) {
        wasm__gc_forward_values(rt, exception_object->values, exception_object->num_values);
    }
    for (i = 0; i < rt->call_frame_sp; i++) {
        wasm__call_frame_t* cf = &rt->call_frames[i];
        uint32_t j;

        if (cf->func && cf->locals) wasm__gc_forward_values(rt, cf->locals, cf->func->num_locals);
        for (j = 0; j < cf->lsp; j++) {
            wasm__gc_forward_values(rt, cf->labels[j].caught_values, cf->labels[j].caught_count);
        }
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
            if (table && table->elems)
                wasm__gc_forward_values(rt, table->elems, table->size);
        }
        for (i = 0; i < mod->num_elem_segments; i++) {
            wasm_elem_segment_t* segment = &mod->elem_segments[i];

            if (!segment->is_passive || segment->is_declarative || segment->is_dropped) continue;
            wasm__gc_forward_values(rt, segment->elems, segment->num_elems);
        }
    }
}

void wasm_gc_collect(wasm_runtime_t* rt) {
    wasm_gc_header_t* object;
    wasm_gc_header_t* new_allocations = NULL;
    wasm_gc_header_t* new_tail = NULL;
    uint8_t* new_heap = NULL;
    size_t new_offset = 0;
    int has_live = 0;

    if (!rt || !rt->gc_heap) return;

    for (object = rt->gc_allocations; object; object = object->next_alloc) object->marked = 0;

    wasm__gc_mark_runtime_roots(rt);
    for (object = rt->gc_allocations; object; object = object->next_alloc) {
        if (object->marked) {
            has_live = 1;
            break;
        }
    }

    if (!has_live) {
        rt->gc_allocations = NULL;
        rt->gc_heap_offset = 0;
        return;
    }

    new_heap = (uint8_t*)WASM_GC_ALLOC(rt->gc_heap_size);
    if (!new_heap) return;

    for (object = rt->gc_allocations; object;) {
        wasm_gc_header_t* next = object->next_alloc;

        if (object->marked) {
            size_t aligned_offset = wasm__align_up_size(new_offset, (size_t)WASM__ALIGNOF(wasm_value_t));
            wasm_gc_header_t* moved;

            if (aligned_offset == (size_t)-1 || aligned_offset > rt->gc_heap_size ||
                object->object_size > rt->gc_heap_size - aligned_offset) {
                WASM_GC_FREE(new_heap);
                return;
            }

            moved = (wasm_gc_header_t*)(new_heap + aligned_offset);
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

    WASM_GC_FREE(rt->gc_heap);
    rt->gc_heap = new_heap;
    rt->gc_heap_offset = new_offset;
    rt->gc_allocations = new_allocations;
}
