#include "support.h"

#ifdef WASM_TEST_MODULAR
#include "../../src/exec/state.h"
#include "../../src/types/canonical.h"
#include "../../src/values.h"
#endif

static uint32_t wasm_test_gc_allocation_count(const wasm_runtime_t* rt) {
    uint32_t count = 0;
    const wasm_gc_header_t* object;

    if (!rt) return 0;
    for (object = rt->gc_allocations; object; object = object->next_alloc) count++;
    return count;
}

/* Loads the field into out; returns out, or NULL if there is no such field. */
static wasm_value_t* wasm_test_gc_struct_field_value(wasm_gc_struct_t* object,
                                                     const wasm_structtype_t* type,
                                                     uint32_t field_index,
                                                     wasm_value_t* out) {
    if (!object || wasm__gc_struct_get_field(object, type, field_index, 0, out) != WASM_OK) return NULL;
    return out;
}

TEST(test_gc_runtime_initializes_heap_arena) {
    wasm_runtime_t rt;

    wasm_init(&rt);

    CHECK_MSG(t, rt.gc_heap != NULL, "%s", "expected wasm_init to allocate a GC heap arena");
    CHECK_MSG(t, rt.gc_heap_size == (size_t)WASM_GC_HEAP_SIZE,
              "expected GC heap size %zu, got %zu",
              (size_t)WASM_GC_HEAP_SIZE,
              rt.gc_heap_size);
    CHECK_MSG(t, rt.gc_heap_offset == 0, "%s", "expected empty GC heap to start at offset 0");
    CHECK_MSG(t, rt.gc_allocations == NULL, "%s", "expected no GC objects to be tracked initially");

    wasm_destroy(&rt);

    CHECK_MSG(t, rt.gc_heap == NULL, "%s", "expected wasm_destroy to release the GC heap arena");
    CHECK_MSG(t, rt.gc_heap_size == 0, "%s", "expected wasm_destroy to clear GC heap size");
    CHECK_MSG(t, rt.gc_heap_offset == 0, "%s", "expected wasm_destroy to clear GC heap offset");
    CHECK_MSG(t, rt.gc_allocations == NULL, "%s", "expected wasm_destroy to clear tracked GC objects");
}

TEST(test_gc_allocator_tracks_struct_and_array_objects) {
    wasm_runtime_t rt;
    wasm_fieldtype_t struct_fields[2];
    wasm_structtype_t struct_type;
    wasm_arraytype_t array_type;
    wasm_gc_struct_t* struct_obj;
    wasm_gc_array_t* array_obj;
    wasm__gc_root_t root;
    wasm_value_t held;
    wasm_value_t* held_values = &held;
    uint32_t held_count = 1;
    size_t struct_size = 0;
    size_t array_size = 0;
    size_t struct_field0_offset;
    size_t struct_field1_offset;

    memset(struct_fields, 0, sizeof(struct_fields));
    memset(&struct_type, 0, sizeof(struct_type));
    memset(&array_type, 0, sizeof(array_type));

    wasm_init(&rt);

    struct_fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    struct_fields[0].storage.of.valtype = WASM_TYPE_I32;
    struct_fields[1].storage.kind = WASM_STORAGE_PACKED;
    struct_fields[1].storage.of.packed_type = WASM_PACKED_I8;
    struct_fields[1].is_mutable = 1;
    struct_type.num_fields = 2;
    struct_type.fields = struct_fields;

    array_type.field.storage.kind = WASM_STORAGE_PACKED;
    array_type.field.storage.of.packed_type = WASM_PACKED_I16;
    WASM_CHECK_OK(t, wasm__gc_struct_layout_init(&rt.allocator, &struct_type, 0));
    wasm__gc_array_layout_init(&array_type);

    struct_size = struct_type.object_size;
    CHECK_MSG(t, struct_size != 0, "%s", "expected struct object size calculation to succeed");
    CHECK_MSG(t, wasm__gc_array_total_size(&array_type, 4, &array_size), "%s", "expected array object size calculation to succeed");

    struct_field0_offset = struct_type.field_offsets[0];
    struct_field1_offset = struct_type.field_offsets[1];
    CHECK_MSG(t, struct_field0_offset == wasm__align_up_size(WASM__GC_STRUCT_HEADER_SIZE, sizeof(int32_t)),
              "unexpected first struct field offset %zu",
              struct_field0_offset);
    CHECK_MSG(t, struct_field1_offset > struct_field0_offset,
              "expected later packed fields to follow earlier fields (%zu <= %zu)",
              struct_field1_offset,
              struct_field0_offset);

    struct_obj = wasm__gc_alloc_struct_object(&rt, NULL, 7, &struct_type);
    CHECK_MSG(t, struct_obj != NULL, "%s", "expected struct allocation from the GC heap to succeed");
    if (struct_obj == NULL) {
        wasm__gc_struct_layout_free(&rt.allocator, &struct_type);
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, struct_obj->header.type_index == 7, "%s", "expected struct object header to store its runtime type index");
    CHECK_MSG(t, struct_obj->header.kind == WASM_GC_OBJ_STRUCT, "%s", "expected struct allocation kind tag");
    CHECK_MSG(t, struct_obj->header.object_size == struct_size,
              "expected struct object size %zu, got %zu",
              struct_size,
              struct_obj->header.object_size);
    CHECK_MSG(t, struct_obj->header.next_alloc == NULL, "%s", "expected first GC allocation to terminate the allocation list");
    CHECK_MSG(t, rt.gc_allocations == &struct_obj->header, "%s", "expected runtime to track the struct allocation");

    held = wasm__gc_type_ref_value(NULL, 7, &struct_obj->header);
    wasm__gc_push_root(&rt, &root, &held_values, &held_count);
    array_obj = wasm__gc_alloc_array_object(&rt, NULL, 11, &array_type, 4);
    wasm__gc_pop_root(&rt, &root);
    struct_obj = (wasm_gc_struct_t*)held.of.gc_obj;
    CHECK_MSG(t, array_obj != NULL, "%s", "expected array allocation from the GC heap to succeed");
    if (array_obj == NULL) {
        wasm__gc_struct_layout_free(&rt.allocator, &struct_type);
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, array_obj->length == 4, "%s", "expected array header to store its logical length");
    CHECK_MSG(t, array_obj->header.type_index == 11, "%s", "expected array object header to store its runtime type index");
    CHECK_MSG(t, array_obj->header.kind == WASM_GC_OBJ_ARRAY, "%s", "expected array allocation kind tag");
    CHECK_MSG(t, array_obj->header.object_size == array_size,
              "expected array object size %zu, got %zu",
              array_size,
              array_obj->header.object_size);
    CHECK_MSG(t, rt.gc_allocations == &array_obj->header, "%s", "expected latest allocation to be the head of the GC allocation list");
    CHECK_MSG(t, array_obj->header.next_alloc == &struct_obj->header,
              "%s",
              "expected GC allocation list to chain older objects behind newer ones");
    CHECK_MSG(t, (uint8_t*)array_obj >= (uint8_t*)struct_obj + struct_size &&
                     rt.gc_heap_offset == (size_t)((uint8_t*)array_obj - rt.gc_heap) + array_size,
              "%s",
              "expected the second allocation to follow the first in the heap");

    wasm__gc_struct_layout_free(&rt.allocator, &struct_type);
    wasm_destroy(&rt);
}

TEST(test_gc_collect_marks_runtime_roots_and_preserves_object_graphs) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[2];
    wasm_fieldtype_t child_fields[1];
    wasm_fieldtype_t parent_fields[1];
    wasm_global_t globals[1];
    wasm_table_t tables[1];
    wasm_value_t table_elems[1];
    wasm_func_t fake_func;
    wasm_valtype_t fake_local_types[1];
    uint32_t fake_local_slots[1] = { 0 };
    uint32_t fake_ops[1] = { 0x01 };
    uint32_t fake_op_offsets[2] = { 0, 0 };
    uint32_t fake_maps[3] = { 0, 0, 1 };
    uint32_t fake_map_bits[1] = { WASM__SLOT_KIND_ANY };
    wasm__slot_t frame_locals[1];
    wasm_gc_struct_t* child;
    wasm_gc_struct_t* parent;
    wasm_gc_struct_t* table_root;
    wasm_gc_struct_t* local_root;
    wasm_gc_struct_t* stack_root;
    wasm_gc_struct_t* garbage;
    wasm_gc_struct_t* moved_parent;
    wasm_gc_struct_t* moved_child;
    wasm_value_t* parent_field;
    wasm_value_t* child_field;
    wasm_value_t* table_field;
    wasm_value_t* local_field;
    wasm_value_t* stack_field;
    wasm_value_t loaded[5];
    wasm__gc_root_t root;
    wasm_value_t held[6];
    wasm_value_t* held_values = held;
    uint32_t held_count = 0;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(child_fields, 0, sizeof(child_fields));
    memset(parent_fields, 0, sizeof(parent_fields));
    memset(globals, 0, sizeof(globals));
    memset(tables, 0, sizeof(tables));
    memset(table_elems, 0, sizeof(table_elems));
    memset(&fake_func, 0, sizeof(fake_func));
    memset(frame_locals, 0, sizeof(frame_locals));

    wasm_init(&rt);

    child_fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    child_fields[0].storage.of.valtype = WASM_TYPE_I32;
    types[0].kind = WASM_COMP_STRUCT;
    types[0].descriptor = UINT32_MAX;
    types[0].of.struct_.num_fields = 1;
    types[0].of.struct_.fields = child_fields;

    parent_fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    parent_fields[0].storage.of.valtype = WASM_TYPE_STRUCTREF;
    types[1].kind = WASM_COMP_STRUCT;
    types[1].descriptor = UINT32_MAX;
    types[1].of.struct_.num_fields = 1;
    types[1].of.struct_.fields = parent_fields;
    WASM_CHECK_OK(t, wasm__gc_struct_layout_init(&rt.allocator, &types[0].of.struct_, 0));
    WASM_CHECK_OK(t, wasm__gc_struct_layout_init(&rt.allocator, &types[1].of.struct_, 0));

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 2;
    mod.globals = globals;
    mod.num_globals = 1;
    mod.tables = tables;
    mod.num_tables = 1;
    tables[0].elems = table_elems;
    tables[0].size = 1;
    tables[0].reftype = WASM_TYPE_STRUCTREF;
    wasm__gc_register_module(&rt, &mod);

    fake_local_types[0] = WASM_TYPE_STRUCTREF;
    fake_func.num_locals = 1;
    fake_func.locals = fake_local_types;
    fake_func.local_slots = fake_local_slots;
    fake_func.num_local_slots = 1;
    fake_func.ops = fake_ops;
    fake_func.num_ops = 1;
    fake_func.op_offsets = fake_op_offsets;
    fake_func.num_op_offsets = 1;
    fake_func.stack_maps = fake_maps;
    fake_func.num_stack_maps = 1;
    fake_func.stack_map_bits = fake_map_bits;
    fake_func.num_stack_map_bits = 1;
    memset(&rt.exec->call_frames[0], 0, sizeof(rt.exec->call_frames[0]));
    rt.exec->call_frame_sp = 1;
    rt.exec->call_frames[0].func = &fake_func;
    rt.exec->call_frames[0].locals = frame_locals;
    rt.exec->call_frames[0].op_at = fake_ops;

    wasm__gc_push_root(&rt, &root, &held_values, &held_count);
    for (held_count = 0; held_count < 6; held_count++) {
        uint32_t type_index = held_count == 1 ? 1u : 0u;
        wasm_gc_struct_t* object =
            wasm__gc_alloc_struct_object(&rt, &mod, type_index, &types[type_index].of.struct_);

        held[held_count] = wasm__gc_type_ref_value(&mod, type_index, object ? &object->header : NULL);
    }
    wasm__gc_pop_root(&rt, &root);
    child = (wasm_gc_struct_t*)held[0].of.gc_obj;
    parent = (wasm_gc_struct_t*)held[1].of.gc_obj;
    table_root = (wasm_gc_struct_t*)held[2].of.gc_obj;
    local_root = (wasm_gc_struct_t*)held[3].of.gc_obj;
    stack_root = (wasm_gc_struct_t*)held[4].of.gc_obj;
    garbage = (wasm_gc_struct_t*)held[5].of.gc_obj;

    CHECK_MSG(t, child && parent && table_root && local_root && stack_root && garbage,
              "%s",
              "expected all GC test allocations to succeed");
    if (!(child && parent && table_root && local_root && stack_root && garbage)) {
        wasm__gc_unregister_module(&rt, &mod);
        wasm__gc_struct_layout_free(&rt.allocator, &types[0].of.struct_);
        wasm__gc_struct_layout_free(&rt.allocator, &types[1].of.struct_);
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, wasm__gc_struct_store_field(child, &types[0].of.struct_, 0, wasm_i32(123)) == WASM_OK,
              "%s",
              "expected child field store to succeed");
    CHECK_MSG(t, wasm__gc_struct_store_field(parent, &types[1].of.struct_, 0, wasm__gc_type_ref_value(&mod, 0, &child->header)) == WASM_OK,
              "%s",
              "expected parent field store to succeed");
    CHECK_MSG(t, wasm__gc_struct_store_field(table_root, &types[0].of.struct_, 0, wasm_i32(77)) == WASM_OK,
              "%s",
              "expected table-root field store to succeed");
    CHECK_MSG(t, wasm__gc_struct_store_field(local_root, &types[0].of.struct_, 0, wasm_i32(99)) == WASM_OK,
              "%s",
              "expected local-root field store to succeed");
    CHECK_MSG(t, wasm__gc_struct_store_field(stack_root, &types[0].of.struct_, 0, wasm_i32(55)) == WASM_OK,
              "%s",
              "expected stack-root field store to succeed");
    CHECK_MSG(t, wasm__gc_struct_store_field(garbage, &types[0].of.struct_, 0, wasm_i32(-1)) == WASM_OK,
              "%s",
              "expected garbage field store to succeed");

    globals[0].type = WASM_TYPE_STRUCTREF;
    globals[0].value = wasm__gc_type_ref_value(&mod, 1, &parent->header);
    table_elems[0] = wasm__gc_type_ref_value(&mod, 0, &table_root->header);
    wasm__slot_set_ref(&frame_locals[0], (uintptr_t)&local_root->header);
    wasm__slot_set_ref(&rt.exec->stack[rt.exec->sp++], (uintptr_t)&stack_root->header);

    wasm_gc_collect(&rt);

    CHECK_MSG(t, wasm_test_gc_allocation_count(&rt) == 5,
              "expected 5 live GC objects after collection, got %u",
              (unsigned)wasm_test_gc_allocation_count(&rt));

    moved_parent = (wasm_gc_struct_t*)globals[0].value.of.gc_obj;
    parent_field = wasm_test_gc_struct_field_value(moved_parent, &types[1].of.struct_, 0, &loaded[0]);
    moved_child = parent_field ? (wasm_gc_struct_t*)parent_field->of.gc_obj : NULL;
    child_field = wasm_test_gc_struct_field_value(moved_child, &types[0].of.struct_, 0, &loaded[1]);
    table_field = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)table_elems[0].of.gc_obj,
                                                  &types[0].of.struct_,
                                                  0,
                                                  &loaded[2]);
    local_field = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)wasm__slot_ref(&frame_locals[0]),
                                                  &types[0].of.struct_,
                                                  0,
                                                  &loaded[3]);
    stack_field = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)wasm__slot_ref(&rt.exec->stack[0]),
                                                  &types[0].of.struct_,
                                                  0,
                                                  &loaded[4]);

    CHECK_MSG(t, moved_parent != NULL && moved_parent != parent,
              "%s",
              "expected collection to rewrite the rooted parent reference");
    CHECK_MSG(t, moved_child != NULL && moved_child != child,
              "%s",
              "expected collection to rewrite nested child references");
    CHECK_MSG(t, child_field != NULL && child_field->of.i32 == 123,
              "%s",
              "expected child object contents to survive collection");
    CHECK_MSG(t, table_field != NULL && table_field->of.i32 == 77,
              "%s",
              "expected table roots to survive collection");
    CHECK_MSG(t, local_field != NULL && local_field->of.i32 == 99,
              "%s",
              "expected call-frame local roots to survive collection");
    CHECK_MSG(t, stack_field != NULL && stack_field->of.i32 == 55,
              "%s",
              "expected operand-stack roots to survive collection");

    wasm__gc_unregister_module(&rt, &mod);
    wasm__gc_struct_layout_free(&rt.allocator, &types[0].of.struct_);
    wasm__gc_struct_layout_free(&rt.allocator, &types[1].of.struct_);
    wasm_destroy(&rt);
}

TEST(test_gc_collect_marks_deep_object_chains_iteratively) {
#ifdef WASM_GC_STRESS
    enum { CHAIN_LENGTH = 2000 };
#else
    enum { CHAIN_LENGTH = 250000 };
#endif
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[1];
    wasm_fieldtype_t fields[1];
    wasm_global_t globals[1];
    wasm_value_t head;
    wasm_value_t* head_values = &head;
    uint32_t head_count = 1;
    wasm__gc_root_t root;
    wasm_gc_header_t* object;
    size_t object_size = 0;
    uint32_t i;
    uint32_t length = 0;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(fields, 0, sizeof(fields));
    memset(globals, 0, sizeof(globals));

    wasm_init(&rt);

    fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    fields[0].storage.of.valtype = WASM_TYPE_STRUCTREF;
    types[0].kind = WASM_COMP_STRUCT;
    types[0].descriptor = UINT32_MAX;
    types[0].of.struct_.num_fields = 1;
    types[0].of.struct_.fields = fields;
    WASM_CHECK_OK(t, wasm__gc_struct_layout_init(&rt.allocator, &types[0].of.struct_, 0));

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 1;
    mod.globals = globals;
    mod.num_globals = 1;
    globals[0].type = WASM_TYPE_STRUCTREF;
    globals[0].value = wasm_ref_null(WASM_TYPE_STRUCTREF);
    wasm__gc_register_module(&rt, &mod);

    object_size = types[0].of.struct_.object_size;
    CHECK_MSG(t, object_size != 0, "%s", "expected struct size calculation to succeed");
    CHECK_MSG(t, wasm__gc_heap_init(&rt, object_size * (CHAIN_LENGTH + 16u)) == WASM_OK, "%s",
              "expected GC heap resize to succeed");

    head = wasm_ref_null(WASM_TYPE_STRUCTREF);
    wasm__gc_push_root(&rt, &root, &head_values, &head_count);
    for (i = 0; i < CHAIN_LENGTH; i++) {
        wasm_gc_struct_t* node = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);

        if (!node || wasm__gc_struct_store_field(node, &types[0].of.struct_, 0, head) != WASM_OK) break;
        head = wasm__gc_type_ref_value(&mod, 0, &node->header);
    }
    wasm__gc_pop_root(&rt, &root);
    CHECK_MSG(t, i == CHAIN_LENGTH, "expected %u chain allocations, got %u", (unsigned)CHAIN_LENGTH,
              (unsigned)i);
    globals[0].value = head;

    wasm_gc_collect(&rt);

    object = globals[0].value.of.gc_obj;
    while (object) {
        wasm_value_t next_value;
        wasm_value_t* next =
            wasm_test_gc_struct_field_value((wasm_gc_struct_t*)object, &types[0].of.struct_, 0, &next_value);

        length++;
        object = next && wasm__is_heap_gc_ref_value(next) ? next->of.gc_obj : NULL;
    }
    CHECK_MSG(t, length == CHAIN_LENGTH, "expected the whole chain to survive, walked %u", (unsigned)length);
    CHECK_MSG(t, wasm_test_gc_allocation_count(&rt) == CHAIN_LENGTH, "expected %u live objects, got %u",
              (unsigned)CHAIN_LENGTH, (unsigned)wasm_test_gc_allocation_count(&rt));

    wasm__gc_unregister_module(&rt, &mod);
    wasm__gc_struct_layout_free(&rt.allocator, &types[0].of.struct_);
    wasm_destroy(&rt);
}

TEST(test_gc_allocator_retries_after_collection_on_heap_exhaustion) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[1];
    wasm_fieldtype_t fields[1];
    wasm_gc_struct_t* root;
    wasm_gc_struct_t* garbage;
    wasm_gc_struct_t* fresh;
    wasm_value_t* held;
    size_t object_size = 0;
    size_t heap_size;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(fields, 0, sizeof(fields));

    wasm_init(&rt);

    fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    fields[0].storage.of.valtype = WASM_TYPE_I32;
    types[0].kind = WASM_COMP_STRUCT;
    types[0].descriptor = UINT32_MAX;
    types[0].of.struct_.num_fields = 1;
    types[0].of.struct_.fields = fields;
    WASM_CHECK_OK(t, wasm__gc_struct_layout_init(&rt.allocator, &types[0].of.struct_, 0));

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 1;
    wasm__gc_register_module(&rt, &mod);

    object_size = types[0].of.struct_.object_size;
    CHECK_MSG(t, object_size != 0, "%s", "expected struct size calculation to succeed");
    heap_size = object_size * 2u + (object_size / 2u);
    CHECK_MSG(t, wasm__gc_heap_init(&rt, heap_size) == WASM_OK,
              "%s",
              "expected GC heap resize for exhaustion test to succeed");

    root = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    garbage = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    CHECK_MSG(t, root != NULL && garbage != NULL,
              "%s",
              "expected initial allocations in the resized heap to succeed");
    if (!(root && garbage)) {
        wasm__gc_unregister_module(&rt, &mod);
        wasm__gc_struct_layout_free(&rt.allocator, &types[0].of.struct_);
        wasm_destroy(&rt);
        return;
    }

    held = wasm_ref_hold(&rt, wasm__gc_type_ref_value(&mod, 0, &root->header));
    fresh = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);

    CHECK_MSG(t, fresh != NULL,
              "%s",
              "expected GC allocation to retry after collecting unreachable objects");
    CHECK_MSG(t, wasm_test_gc_allocation_count(&rt) == 2,
              "expected only the rooted object and the fresh allocation to remain, got %u",
              (unsigned)wasm_test_gc_allocation_count(&rt));
    CHECK_MSG(t, held && held->of.gc_obj != NULL && held->of.gc_obj != &root->header,
              "%s",
              "expected collection to rewrite the rooted reference during retry");

    wasm_ref_release(&rt, held);
    wasm__gc_unregister_module(&rt, &mod);
    wasm__gc_struct_layout_free(&rt.allocator, &types[0].of.struct_);
    wasm_destroy(&rt);
}

static const uint8_t wasm_test_gc_growth_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x1d, 0x06, 0x5f, 0x02, 0x7e, 0x00, 0x7e, 0x00, 0x5e,
        0x63, 0x00, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x63, 0x01, 0x60, 0x00, 0x01, 0x7f,
        0x60, 0x00, 0x00, 0x03, 0x05, 0x04, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x01, 0x63, 0x01, 0x01, 0xd0, 0x01,
        0x0b, 0x07, 0x1e, 0x04, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x00, 0x04, 0x66, 0x69, 0x6c, 0x6c, 0x00, 0x01, 0x05,
        0x63, 0x68, 0x65, 0x63, 0x6b, 0x00, 0x02, 0x05, 0x63, 0x6c, 0x65, 0x61, 0x72, 0x00, 0x03, 0x0a, 0xb9, 0x01,
        0x04, 0x31, 0x02, 0x01, 0x63, 0x01, 0x01, 0x7f, 0x20, 0x00, 0xfb, 0x07, 0x01, 0x21, 0x01, 0x03, 0x40, 0x20,
        0x01, 0x20, 0x02, 0x42, 0x01, 0x42, 0x02, 0xfb, 0x00, 0x00, 0xfb, 0x0e, 0x01, 0x20, 0x02, 0x41, 0x01, 0x6a,
        0x21, 0x02, 0x20, 0x02, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0xfb, 0x0f, 0x0b, 0x31, 0x01, 0x01,
        0x7f, 0x20, 0x00, 0xfb, 0x07, 0x01, 0x24, 0x00, 0x03, 0x40, 0x23, 0x00, 0x20, 0x01, 0x20, 0x01, 0xad, 0x20,
        0x01, 0xad, 0x42, 0x03, 0x7e, 0xfb, 0x00, 0x00, 0xfb, 0x0e, 0x01, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01,
        0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x23, 0x00, 0x0b, 0x4c, 0x02, 0x02, 0x7f, 0x01, 0x63, 0x00,
        0x02, 0x40, 0x03, 0x40, 0x20, 0x00, 0x23, 0x00, 0xfb, 0x0f, 0x4f, 0x0d, 0x01, 0x23, 0x00, 0x20, 0x00, 0xfb,
        0x0b, 0x01, 0x21, 0x02, 0x20, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x00, 0xad, 0x51, 0x20, 0x02, 0xfb, 0x02,
        0x00, 0x01, 0x20, 0x00, 0xad, 0x42, 0x03, 0x7e, 0x51, 0x71, 0x04, 0x40, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21,
        0x01, 0x0b, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x01, 0x0b, 0x06, 0x00,
        0xd0, 0x01, 0x24, 0x00, 0x0b,
};

static wasm_module_t* wasm_test_gc_growth_load(test_ctx* t, wasm_runtime_t* rt, const wasm_config_t* config) {
    wasm_module_t* m;

    wasm_init_config(rt, config);
    m = wasm_load(rt, wasm_test_gc_growth_bytes, sizeof(wasm_test_gc_growth_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    return m;
}

static int wasm_test_gc_held_array_intact(wasm_module_t* m, wasm_value_t array, uint32_t expected_len) {
    uint32_t len = 0;
    uint32_t i;

    if (wasm_array_length(m, array, &len) != WASM_OK || len != expected_len) return 0;
    for (i = 0; i < len; i++) {
        wasm_value_t elem;
        wasm_value_t first;
        wasm_value_t second;

        if (wasm_array_get_elem(m, array, i, &elem) != WASM_OK) return 0;
        if (wasm_struct_get_field(m, elem, 0, &first) != WASM_OK ||
            wasm_struct_get_field(m, elem, 1, &second) != WASM_OK)
            return 0;
        if (first.of.i64 != (int64_t)i || second.of.i64 != (int64_t)i * 3) return 0;
    }
    return 1;
}

TEST(test_gc_heap_grows_with_live_data) {
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

#ifndef WASM_GC_STRESS
    wasm_config_default(&config);
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        arg = wasm_i32(1000000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1000000);
        CHECK_MSG(t, rt.gc_heap_size > (size_t)WASM_GC_HEAP_SIZE, "expected the heap to grow, size %zu",
                  rt.gc_heap_size);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_config_default(&config);
    config.initial_gc_heap_size = 64u * 1024u;
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        wasm_value_t* held;
        size_t heap_size;

        arg = wasm_i32(20000);
        WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, &result, 1));
        held = wasm_ref_hold(&rt, result);
        CHECK(t, held != NULL);
        WASM_CHECK_OK(t, wasm_call(m, "check", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 20000);
        heap_size = rt.gc_heap_size;
        CHECK_MSG(t, heap_size > config.initial_gc_heap_size, "expected the heap to grow, size %zu", heap_size);

        arg = wasm_i32(200000);
        WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, &result, 1));
        arg = wasm_i32(500000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        CHECK_MSG(t, rt.gc_heap_size > heap_size, "expected the heap to grow again, size %zu", rt.gc_heap_size);
        WASM_CHECK_OK(t, wasm_call(m, "check", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 200000);
        if (held) {
            CHECK(t, wasm_test_gc_held_array_intact(m, *held, 20000));
            wasm_ref_release(&rt, held);
        }

        WASM_CHECK_OK(t, wasm_call(m, "clear", NULL, 0, NULL, 0));
        wasm_gc_collect(&rt);
        CHECK_MSG(t, rt.gc_heap_size == config.initial_gc_heap_size,
                  "expected an empty heap to shrink to its initial size, size %zu", rt.gc_heap_size);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
#endif

    wasm_config_default(&config);
    config.initial_gc_heap_size = 0;
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        arg = wasm_i32(1000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1000);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_heap_growth_respects_max_gc_heap_bytes) {
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);
    wasm_value_t* held = NULL;
    wasm_error_t err;

    wasm_config_default(&config);
    CHECK(t, config.max_gc_heap_bytes == 0u);
    config.max_gc_heap_bytes = 1024u * 1024u;
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        CHECK_MSG(t, rt.gc_heap_size == 1024u * 1024u, "expected the initial heap to be capped, size %zu",
                  rt.gc_heap_size);
        arg = wasm_i32(2000);
        WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, &result, 1));
        held = wasm_ref_hold(&rt, result);
        CHECK(t, held != NULL);

        arg = wasm_i32(100000);
        err = wasm_call(m, "run", &arg, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_OOM, "expected WASM_ERR_OOM, got %s", wasm_error_string(err));
        CHECK(t, wasm_runtime_last_error(&rt) == WASM_ERR_OOM);
        CHECK_MSG(t, strstr(wasm_runtime_error_message(&rt), "limit") != NULL, "unexpected message '%s'",
                  wasm_runtime_error_message(&rt));
        CHECK(t, rt.gc_heap_size <= 1024u * 1024u);

        WASM_CHECK_OK(t, wasm_call(m, "check", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 2000);
        if (held) CHECK(t, wasm_test_gc_held_array_intact(m, *held, 2000));
        arg = wasm_i32(1000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1000);

        arg = wasm_i32(1000000);
        err = wasm_call(m, "run", &arg, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_OOM, "expected an array larger than the limit to fail, got %s",
                  wasm_error_string(err));
        if (held) {
            CHECK(t, wasm_test_gc_held_array_intact(m, *held, 2000));
            wasm_ref_release(&rt, held);
        }
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_host_externrefs_round_trip_through_anyref) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x24, 0x08, 0x5f, 0x01, 0x7e, 0x00, 0x5e, 0x63, 0x00,
        0x01, 0x60, 0x01, 0x6f, 0x00, 0x60, 0x00, 0x00, 0x60, 0x02, 0x6f, 0x7f, 0x01, 0x6f, 0x60, 0x00, 0x01, 0x6f,
        0x60, 0x01, 0x6f, 0x01, 0x7f, 0x60, 0x01, 0x6f, 0x01, 0x6f, 0x03, 0x0b, 0x0a, 0x02, 0x03, 0x04, 0x05, 0x05,
        0x06, 0x07, 0x02, 0x02, 0x02, 0x04, 0x04, 0x01, 0x6e, 0x00, 0x01, 0x06, 0x06, 0x01, 0x6e, 0x01, 0xd0, 0x6e,
        0x0b, 0x07, 0x5f, 0x0a, 0x04, 0x6b, 0x65, 0x65, 0x70, 0x00, 0x00, 0x05, 0x63, 0x6c, 0x65, 0x61, 0x72, 0x00,
        0x01, 0x05, 0x63, 0x68, 0x75, 0x72, 0x6e, 0x00, 0x02, 0x06, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x00, 0x03,
        0x05, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x04, 0x05, 0x74, 0x65, 0x73, 0x74, 0x73, 0x00, 0x05, 0x08, 0x63,
        0x61, 0x73, 0x74, 0x5f, 0x61, 0x6e, 0x79, 0x00, 0x06, 0x07, 0x63, 0x61, 0x73, 0x74, 0x5f, 0x65, 0x71, 0x00,
        0x07, 0x08, 0x63, 0x61, 0x73, 0x74, 0x5f, 0x69, 0x33, 0x31, 0x00, 0x08, 0x0b, 0x63, 0x61, 0x73, 0x74, 0x5f,
        0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x00, 0x09, 0x0a, 0xc1, 0x01, 0x0a, 0x10, 0x00, 0x20, 0x00, 0xfb, 0x1a,
        0x24, 0x00, 0x41, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0x26, 0x00, 0x0b, 0x0c, 0x00, 0xd0, 0x6e, 0x24, 0x00, 0x41,
        0x00, 0xd0, 0x6e, 0x26, 0x00, 0x0b, 0x39, 0x02, 0x01, 0x6e, 0x01, 0x63, 0x01, 0x20, 0x00, 0xfb, 0x1a, 0x21,
        0x02, 0x20, 0x01, 0xfb, 0x07, 0x01, 0x21, 0x03, 0x03, 0x40, 0x20, 0x01, 0x41, 0x01, 0x6b, 0x21, 0x01, 0x20,
        0x03, 0x20, 0x01, 0x20, 0x01, 0xad, 0xfb, 0x00, 0x00, 0xfb, 0x0e, 0x01, 0x42, 0x00, 0xfb, 0x00, 0x00, 0x1a,
        0x20, 0x01, 0x0d, 0x00, 0x0b, 0x20, 0x02, 0xfb, 0x1b, 0x0b, 0x06, 0x00, 0x23, 0x00, 0xfb, 0x1b, 0x0b, 0x08,
        0x00, 0x41, 0x00, 0x25, 0x00, 0xfb, 0x1b, 0x0b, 0x2a, 0x01, 0x01, 0x6e, 0x20, 0x00, 0xfb, 0x1a, 0x21, 0x01,
        0x20, 0x01, 0xfb, 0x14, 0x6e, 0x20, 0x01, 0xfb, 0x14, 0x6d, 0x41, 0x01, 0x74, 0x72, 0x20, 0x01, 0xfb, 0x14,
        0x6c, 0x41, 0x02, 0x74, 0x20, 0x01, 0xfb, 0x14, 0x6b, 0x41, 0x03, 0x74, 0x72, 0x72, 0x0b, 0x0b, 0x00, 0x20,
        0x00, 0xfb, 0x1a, 0xfb, 0x16, 0x6e, 0xfb, 0x1b, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0xfb, 0x16, 0x6d,
        0x1a, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0xfb, 0x16, 0x6c, 0x1a, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0xfb,
        0x1a, 0xfb, 0x16, 0x6b, 0x1a, 0x0b,
    };
    static int host_object;
    const uintptr_t handles[] = { (uintptr_t)1u, (uintptr_t)3u, (uintptr_t)0x1234u, (uintptr_t)0x1235u,
                                  (uintptr_t)&host_object, UINTPTR_MAX - 1u, UINTPTR_MAX };
    const char* failing_casts[] = { "cast_eq", "cast_i31", "cast_struct" };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t max_boxes = 0;
    size_t heap_size;
    size_t i;
    size_t j;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 8192;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    for (i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
        wasm_value_t args[2];
        wasm_value_t result = wasm_i32(0);

        args[0] = wasm_externref(handles[i]);
        args[1] = wasm_i32(2000 * (int32_t)(i + 1));
        WASM_CHECK_OK(t, wasm_call(m, "keep", args, 1, NULL, 0));
        heap_size = rt.gc_heap_size;
        WASM_CHECK_OK(t, wasm_call(m, "churn", args, 2, &result, 1));
        CHECK_MSG(t, rt.gc_heap_size > heap_size, "expected the heap to grow, size %zu", rt.gc_heap_size);
        CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_call(m, "global", NULL, 0, &result, 1));
        CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);
        WASM_CHECK_OK(t, wasm_call(m, "table", NULL, 0, &result, 1));
        CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);

        WASM_CHECK_OK(t, wasm_call(m, "tests", args, 1, &result, 1));
        CHECK_MSG(t, result.of.i32 == 1, "value %p: expected only ref.test any to succeed, mask %d",
                  (void*)handles[i], (int)result.of.i32);
        WASM_CHECK_OK(t, wasm_call(m, "cast_any", args, 1, &result, 1));
        CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);
        for (j = 0; j < sizeof(failing_casts) / sizeof(failing_casts[0]); j++)
            CHECK_MSG(t, wasm_call(m, failing_casts[j], args, 1, NULL, 0) == WASM_ERR_TRAP, "%s of %p succeeded",
                      failing_casts[j], (void*)handles[i]);
    }

    for (i = 0; i < 20000; i++) {
        wasm_value_t arg = wasm_externref(handles[i % (sizeof(handles) / sizeof(handles[0]))]);
        wasm_value_t result = wasm_i32(0);

        if (wasm_call(m, "tests", &arg, 1, &result, 1) != WASM_OK || result.of.i32 != 1) break;
        if (rt.externref_boxes.count > max_boxes) max_boxes = rt.externref_boxes.count;
    }
    CHECK_MSG(t, i == 20000, "conversion loop stopped at %u", (unsigned)i);
    CHECK_MSG(t, max_boxes < 4096, "expected unreachable boxes to be freed, %u live", (unsigned)max_boxes);

    WASM_CHECK_OK(t, wasm_call(m, "clear", NULL, 0, NULL, 0));
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.externref_boxes.count == 0, "expected every box to be freed, %u live",
              (unsigned)rt.externref_boxes.count);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

#define WASM_TEST_HEAP_ARENA_SIZE (128u * 1024u)

static int g_wasm_test_heap_arena_used[2];

static uint8_t* wasm_test_heap_arena(int index) {
    static uint8_t raw[2u * WASM_TEST_HEAP_ARENA_SIZE + 16u];
    uintptr_t base = ((uintptr_t)raw + 15u) & ~(uintptr_t)15u;

    return (uint8_t*)base + (size_t)index * WASM_TEST_HEAP_ARENA_SIZE;
}

static void* wasm_test_heap_alloc(void* ctx, size_t size, size_t align) {
    int i;

    if (align != 16u || size < 4096u || size > WASM_TEST_HEAP_ARENA_SIZE) return wasm_test_alloc(ctx, size, align);
    for (i = 0; i < 2; i++) {
        if (!g_wasm_test_heap_arena_used[i]) {
            g_wasm_test_heap_arena_used[i] = 1;
            return wasm_test_heap_arena(i);
        }
    }
    return NULL;
}

static void* wasm_test_heap_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align) {
    if (p == wasm_test_heap_arena(0) || p == wasm_test_heap_arena(1)) return NULL;
    return wasm_test_resize(ctx, p, old_size, new_size, align);
}

static void wasm_test_heap_free(void* ctx, void* p, size_t size, size_t align) {
    int i;

    for (i = 0; i < 2; i++) {
        if (p == wasm_test_heap_arena(i)) {
            g_wasm_test_heap_arena_used[i] = 0;
            return;
        }
    }
    wasm_test_free(ctx, p, size, align);
}

TEST(test_gc_host_refs_inside_a_later_heap_stay_host_values) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x27, 0x09, 0x5f, 0x01, 0x7e, 0x01, 0x60, 0x02, 0x6f,
        0x6e, 0x00, 0x60, 0x00, 0x01, 0x6f, 0x60, 0x00, 0x01, 0x6e, 0x60, 0x01, 0x6e, 0x01, 0x7f, 0x60, 0x00, 0x01,
        0x7f, 0x60, 0x01, 0x7e, 0x00, 0x60, 0x00, 0x01, 0x7e, 0x60, 0x01, 0x7f, 0x00, 0x03, 0x09, 0x08, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x06, 0x11, 0x03, 0x6e, 0x01, 0xd0, 0x6e, 0x0b, 0x6e, 0x01, 0xd0, 0x6e,
        0x0b, 0x63, 0x00, 0x01, 0xd0, 0x00, 0x0b, 0x07, 0x31, 0x07, 0x04, 0x6b, 0x65, 0x65, 0x70, 0x00, 0x00, 0x03,
        0x65, 0x78, 0x74, 0x00, 0x01, 0x03, 0x61, 0x6e, 0x79, 0x00, 0x02, 0x05, 0x74, 0x65, 0x73, 0x74, 0x73, 0x00,
        0x04, 0x03, 0x70, 0x69, 0x6e, 0x00, 0x05, 0x04, 0x6c, 0x69, 0x76, 0x65, 0x00, 0x06, 0x05, 0x63, 0x68, 0x75,
        0x72, 0x6e, 0x00, 0x07, 0x0a, 0x75, 0x08, 0x0c, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0x24, 0x00, 0x20, 0x01, 0x24,
        0x01, 0x0b, 0x06, 0x00, 0x23, 0x00, 0xfb, 0x1b, 0x0b, 0x04, 0x00, 0x23, 0x01, 0x0b, 0x22, 0x00, 0x20, 0x00,
        0xfb, 0x15, 0x6e, 0x20, 0x00, 0xfb, 0x15, 0x6d, 0x41, 0x01, 0x74, 0x72, 0x20, 0x00, 0xfb, 0x15, 0x6b, 0x41,
        0x02, 0x74, 0x20, 0x00, 0xfb, 0x15, 0x6c, 0x41, 0x03, 0x74, 0x72, 0x72, 0x0b, 0x0e, 0x00, 0x23, 0x00, 0x10,
        0x03, 0x23, 0x01, 0x10, 0x03, 0x41, 0x04, 0x74, 0x72, 0x0b, 0x09, 0x00, 0x20, 0x00, 0xfb, 0x00, 0x00, 0x24,
        0x02, 0x0b, 0x08, 0x00, 0x23, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x0b, 0x15, 0x00, 0x03, 0x40, 0x20, 0x00, 0xad,
        0xfb, 0x00, 0x00, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x0b,
    };
    static const wasm_allocator_t allocator = { NULL, wasm_test_heap_alloc, wasm_test_heap_resize,
                                                wasm_test_heap_free };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m = NULL;
    wasm_value_t args[2];
    wasm_value_t result;
    uintptr_t ext_bits;
    uintptr_t any_bits;
    int round;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 8192;
    config.max_gc_heap_bytes = WASM_TEST_HEAP_ARENA_SIZE;
    WASM_CHECK_OK(t, wasm__runtime_init(&rt, &allocator, &config));
    CHECK(t, rt.gc_heap == wasm_test_heap_arena(0));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        args[0] = wasm_i64(7);
        WASM_CHECK_OK(t, wasm_call(m, "pin", args, 1, NULL, 0));
        ext_bits = (uintptr_t)wasm_test_heap_arena(1);
        any_bits = ext_bits + 64u;
        args[0] = wasm_externref(ext_bits);
        args[1] = wasm_externref(any_bits);
        args[1].type = WASM_TYPE_ANYREF;
        WASM_CHECK_OK(t, wasm_call(m, "keep", args, 2, NULL, 0));
        for (round = 0; round < 6; round++) {
            if (round % 2 == 0) {
                wasm_gc_collect(&rt);
            } else {
                args[0] = wasm_i32(3000);
                WASM_CHECK_OK(t, wasm_call(m, "churn", args, 1, NULL, 0));
            }
            CHECK(t, rt.gc_heap == wasm_test_heap_arena(0) || rt.gc_heap == wasm_test_heap_arena(1));
            WASM_CHECK_OK(t, wasm_call(m, "ext", NULL, 0, &result, 1));
            CHECK_MSG(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == ext_bits,
                      "round %d: externref came back as %p", round, (void*)result.of.externref);
            WASM_CHECK_OK(t, wasm_call(m, "any", NULL, 0, &result, 1));
            CHECK_MSG(t, result.type == WASM_TYPE_ANYREF && result.of.gc_ref == any_bits,
                      "round %d: anyref came back as %p", round, (void*)result.of.gc_ref);
            WASM_CHECK_OK(t, wasm_call(m, "tests", NULL, 0, &result, 1));
            CHECK_MSG(t, result.of.i32 == 0x11, "round %d: expected only ref.test any to succeed, mask 0x%x",
                      round, (unsigned)result.of.i32);
            WASM_CHECK_OK(t, wasm_call(m, "live", NULL, 0, &result, 1));
            WASM_CHECK_I64(t, result.of.i64, 7);
        }
    }
    wasm_module_free(m);
    wasm__runtime_destroy(&rt);
    CHECK(t, !g_wasm_test_heap_arena_used[0] && !g_wasm_test_heap_arena_used[1]);
}

static const uint8_t wasm_test_init_pair_150000_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x04, 0x5e, 0x7e, 0x01, 0x5f, 0x02, 0x63, 0x00,
        0x00, 0x63, 0x00, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7e, 0x60, 0x00, 0x01, 0x7e, 0x03, 0x04, 0x03, 0x02,
        0x03, 0x03, 0x06, 0x1a, 0x01, 0x63, 0x01, 0x00, 0x42, 0x07, 0x41, 0xf0, 0x93, 0x09, 0xfb, 0x06, 0x00, 0x42,
        0x09, 0x41, 0xf0, 0x93, 0x09, 0xfb, 0x06, 0x00, 0xfb, 0x00, 0x01, 0x0b, 0x07, 0x13, 0x02, 0x06, 0x66, 0x69,
        0x65, 0x6c, 0x64, 0x30, 0x00, 0x01, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x02, 0x0a, 0x26, 0x03,
        0x0e, 0x00, 0x20, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x41, 0x01, 0x6b, 0xfb, 0x0b, 0x00, 0x0b, 0x0a, 0x00, 0x23,
        0x00, 0xfb, 0x02, 0x01, 0x00, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x00, 0xfb, 0x02, 0x01, 0x01, 0x10, 0x00,
        0x0b,
};

static const uint8_t wasm_test_init_pair_300000_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x04, 0x5e, 0x7e, 0x01, 0x5f, 0x02, 0x63, 0x00,
        0x00, 0x63, 0x00, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7e, 0x60, 0x00, 0x01, 0x7e, 0x03, 0x04, 0x03, 0x02,
        0x03, 0x03, 0x06, 0x1a, 0x01, 0x63, 0x01, 0x00, 0x42, 0x07, 0x41, 0xe0, 0xa7, 0x12, 0xfb, 0x06, 0x00, 0x42,
        0x09, 0x41, 0xe0, 0xa7, 0x12, 0xfb, 0x06, 0x00, 0xfb, 0x00, 0x01, 0x0b, 0x07, 0x13, 0x02, 0x06, 0x66, 0x69,
        0x65, 0x6c, 0x64, 0x30, 0x00, 0x01, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x02, 0x0a, 0x26, 0x03,
        0x0e, 0x00, 0x20, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x41, 0x01, 0x6b, 0xfb, 0x0b, 0x00, 0x0b, 0x0a, 0x00, 0x23,
        0x00, 0xfb, 0x02, 0x01, 0x00, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x00, 0xfb, 0x02, 0x01, 0x01, 0x10, 0x00,
        0x0b,
};

static const uint8_t wasm_test_init_sequence_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x1f, 0x06, 0x5e, 0x7e, 0x01, 0x5f, 0x02, 0x63, 0x00,
        0x00, 0x63, 0x00, 0x00, 0x5e, 0x63, 0x00, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7e, 0x60, 0x00, 0x01, 0x7e,
        0x60, 0x01, 0x7f, 0x01, 0x7e, 0x03, 0x0a, 0x09, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x04, 0x05, 0x05, 0x04,
        0x18, 0x03, 0x40, 0x00, 0x63, 0x00, 0x00, 0x01, 0x42, 0x05, 0x41, 0xc0, 0x00, 0xfb, 0x06, 0x00, 0x0b, 0x63,
        0x00, 0x00, 0x02, 0x63, 0x00, 0x00, 0x02, 0x06, 0x4f, 0x04, 0x63, 0x02, 0x00, 0x42, 0x17, 0x41, 0x80, 0x01,
        0xfb, 0x06, 0x00, 0x41, 0x80, 0x02, 0xfb, 0x06, 0x02, 0x0b, 0x63, 0x00, 0x00, 0x42, 0x03, 0x41, 0x80, 0x04,
        0xfb, 0x06, 0x00, 0x0b, 0x63, 0x01, 0x00, 0x42, 0x07, 0x41, 0x80, 0x08, 0xfb, 0x06, 0x00, 0x42, 0x09, 0x41,
        0x80, 0x10, 0xfb, 0x06, 0x00, 0xfb, 0x00, 0x01, 0x0b, 0x63, 0x02, 0x00, 0x42, 0x13, 0x41, 0x80, 0x20, 0xfb,
        0x06, 0x00, 0x42, 0x15, 0x41, 0x80, 0xc0, 0x00, 0xfb, 0x06, 0x00, 0xfb, 0x08, 0x02, 0x02, 0x0b, 0x07, 0x4c,
        0x08, 0x08, 0x72, 0x65, 0x70, 0x65, 0x61, 0x74, 0x65, 0x64, 0x00, 0x01, 0x06, 0x67, 0x6c, 0x6f, 0x62, 0x61,
        0x6c, 0x00, 0x02, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x03, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64,
        0x31, 0x00, 0x04, 0x05, 0x66, 0x69, 0x78, 0x65, 0x64, 0x00, 0x05, 0x07, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c,
        0x74, 0x00, 0x06, 0x06, 0x61, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x07, 0x07, 0x70, 0x61, 0x73, 0x73, 0x69,
        0x76, 0x65, 0x00, 0x08, 0x09, 0x35, 0x02, 0x05, 0x63, 0x00, 0x02, 0x42, 0x0b, 0x41, 0x80, 0x80, 0x01, 0xfb,
        0x06, 0x00, 0x0b, 0x42, 0x0d, 0x41, 0x80, 0x80, 0x02, 0xfb, 0x06, 0x00, 0x0b, 0x06, 0x01, 0x41, 0x00, 0x0b,
        0x63, 0x00, 0x02, 0x42, 0x0f, 0x41, 0x80, 0x80, 0x04, 0xfb, 0x06, 0x00, 0x0b, 0x42, 0x11, 0x41, 0x80, 0x80,
        0x08, 0xfb, 0x06, 0x00, 0x0b, 0x0a, 0x6f, 0x09, 0x0e, 0x00, 0x20, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x41, 0x01,
        0x6b, 0xfb, 0x0b, 0x00, 0x0b, 0x10, 0x00, 0x23, 0x00, 0x23, 0x00, 0xfb, 0x0f, 0x41, 0x01, 0x6b, 0xfb, 0x0b,
        0x02, 0x10, 0x00, 0x0b, 0x06, 0x00, 0x23, 0x01, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x02, 0xfb, 0x02, 0x01,
        0x00, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x02, 0xfb, 0x02, 0x01, 0x01, 0x10, 0x00, 0x0b, 0x0b, 0x00, 0x23,
        0x03, 0x20, 0x00, 0xfb, 0x0b, 0x02, 0x10, 0x00, 0x0b, 0x08, 0x00, 0x41, 0x00, 0x25, 0x00, 0x10, 0x00, 0x0b,
        0x08, 0x00, 0x20, 0x00, 0x25, 0x01, 0x10, 0x00, 0x0b, 0x12, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x02, 0xfc,
        0x0c, 0x00, 0x02, 0x20, 0x00, 0x25, 0x02, 0x10, 0x00, 0x0b,
};

static int64_t wasm_test_call_i64(test_ctx* t, wasm_module_t* m, const char* name, int32_t arg, int has_arg) {
    wasm_value_t value = wasm_i32(arg);
    wasm_value_t result = wasm_i64(-1);
    wasm_error_t err = wasm_call(m, name, &value, has_arg ? 1u : 0u, &result, 1);

    CHECK_MSG(t, err == WASM_OK, "%s: %s", name, wasm_runtime_error_message(wasm_module_runtime(m)));
    return result.of.i64;
}

TEST(test_gc_init_expressions_keep_their_operands_alive) {
    const uint8_t* pairs[] = { wasm_test_init_pair_150000_bytes, wasm_test_init_pair_300000_bytes };
    const size_t pair_sizes[] = { sizeof(wasm_test_init_pair_150000_bytes), sizeof(wasm_test_init_pair_300000_bytes) };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    size_t i;

    for (i = 0; i < 2; i++) {
        wasm_init(&rt);
        m = wasm_load(&rt, pairs[i], pair_sizes[i]);
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (m) {
            WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field0", 0, 0), 7);
            WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field1", 0, 0), 9);
        }
        wasm_module_free(m);
        wasm_destroy(&rt);
    }

    wasm_config_default(&config);
    config.initial_gc_heap_size = 0;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, wasm_test_init_sequence_bytes, sizeof(wasm_test_init_sequence_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "default", 0, 0), 5);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "repeated", 0, 0), 23);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "global", 0, 0), 3);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field0", 0, 0), 7);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field1", 0, 0), 9);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "fixed", 0, 1), 19);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "fixed", 1, 1), 21);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "passive", 0, 1), 11);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "passive", 1, 1), 13);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "active", 0, 1), 15);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "active", 1, 1), 17);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static int wasm_test_gc_heap_fits(const wasm_runtime_t* rt, size_t size) {
    size_t offset = wasm__align_up_size(rt->gc_heap_offset, (size_t)WASM__ALIGNOF(wasm_value_t));

    return offset <= rt->gc_heap_size && size <= rt->gc_heap_size - offset;
}

static int wasm_test_big_array_intact(wasm_module_t* m, wasm_value_t array, uint32_t expected_len) {
    uint32_t len = 0;
    uint32_t i;

    if (wasm_array_length(m, array, &len) != WASM_OK || len != expected_len) return 0;
    for (i = 0; i < len; i++) {
        wasm_value_t elem;

        if (wasm_array_get_elem(m, array, i, &elem) != WASM_OK || elem.of.i64 != (int64_t)i * 7) return 0;
    }
    return 1;
}

TEST(test_gc_host_constructors_keep_their_inputs_alive) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0d, 0x03, 0x5e, 0x7e, 0x01, 0x5f, 0x01, 0x63, 0x00,
        0x00, 0x5e, 0x63, 0x00, 0x00,
    };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t elems[16];
    wasm_value_t null_ref = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t array = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t outer = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t garbage;
    wasm_value_t field;
    wasm_value_t* held;
    size_t struct_size = 0;
    size_t pair_size = 0;
    uint32_t i;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    struct_size = wasm__module_const_structtype(m, 1)->object_size;
    CHECK(t, struct_size != 0);
    CHECK(t, wasm__gc_array_total_size(wasm__module_const_arraytype(m, 2), 1, &pair_size));
    for (i = 0; i < 16; i++) elems[i] = wasm_i64((int64_t)i * 7);

    WASM_CHECK_OK(t, wasm_array_new(m, 0, elems, 16, &array));
    held = wasm_ref_hold(&rt, array);
    for (i = 0; i < config.initial_gc_heap_size && wasm_test_gc_heap_fits(&rt, struct_size); i++)
        WASM_CHECK_OK(t, wasm_struct_new(m, 1, &null_ref, 1, &garbage));
    if (held != NULL) array = *held;
    wasm_ref_release(&rt, held);
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, &array, 1, &outer));
    WASM_CHECK_OK(t, wasm_struct_get_field(m, outer, 0, &field));
    CHECK(t, field.of.gc_obj != array.of.gc_obj);
    CHECK(t, wasm_test_big_array_intact(m, field, 16));

    WASM_CHECK_OK(t, wasm_array_new(m, 0, elems, 16, &array));
    held = wasm_ref_hold(&rt, array);
    for (i = 0; i < config.initial_gc_heap_size && wasm_test_gc_heap_fits(&rt, pair_size); i++)
        WASM_CHECK_OK(t, wasm_array_new(m, 2, &null_ref, 1, &garbage));
    if (held != NULL) array = *held;
    wasm_ref_release(&rt, held);
    WASM_CHECK_OK(t, wasm_array_new(m, 2, &array, 1, &outer));
    WASM_CHECK_OK(t, wasm_array_get_elem(m, outer, 0, &field));
    CHECK(t, field.of.gc_obj != array.of.gc_obj);
    CHECK(t, wasm_test_big_array_intact(m, field, 16));

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_collects_unreachable_externref_boxes_and_exnrefs) {
    /* boxes(n) converts n i31refs to externref and keeps one boxed struct alive;
     * exns(n) catches n exceptions with catch_all_ref and drops each exnref. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0b, 0x03, 0x5f, 0x00, 0x60, 0x00, 0x00, 0x60, 0x01,
        0x7f, 0x01, 0x7f, 0x03, 0x03, 0x02, 0x02, 0x02, 0x0d, 0x03, 0x01, 0x00, 0x01, 0x07, 0x10, 0x02, 0x05, 0x62,
        0x6f, 0x78, 0x65, 0x73, 0x00, 0x00, 0x04, 0x65, 0x78, 0x6e, 0x73, 0x00, 0x01, 0x0a, 0x49, 0x02, 0x29, 0x01,
        0x01, 0x6f, 0xfb, 0x00, 0x00, 0xfb, 0x1b, 0x21, 0x01, 0x03, 0x40, 0x20, 0x00, 0xfb, 0x1c, 0xfb, 0x1b, 0x1a,
        0xfb, 0x00, 0x00, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0xfb, 0x1a,
        0xfb, 0x14, 0x00, 0x0b, 0x1d, 0x00, 0x03, 0x40, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x08, 0x00, 0x0b,
        0x00, 0x0b, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x41, 0x01, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(100000);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "boxes", &arg, 1, &result, 1));
    CHECK_MSG(t, result.of.i32 == 1, "%s", "expected the rooted box to still hold its struct");
    CHECK_MSG(t, rt.externref_boxes.count < 4096u, "expected unreachable boxes to be freed, %u live",
              (unsigned)rt.externref_boxes.count);

    WASM_CHECK_OK(t, wasm_call(m, "exns", &arg, 1, &result, 1));
    CHECK_MSG(t, rt.exns.live < 8192u, "expected unreachable exnrefs to be freed, %u live",
              (unsigned)rt.exns.live);

    wasm_gc_collect(&rt);
    CHECK(t, rt.externref_boxes.count == 0u);
    CHECK(t, rt.exns.live == 0u);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $inner (struct (field i32))) (type $outer (struct (field (ref null $inner))))
     (type $arr (array (mut (ref null $inner))))
     (func (export "make") (result anyref) (struct.new $outer (struct.new $inner (i32.const 42))))
     (func (export "make-array") (result anyref) (array.new $arr (struct.new $inner (i32.const 7)) (i32.const 3)))) */
static const uint8_t wasm_test_gc_owner_type_sections[] = {
    0x01, 0x12, 0x04, 0x5F, 0x01, 0x7F, 0x00, 0x5F, 0x01, 0x63, 0x00, 0x00, 0x5E, 0x63, 0x00, 0x01,
    0x60, 0x00, 0x01, 0x6E, 0x03, 0x03, 0x02, 0x03, 0x03, 0x07, 0x15, 0x02, 0x04, 0x6D, 0x61, 0x6B,
    0x65, 0x00, 0x00, 0x0A, 0x6D, 0x61, 0x6B, 0x65, 0x2D, 0x61, 0x72, 0x72, 0x61, 0x79, 0x00, 0x01,
    0x0A, 0x19, 0x02, 0x0A, 0x00, 0x41, 0x2A, 0xFB, 0x00, 0x00, 0xFB, 0x00, 0x01, 0x0B, 0x0C, 0x00,
    0x41, 0x07, 0xFB, 0x00, 0x00, 0x41, 0x03, 0xFB, 0x06, 0x02, 0x0B,
};

/* (module (type $inner (struct (field i32))) (type $outer (struct (field (ref null $inner))))
     (type $arr (array (mut (ref null $inner))))
     (global $g (mut anyref) (ref.null any)) (global $t (mut (ref null $arr)) (ref.null none))
     (func (export "store") (param anyref anyref)
       (global.set $g (local.get 0)) (global.set $t (ref.cast (ref null $arr) (local.get 1))))
     (func (export "read") (result i32)
       (struct.get $inner 0 (struct.get $outer 0 (ref.cast (ref $outer) (global.get $g)))))
     (func (export "read-array") (result i32) (struct.get $inner 0 (array.get $arr (global.get $t) (i32.const 2))))
     (func (export "garbage") (param i32)
       (loop $l (drop (array.new_default $arr (i32.const 16)))
         (br_if $l (local.tee 0 (i32.sub (local.get 0) (i32.const 1))))))) */
static const uint8_t wasm_test_gc_owner_holder_sections[] = {
    0x01, 0x1B, 0x06, 0x5F, 0x01, 0x7F, 0x00, 0x5F, 0x01, 0x63, 0x00, 0x00, 0x5E, 0x63, 0x00, 0x01,
    0x60, 0x02, 0x6E, 0x6E, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x60, 0x01, 0x7F, 0x00, 0x03, 0x05, 0x04,
    0x03, 0x04, 0x04, 0x05, 0x06, 0x0C, 0x02, 0x6E, 0x01, 0xD0, 0x6E, 0x0B, 0x63, 0x02, 0x01, 0xD0,
    0x71, 0x0B, 0x07, 0x27, 0x04, 0x05, 0x73, 0x74, 0x6F, 0x72, 0x65, 0x00, 0x00, 0x04, 0x72, 0x65,
    0x61, 0x64, 0x00, 0x01, 0x0A, 0x72, 0x65, 0x61, 0x64, 0x2D, 0x61, 0x72, 0x72, 0x61, 0x79, 0x00,
    0x02, 0x07, 0x67, 0x61, 0x72, 0x62, 0x61, 0x67, 0x65, 0x00, 0x03, 0x0A, 0x42, 0x04, 0x0D, 0x00,
    0x20, 0x00, 0x24, 0x00, 0x20, 0x01, 0xFB, 0x17, 0x02, 0x24, 0x01, 0x0B, 0x0F, 0x00, 0x23, 0x00,
    0xFB, 0x16, 0x01, 0xFB, 0x02, 0x01, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x0B, 0x0D, 0x00, 0x23, 0x01,
    0x41, 0x02, 0xFB, 0x0B, 0x02, 0xFB, 0x02, 0x00, 0x00, 0x0B, 0x14, 0x00, 0x03, 0x40, 0x41, 0x10,
    0xFB, 0x07, 0x02, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B,
};

TEST(test_gc_objects_outlive_their_type_module) {
    wasm_runtime_t rt;
    wasm_module_t* types;
    wasm_module_t* holder;
    wasm_value_t args[2];
    wasm_value_t* held = NULL;
    wasm_value_t field;
    wasm_value_t result = wasm_i32(0);
    wasm_value_t garbage = wasm_i32(200);

    wasm_init(&rt);
    types = wasm_test_load_sections(&rt, wasm_test_gc_owner_type_sections, sizeof(wasm_test_gc_owner_type_sections));
    holder = wasm_test_load_sections(&rt, wasm_test_gc_owner_holder_sections,
                                     sizeof(wasm_test_gc_owner_holder_sections));
    CHECK_MSG(t, types != NULL && holder != NULL, "%s", rt.error_msg);
    if (types == NULL || holder == NULL) {
        wasm_module_free(types);
        wasm_module_free(holder);
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(types, "make", NULL, 0, &args[0], 1));
    held = wasm_ref_hold(&rt, args[0]);
    WASM_CHECK_OK(t, wasm_call(types, "make-array", NULL, 0, &args[1], 1));
    if (held != NULL) args[0] = *held;
    WASM_CHECK_OK(t, wasm_call(holder, "store", args, 2, NULL, 0));
    wasm_module_free(types);
    CHECK(t, rt.type_owners != NULL);
    WASM_CHECK_OK(t, wasm_call(holder, "garbage", &garbage, 1, NULL, 0));
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_call(holder, "garbage", &garbage, 1, NULL, 0));
    wasm_gc_collect(&rt);

    WASM_CHECK_OK(t, wasm_call(holder, "read", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_OK(t, wasm_call(holder, "read-array", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 7);
    CHECK(t, held != NULL);
    if (held != NULL) {
        WASM_CHECK_OK(t, wasm_struct_get_field(holder, *held, 0, &field));
        WASM_CHECK_OK(t, wasm_struct_get_field(holder, field, 0, &result));
        WASM_CHECK_I32(t, result.of.i32, 42);
        wasm_ref_release(&rt, held);
    }
    wasm_gc_collect(&rt);
    CHECK(t, rt.type_owners != NULL);
    WASM_CHECK_OK(t, wasm_call(holder, "read", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);

    args[0] = wasm_ref_null(WASM_TYPE_ANYREF);
    args[1] = wasm_ref_null(WASM_TYPE_ANYREF);
    WASM_CHECK_OK(t, wasm_call(holder, "store", args, 2, NULL, 0));
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.type_owners == NULL, "%s", "expected the freed module's types to be released");

    types = wasm_test_load_sections(&rt, wasm_test_gc_owner_type_sections, sizeof(wasm_test_gc_owner_type_sections));
    CHECK_MSG(t, types != NULL, "%s", rt.error_msg);
    if (types != NULL) {
        WASM_CHECK_OK(t, wasm_call(types, "make", NULL, 0, &args[0], 1));
        held = wasm_ref_hold(&rt, args[0]);
        WASM_CHECK_OK(t, wasm_call(types, "make-array", NULL, 0, &args[1], 1));
        if (held != NULL) args[0] = *held;
        wasm_ref_release(&rt, held);
        WASM_CHECK_OK(t, wasm_call(holder, "store", args, 2, NULL, 0));
        g_wasm_test_alloc_fail_countdown = 1;
        wasm_module_free(types);
        CHECK(t, rt.type_owners != NULL);
        g_wasm_test_alloc_fail_countdown = 0;
        WASM_CHECK_OK(t, wasm_call(holder, "garbage", &garbage, 1, NULL, 0));
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_call(holder, "garbage", &garbage, 1, NULL, 0));
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_call(holder, "read", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 42);
        WASM_CHECK_OK(t, wasm_call(holder, "read-array", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 7);
        args[0] = wasm_ref_null(WASM_TYPE_ANYREF);
        args[1] = wasm_ref_null(WASM_TYPE_ANYREF);
        WASM_CHECK_OK(t, wasm_call(holder, "store", args, 2, NULL, 0));
        wasm_gc_collect(&rt);
        CHECK(t, rt.type_owners == NULL);
    }

    types = wasm_test_load_sections(&rt, wasm_test_gc_owner_type_sections, sizeof(wasm_test_gc_owner_type_sections));
    CHECK_MSG(t, types != NULL, "%s", rt.error_msg);
    if (types != NULL) {
        WASM_CHECK_OK(t, wasm_call(types, "make", NULL, 0, &args[0], 1));
        WASM_CHECK_OK(t, wasm_call(types, "make-array", NULL, 0, &args[1], 1));
        WASM_CHECK_OK(t, wasm_call(holder, "store", args, 2, NULL, 0));
        wasm_module_free(types);
        CHECK(t, rt.type_owners != NULL);
    }
    wasm_module_free(holder);
    wasm_destroy(&rt);
}

/* (rec (type $obj (descriptor $vt) (struct (field i32))) (type $vt (describes $obj) (struct (field i32))))
   (type $junk (array i8))
   (global $vt (mut (ref null (exact $vt))) (ref.null none))
   (global $obj (mut (ref null $obj)) (ref.null none))
   make-vt(x) sets $vt to (struct.new $vt x), make-obj sets $obj to (struct.new_desc $obj 1 $vt), drop-vt clears
   $vt, garbage(n) allocates n junk arrays, desc-id reads field 0 of $obj's descriptor, same-desc is
   (ref.eq (ref.get_desc $obj $obj) $vt), and obj returns $obj. */
static const uint8_t wasm_test_gc_desc_sections[] = {
    0x01, 0x21, 0x06, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x4C, 0x00, 0x5F, 0x01, 0x7F,
    0x00, 0x5E, 0x78, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x60,
    0x00, 0x01, 0x6E, 0x03, 0x08, 0x07, 0x03, 0x04, 0x04, 0x03, 0x05, 0x05, 0x06, 0x06, 0x0E, 0x02,
    0x63, 0x62, 0x01, 0x01, 0xD0, 0x71, 0x0B, 0x63, 0x00, 0x01, 0xD0, 0x71, 0x0B, 0x07, 0x46, 0x07,
    0x07, 0x6D, 0x61, 0x6B, 0x65, 0x2D, 0x76, 0x74, 0x00, 0x00, 0x08, 0x6D, 0x61, 0x6B, 0x65, 0x2D,
    0x6F, 0x62, 0x6A, 0x00, 0x01, 0x07, 0x64, 0x72, 0x6F, 0x70, 0x2D, 0x76, 0x74, 0x00, 0x02, 0x07,
    0x67, 0x61, 0x72, 0x62, 0x61, 0x67, 0x65, 0x00, 0x03, 0x07, 0x64, 0x65, 0x73, 0x63, 0x2D, 0x69,
    0x64, 0x00, 0x04, 0x09, 0x73, 0x61, 0x6D, 0x65, 0x2D, 0x64, 0x65, 0x73, 0x63, 0x00, 0x05, 0x03,
    0x6F, 0x62, 0x6A, 0x00, 0x06, 0x0A, 0x50, 0x07, 0x09, 0x00, 0x20, 0x00, 0xFB, 0x00, 0x01, 0x24,
    0x00, 0x0B, 0x0B, 0x00, 0x41, 0x01, 0x23, 0x00, 0xFB, 0x20, 0x00, 0x24, 0x01, 0x0B, 0x06, 0x00,
    0xD0, 0x71, 0x24, 0x00, 0x0B, 0x15, 0x00, 0x03, 0x40, 0x41, 0xC0, 0x00, 0xFB, 0x07, 0x02, 0x1A,
    0x20, 0x00, 0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x0B, 0x00, 0x23, 0x01, 0xFB,
    0x22, 0x00, 0xFB, 0x02, 0x01, 0x00, 0x0B, 0x0A, 0x00, 0x23, 0x01, 0xFB, 0x22, 0x00, 0x23, 0x00,
    0xD3, 0x0B, 0x04, 0x00, 0x23, 0x01, 0x0B,
};

static int32_t wasm_test_desc_call(test_ctx* t, wasm_module_t* m, const char* name, const wasm_value_t* arg) {
    wasm_value_t result = wasm_i32(-1);
    wasm_error_t err = wasm_call(m, name, arg, arg ? 1u : 0u, &result, 1);

    CHECK_MSG(t, err == WASM_OK, "%s: %s", name, wasm_runtime_error_message(wasm_module_runtime(m)));
    return result.of.i32;
}

TEST(test_gc_descriptors_move_with_their_objects) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* other;
    wasm_value_t garbage = wasm_i32(50);
    wasm_value_t id = wasm_i32(7);
    wasm_value_t obj = wasm_ref_null(WASM_TYPE_ANYREF);
    wasm_value_t desc;
    wasm_value_t field = wasm_i32(0);
    wasm_value_t* held = NULL;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_gc_desc_sections, sizeof(wasm_test_gc_desc_sections));
    other = wasm_test_load_sections(&rt, wasm_test_gc_desc_sections, sizeof(wasm_test_gc_desc_sections));
    CHECK_MSG(t, m != NULL && other != NULL, "%s", rt.error_msg);
    if (m == NULL || other == NULL) {
        wasm_module_free(m);
        wasm_module_free(other);
        wasm_destroy(&rt);
        return;
    }
    (void)wasm_test_desc_call(t, m, "garbage", &garbage);
    WASM_CHECK_OK(t, wasm_call(m, "make-vt", &id, 1, NULL, 0));
    (void)wasm_test_desc_call(t, m, "garbage", &garbage);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_call(m, "make-obj", NULL, 0, NULL, 0));
    (void)wasm_test_desc_call(t, m, "garbage", &garbage);
    wasm_gc_collect(&rt);
    WASM_CHECK_I32(t, wasm_test_desc_call(t, m, "same-desc", NULL), 1);
    WASM_CHECK_I32(t, wasm_test_desc_call(t, m, "desc-id", NULL), 7);

    WASM_CHECK_OK(t, wasm_call(m, "drop-vt", NULL, 0, NULL, 0));
    (void)wasm_test_desc_call(t, m, "garbage", &garbage);
    wasm_gc_collect(&rt);
    WASM_CHECK_I32(t, wasm_test_desc_call(t, m, "desc-id", NULL), 7);

    WASM_CHECK_OK(t, wasm_call(m, "obj", NULL, 0, &obj, 1));
    held = wasm_ref_hold(&rt, obj);
    wasm_module_free(m);
    CHECK(t, rt.type_owners != NULL);
    (void)wasm_test_desc_call(t, other, "garbage", &garbage);
    wasm_gc_collect(&rt);
    wasm_gc_collect(&rt);
    CHECK(t, held != NULL);
    if (held != NULL) {
        WASM_CHECK_OK(t, wasm_struct_get_desc(other, *held, &desc));
        WASM_CHECK_OK(t, wasm_struct_get_field(other, desc, 0, &field));
        WASM_CHECK_I32(t, field.of.i32, 7);
        wasm_ref_release(&rt, held);
    }
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.type_owners == NULL, "%s", "expected the freed module's types to be released");
    wasm_module_free(other);
    wasm_destroy(&rt);
}

TEST(test_gc_host_api_allocates_and_reads_descriptors) {
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t field = wasm_i32(1);
    wasm_value_t id = wasm_i32(7);
    wasm_value_t vt = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t obj = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t desc = wasm_ref_null(WASM_TYPE_NONE);

    wasm_config_default(&config);
    config.initial_gc_heap_size = 0;
    wasm_init_config(&rt, &config);
    m = wasm_test_load_sections(&rt, wasm_test_gc_desc_sections, sizeof(wasm_test_gc_desc_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    CHECK(t, wasm_struct_new(m, 0, &field, 1, &obj) == WASM_ERR_TYPE_MISMATCH);
    CHECK_MSG(t, strstr(rt.error_msg, "requires a descriptor") != NULL, "%s", rt.error_msg);
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, &id, 1, &vt));
    CHECK(t, wasm_struct_new_desc(m, 1, &field, 1, vt, &obj) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_struct_new_desc(m, 0, &field, 1, wasm_ref_null(WASM_TYPE_NONE), &obj) == WASM_ERR_TYPE_MISMATCH);
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, &id, 1, &vt));
    WASM_CHECK_OK(t, wasm_struct_new_desc(m, 0, &field, 1, vt, &obj));
    WASM_CHECK_OK(t, wasm_struct_get_desc(m, obj, &desc));
    CHECK(t, desc.of.gc_obj != vt.of.gc_obj);
    WASM_CHECK_OK(t, wasm_struct_get_field(m, desc, 0, &field));
    WASM_CHECK_I32(t, field.of.i32, 7);
    CHECK(t, wasm_struct_new_desc(m, 0, &field, 1, obj, &vt) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_struct_get_desc(m, desc, &vt) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_struct_get_desc(m, wasm_ref_null(WASM_TYPE_STRUCTREF), &vt) == WASM_ERR_TRAP);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (rec (type $obj (descriptor $vt) (struct (field i32))) (type $vt (describes $obj) (struct)))
   (type $h (struct (field (ref $vt))))
   (func (export "take-ext") (param (ref extern)) (result i32) (i32.const 1))
   (func (export "take-any") (param (ref any)) (result i32) (i32.const 1)) */
static const uint8_t wasm_test_gc_bottom_sections[] = {
    0x01, 0x1E, 0x04, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x5F,
    0x01, 0x64, 0x01, 0x00, 0x60, 0x01, 0x64, 0x6F, 0x01, 0x7F, 0x60, 0x01, 0x64, 0x6E, 0x01, 0x7F,
    0x03, 0x03, 0x02, 0x03, 0x04, 0x07, 0x17, 0x02, 0x08, 0x74, 0x61, 0x6B, 0x65, 0x2D, 0x65, 0x78,
    0x74, 0x00, 0x00, 0x08, 0x74, 0x61, 0x6B, 0x65, 0x2D, 0x61, 0x6E, 0x79, 0x00, 0x01, 0x0A, 0x0B,
    0x02, 0x04, 0x00, 0x41, 0x01, 0x0B, 0x04, 0x00, 0x41, 0x01, 0x0B,
};

TEST(test_gc_host_values_of_bottom_types_must_be_null) {
    static uint64_t fake[16];
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t field = wasm_i32(1);
    wasm_value_t forged;
    wasm_value_t vt;
    wasm_value_t* held;
    wasm_value_t obj;
    wasm_value_t result;
    wasm_valtype_t bottoms[] = { WASM_TYPE_NONE, WASM_TYPE_NOEXTERN, WASM_TYPE_NOFUNC };
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_gc_bottom_sections, sizeof(wasm_test_gc_bottom_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(bottoms); i++) {
        memset(&forged, 0, sizeof(forged));
        forged.type = bottoms[i];
        forged.of.gc_ref = (uintptr_t)fake;
        CHECK(t, wasm_struct_new_desc(m, 0, &field, 1, forged, &obj) == WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_struct_new(m, 2, &forged, 1, &obj) == WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_call(m, "take-ext", &forged, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_call(m, "take-any", &forged, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    }
    forged = wasm_externref((uintptr_t)fake);
    WASM_CHECK_OK(t, wasm_call(m, "take-ext", &forged, 1, &result, 1));
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, NULL, 0, &vt));
    held = wasm_ref_hold(&rt, vt);
    CHECK(t, held != NULL);
    if (held != NULL) {
        WASM_CHECK_OK(t, wasm_struct_new_desc(m, 0, &field, 1, *held, &obj));
        WASM_CHECK_OK(t, wasm_struct_new(m, 2, held, 1, &obj));
        wasm_ref_release(&rt, held);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* Types 0 $leaf (struct (mut i32)); 1 $mixed with mutable fields i8 i64 i16 f32 (ref null $leaf) i8 v128 f64 i32
   anyref i16 externref i8 (ref null $leaf); 2 $obj (mut i16) (mut (ref null $leaf)) with descriptor 3 $vt
   (mut (ref null $leaf)); 4 to 13 mutable arrays of i8 i16 i32 i64 f32 f64 v128 (ref null $leaf) externref anyref. */
static const uint8_t wasm_test_gc_kinds_sections[] = {
    0x01, 0x56, 0x0D, 0x5F, 0x01, 0x7F, 0x01, 0x5F, 0x0E, 0x78, 0x01, 0x7E, 0x01, 0x77, 0x01, 0x7D,
    0x01, 0x63, 0x00, 0x01, 0x78, 0x01, 0x7B, 0x01, 0x7C, 0x01, 0x7F, 0x01, 0x6E, 0x01, 0x77, 0x01,
    0x6F, 0x01, 0x78, 0x01, 0x63, 0x00, 0x01, 0x4E, 0x02, 0x4D, 0x03, 0x5F, 0x02, 0x77, 0x01, 0x63,
    0x00, 0x01, 0x4C, 0x02, 0x5F, 0x01, 0x63, 0x00, 0x01, 0x5E, 0x78, 0x01, 0x5E, 0x77, 0x01, 0x5E,
    0x7F, 0x01, 0x5E, 0x7E, 0x01, 0x5E, 0x7D, 0x01, 0x5E, 0x7C, 0x01, 0x5E, 0x7B, 0x01, 0x5E, 0x63,
    0x00, 0x01, 0x5E, 0x6F, 0x01, 0x5E, 0x6E, 0x01,
};

enum { WASM_TEST_KIND_FIELDS = 14, WASM_TEST_KIND_ARRAYS = 10, WASM_TEST_KIND_ELEMS = 5 };

/* Returns a held $leaf whose field is value, or NULL. */
static wasm_value_t* wasm_test_kinds_leaf(test_ctx* t, wasm_module_t* m, int32_t value) {
    wasm_value_t field = wasm_i32(value);
    wasm_value_t ref = wasm_ref_null(WASM_TYPE_NONE);

    WASM_CHECK_OK(t, wasm_struct_new(m, 0, &field, 1, &ref));
    return wasm_ref_hold(wasm_module_runtime(m), ref);
}

static int32_t wasm_test_kinds_leaf_value(test_ctx* t, const wasm_module_t* m, wasm_value_t ref) {
    wasm_value_t field = wasm_i32(-1);

    WASM_CHECK_OK(t, wasm_struct_get_field(m, ref, 0, &field));
    return field.of.i32;
}

static int32_t wasm_test_kinds_base(uint32_t gen) {
    return gen ? 1000 : 200;
}

static int wasm_test_kinds_is_ref_field(uint32_t i) {
    return i == 4 || i == 9 || i == 13;
}

/* The value $mixed field i holds in generation gen; a ref field holds leaf, a $leaf of base + i. */
static wasm_value_t wasm_test_kinds_mixed_value(uint32_t gen, uint32_t i, const wasm_value_t* leaf) {
    int32_t base = wasm_test_kinds_base(gen);
    uint8_t bytes[16];
    uint32_t j;

    switch (i) {
        case 1:
            return wasm_i64((int64_t)base * INT64_C(0x100000001));
        case 2:
            return wasm_i32(base + 0xFF00);
        case 3:
            return wasm_f32((float)base + 0.25f);
        case 6:
            for (j = 0; j < 16; j++) bytes[j] = (uint8_t)(base + 6 + (int32_t)j);
            return wasm_v128(bytes);
        case 7:
            return wasm_f64((double)base * 1.5);
        case 8:
            return wasm_i32(-(base + 8));
        case 9:
            return gen ? wasm_i31ref(base + 9) : *leaf;
        case 11:
            return wasm_externref((uintptr_t)0x7000u + (uintptr_t)base * 16u);
        case 13:
            return gen ? wasm_ref_null(WASM_TYPE_NONE) : *leaf;
        case 4:
            return *leaf;
        default:
            return wasm_i32(base + (int32_t)i);
    }
}

static void wasm_test_kinds_check_mixed(test_ctx* t, const wasm_module_t* m, wasm_value_t obj, uint32_t gen) {
    int32_t base = wasm_test_kinds_base(gen);
    uint32_t i;

    for (i = 0; i < WASM_TEST_KIND_FIELDS; i++) {
        wasm_value_t got = wasm_i32(0);
        wasm_value_t want = wasm_test_kinds_mixed_value(gen, i, &got);

        WASM_CHECK_OK(t, wasm_struct_get_field(m, obj, i, &got));
        switch (i) {
            case 0:
            case 5:
            case 12:
                WASM_CHECK_I32(t, got.of.i32, (base + (int32_t)i) & 0xFF);
                break;
            case 2:
            case 10:
                WASM_CHECK_I32(t, got.of.i32, want.of.i32 & 0xFFFF);
                break;
            case 1:
                WASM_CHECK_I64(t, got.of.i64, want.of.i64);
                break;
            case 3:
                CHECK(t, got.type == WASM_TYPE_F32 && got.of.f32 == want.of.f32);
                break;
            case 6:
                CHECK(t, got.type == WASM_TYPE_V128 && memcmp(got.of.v128, want.of.v128, 16) == 0);
                break;
            case 7:
                CHECK(t, got.type == WASM_TYPE_F64 && got.of.f64 == want.of.f64);
                break;
            case 8:
                WASM_CHECK_I32(t, got.of.i32, want.of.i32);
                break;
            case 9:
                if (gen) {
                    CHECK(t, (got.of.gc_ref & 1u) != 0 && wasm_test_gc_i31_get_s(got.of.gc_ref) == base + 9);
                } else {
                    WASM_CHECK_I32(t, wasm_test_kinds_leaf_value(t, m, got), base + 9);
                }
                break;
            case 11:
                CHECK(t, got.type == WASM_TYPE_EXTERNREF && got.of.externref == want.of.externref);
                break;
            case 13:
                if (gen) {
                    CHECK(t, got.of.gc_ref == 0);
                    break;
                }
                WASM_CHECK_I32(t, wasm_test_kinds_leaf_value(t, m, got), base + 13);
                break;
            default:
                WASM_CHECK_I32(t, wasm_test_kinds_leaf_value(t, m, got), base + (int32_t)i);
                break;
        }
    }
}

/* Returns a held $mixed of generation 0, or NULL. */
static wasm_value_t* wasm_test_kinds_new_mixed(test_ctx* t, wasm_module_t* m) {
    wasm_value_t* leaves[WASM_TEST_KIND_FIELDS];
    wasm_value_t values[WASM_TEST_KIND_FIELDS];
    wasm_value_t ref = wasm_ref_null(WASM_TYPE_NONE);
    uint32_t i;

    memset(leaves, 0, sizeof(leaves));
    for (i = 0; i < WASM_TEST_KIND_FIELDS; i++) {
        if (wasm_test_kinds_is_ref_field(i)) leaves[i] = wasm_test_kinds_leaf(t, m, wasm_test_kinds_base(0) + (int32_t)i);
    }
    for (i = 0; i < WASM_TEST_KIND_FIELDS; i++) {
        wasm_value_t null_leaf = wasm_ref_null(WASM_TYPE_NONE);

        values[i] = wasm_test_kinds_mixed_value(0, i, leaves[i] ? leaves[i] : &null_leaf);
    }
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, values, WASM_TEST_KIND_FIELDS, &ref));
    for (i = 0; i < WASM_TEST_KIND_FIELDS; i++) wasm_ref_release(wasm_module_runtime(m), leaves[i]);
    return wasm_ref_hold(wasm_module_runtime(m), ref);
}

static void wasm_test_kinds_rewrite_mixed(test_ctx* t, wasm_module_t* m, const wasm_value_t* obj) {
    uint32_t i;

    for (i = 0; i < WASM_TEST_KIND_FIELDS; i++) {
        wasm_value_t* leaf = wasm_test_kinds_is_ref_field(i)
                                 ? wasm_test_kinds_leaf(t, m, wasm_test_kinds_base(1) + (int32_t)i)
                                 : NULL;
        wasm_value_t null_leaf = wasm_ref_null(WASM_TYPE_NONE);

        WASM_CHECK_OK(t, wasm_struct_set_field(m, *obj, i, wasm_test_kinds_mixed_value(1, i, leaf ? leaf : &null_leaf)));
        wasm_ref_release(wasm_module_runtime(m), leaf);
    }
}

static int wasm_test_kinds_is_ref_elem(uint32_t kind, uint32_t j, uint32_t gen) {
    return (kind == 7 && !(gen && j == 2)) || (kind == 9 && j % 3u == 1u);
}

static int32_t wasm_test_kinds_leaf_elem(uint32_t kind, uint32_t j, uint32_t gen) {
    return (kind == 7 ? 300 : 400) + (int32_t)j + (int32_t)gen * 50;
}

/* The value element j of array type 4 + kind holds in generation gen; a ref element holds leaf. */
static wasm_value_t wasm_test_kinds_elem_value(uint32_t kind, uint32_t j, uint32_t gen, const wasm_value_t* leaf) {
    int32_t n = (int32_t)j + (int32_t)gen * 3;
    uint8_t bytes[16];
    wasm_value_t value;
    uint32_t b;

    switch (kind) {
        case 0:
            return wasm_i32(250 + n);
        case 1:
            return wasm_i32(65530 + n);
        case 2:
            return wasm_i32(-7 * (n + 1));
        case 3:
            return wasm_i64((int64_t)(n + 1) * INT64_C(0x100000001));
        case 4:
            return wasm_f32((float)n + 0.5f);
        case 5:
            return wasm_f64((double)n * 1.25);
        case 6:
            for (b = 0; b < 16; b++) bytes[b] = (uint8_t)((int32_t)j * 16 + (int32_t)b + (int32_t)gen);
            return wasm_v128(bytes);
        case 7:
            return gen && j == 2 ? wasm_ref_null(WASM_TYPE_NONE) : *leaf;
        case 8:
            return wasm_externref((uintptr_t)0x1000u + (uintptr_t)(n + 1) * 16u);
        default:
            if (j % 3u == 0u) return wasm_i31ref(n);
            if (j % 3u == 1u) return *leaf;
            value = wasm_ref_null(WASM_TYPE_ANYREF);
            value.of.gc_ref = (uintptr_t)0x20000u + (uintptr_t)(n + 1) * 16u;
            return value;
    }
}

static void wasm_test_kinds_check_array(test_ctx* t, const wasm_module_t* m, wasm_value_t array, uint32_t kind,
                                        uint32_t gen) {
    uint32_t length = 0;
    uint32_t j;

    WASM_CHECK_OK(t, wasm_array_length(m, array, &length));
    WASM_CHECK_I32(t, (int32_t)length, (int32_t)WASM_TEST_KIND_ELEMS);
    for (j = 0; j < WASM_TEST_KIND_ELEMS; j++) {
        wasm_value_t got = wasm_i32(0);
        wasm_value_t want = wasm_test_kinds_elem_value(kind, j, gen, &got);

        WASM_CHECK_OK(t, wasm_array_get_elem(m, array, j, &got));
        if (wasm_test_kinds_is_ref_elem(kind, j, gen)) {
            WASM_CHECK_I32(t, wasm_test_kinds_leaf_value(t, m, got), wasm_test_kinds_leaf_elem(kind, j, gen));
            continue;
        }
        switch (kind) {
            case 0:
                WASM_CHECK_I32(t, got.of.i32, want.of.i32 & 0xFF);
                break;
            case 1:
                WASM_CHECK_I32(t, got.of.i32, want.of.i32 & 0xFFFF);
                break;
            case 2:
                WASM_CHECK_I32(t, got.of.i32, want.of.i32);
                break;
            case 3:
                WASM_CHECK_I64(t, got.of.i64, want.of.i64);
                break;
            case 4:
                CHECK(t, got.type == WASM_TYPE_F32 && got.of.f32 == want.of.f32);
                break;
            case 5:
                CHECK(t, got.type == WASM_TYPE_F64 && got.of.f64 == want.of.f64);
                break;
            case 6:
                CHECK(t, got.type == WASM_TYPE_V128 && memcmp(got.of.v128, want.of.v128, 16) == 0);
                break;
            case 7:
                CHECK(t, got.of.gc_ref == 0);
                break;
            case 8:
                CHECK(t, got.type == WASM_TYPE_EXTERNREF && got.of.externref == want.of.externref);
                break;
            default:
                CHECK(t, got.of.gc_ref == want.of.gc_ref);
                break;
        }
    }
}

/* Returns a held array of type 4 + kind of generation 0, or NULL. */
static wasm_value_t* wasm_test_kinds_new_array(test_ctx* t, wasm_module_t* m, uint32_t kind) {
    wasm_value_t* leaves[WASM_TEST_KIND_ELEMS];
    wasm_value_t values[WASM_TEST_KIND_ELEMS];
    wasm_value_t ref = wasm_ref_null(WASM_TYPE_NONE);
    uint32_t j;

    memset(leaves, 0, sizeof(leaves));
    for (j = 0; j < WASM_TEST_KIND_ELEMS; j++) {
        if (wasm_test_kinds_is_ref_elem(kind, j, 0))
            leaves[j] = wasm_test_kinds_leaf(t, m, wasm_test_kinds_leaf_elem(kind, j, 0));
    }
    for (j = 0; j < WASM_TEST_KIND_ELEMS; j++) {
        wasm_value_t null_leaf = wasm_ref_null(WASM_TYPE_NONE);

        values[j] = wasm_test_kinds_elem_value(kind, j, 0, leaves[j] ? leaves[j] : &null_leaf);
    }
    WASM_CHECK_OK(t, wasm_array_new(m, 4 + kind, values, WASM_TEST_KIND_ELEMS, &ref));
    for (j = 0; j < WASM_TEST_KIND_ELEMS; j++) wasm_ref_release(wasm_module_runtime(m), leaves[j]);
    return wasm_ref_hold(wasm_module_runtime(m), ref);
}

static void wasm_test_kinds_rewrite_array(test_ctx* t, wasm_module_t* m, const wasm_value_t* array, uint32_t kind) {
    uint32_t j;

    for (j = 0; j < WASM_TEST_KIND_ELEMS; j++) {
        wasm_value_t* leaf =
            wasm_test_kinds_is_ref_elem(kind, j, 1) ? wasm_test_kinds_leaf(t, m, wasm_test_kinds_leaf_elem(kind, j, 1)) : NULL;
        wasm_value_t null_leaf = wasm_ref_null(WASM_TYPE_NONE);

        WASM_CHECK_OK(t, wasm_array_set_elem(m, *array, j, wasm_test_kinds_elem_value(kind, j, 1, leaf ? leaf : &null_leaf)));
        wasm_ref_release(wasm_module_runtime(m), leaf);
    }
}

/* Returns a held $obj of generation 0, whose descriptor $vt holds a $leaf of 500. */
static wasm_value_t* wasm_test_kinds_new_described(test_ctx* t, wasm_module_t* m) {
    wasm_value_t* leaf = wasm_test_kinds_leaf(t, m, 500);
    wasm_value_t* vt = NULL;
    wasm_value_t values[2];
    wasm_value_t ref = wasm_ref_null(WASM_TYPE_NONE);

    if (leaf) {
        WASM_CHECK_OK(t, wasm_struct_new(m, 3, leaf, 1, &ref));
        vt = wasm_ref_hold(wasm_module_runtime(m), ref);
    }
    wasm_ref_release(wasm_module_runtime(m), leaf);
    leaf = wasm_test_kinds_leaf(t, m, 501);
    if (vt && leaf) {
        values[0] = wasm_i32(0x1BEEF);
        values[1] = *leaf;
        WASM_CHECK_OK(t, wasm_struct_new_desc(m, 2, values, 2, *vt, &ref));
    }
    wasm_ref_release(wasm_module_runtime(m), leaf);
    wasm_ref_release(wasm_module_runtime(m), vt);
    return wasm_ref_hold(wasm_module_runtime(m), ref);
}

static void wasm_test_kinds_check_described(test_ctx* t, const wasm_module_t* m, wasm_value_t obj, uint32_t gen) {
    wasm_value_t got = wasm_i32(0);
    wasm_value_t desc = wasm_ref_null(WASM_TYPE_NONE);

    WASM_CHECK_OK(t, wasm_struct_get_field(m, obj, 0, &got));
    WASM_CHECK_I32(t, got.of.i32, gen ? 0x1234 : 0xBEEF);
    WASM_CHECK_OK(t, wasm_struct_get_field(m, obj, 1, &got));
    WASM_CHECK_I32(t, wasm_test_kinds_leaf_value(t, m, got), gen ? 601 : 501);
    WASM_CHECK_OK(t, wasm_struct_get_desc(m, obj, &desc));
    WASM_CHECK_OK(t, wasm_struct_get_field(m, desc, 0, &got));
    WASM_CHECK_I32(t, wasm_test_kinds_leaf_value(t, m, got), gen ? 600 : 500);
}

static void wasm_test_kinds_rewrite_described(test_ctx* t, wasm_module_t* m, const wasm_value_t* obj) {
    wasm_value_t* leaf = wasm_test_kinds_leaf(t, m, 600);
    wasm_value_t desc = wasm_ref_null(WASM_TYPE_NONE);

    WASM_CHECK_OK(t, wasm_struct_get_desc(m, *obj, &desc));
    if (leaf) WASM_CHECK_OK(t, wasm_struct_set_field(m, desc, 0, *leaf));
    wasm_ref_release(wasm_module_runtime(m), leaf);
    leaf = wasm_test_kinds_leaf(t, m, 601);
    WASM_CHECK_OK(t, wasm_struct_set_field(m, *obj, 0, wasm_i32(0x1234)));
    if (leaf) WASM_CHECK_OK(t, wasm_struct_set_field(m, *obj, 1, *leaf));
    wasm_ref_release(wasm_module_runtime(m), leaf);
}

/* held[0] is the $mixed, held[1] the $obj, and held[2 + kind] the array of type 4 + kind. */
static void wasm_test_kinds_check_all(test_ctx* t, const wasm_module_t* m, wasm_value_t* const* held, uint32_t gen) {
    uint32_t kind;

    wasm_test_kinds_check_mixed(t, m, *held[0], gen);
    wasm_test_kinds_check_described(t, m, *held[1], gen);
    for (kind = 0; kind < WASM_TEST_KIND_ARRAYS; kind++) wasm_test_kinds_check_array(t, m, *held[2 + kind], kind, gen);
}

static void wasm_test_kinds_garbage(test_ctx* t, wasm_module_t* m, uint32_t count) {
    wasm_value_t elems[WASM_TEST_KIND_ELEMS];
    wasm_value_t field = wasm_i32(-1);
    wasm_value_t ref;
    uint32_t i;

    for (i = 0; i < WASM_TEST_KIND_ELEMS; i++) elems[i] = wasm_i64((int64_t)i);
    for (i = 0; i < count; i++) {
        WASM_CHECK_OK(t, wasm_array_new(m, 7, elems, WASM_TEST_KIND_ELEMS, &ref));
        WASM_CHECK_OK(t, wasm_struct_new(m, 0, &field, 1, &ref));
    }
}

TEST(test_gc_objects_of_every_field_kind_survive_their_module) {
    wasm_runtime_t rt;
    wasm_module_t* types;
    wasm_module_t* other;
    wasm_value_t* held[2 + WASM_TEST_KIND_ARRAYS];
    uint32_t kind;
    uint32_t i;
    int all_held = 1;

    wasm_init(&rt);
    types = wasm_test_load_sections(&rt, wasm_test_gc_kinds_sections, sizeof(wasm_test_gc_kinds_sections));
    other = wasm_test_load_sections(&rt, wasm_test_gc_kinds_sections, sizeof(wasm_test_gc_kinds_sections));
    CHECK_MSG(t, types != NULL && other != NULL, "%s", rt.error_msg);
    if (types == NULL || other == NULL) {
        wasm_module_free(types);
        wasm_module_free(other);
        wasm_destroy(&rt);
        return;
    }
    held[0] = wasm_test_kinds_new_mixed(t, types);
    held[1] = wasm_test_kinds_new_described(t, types);
    for (kind = 0; kind < WASM_TEST_KIND_ARRAYS; kind++) held[2 + kind] = wasm_test_kinds_new_array(t, types, kind);
    for (i = 0; i < 2 + WASM_TEST_KIND_ARRAYS; i++) all_held = all_held && held[i] != NULL;
    CHECK(t, all_held);
    if (all_held) {
        wasm_test_kinds_check_all(t, types, held, 0);
        g_wasm_test_alloc_fail_countdown = 1;
        wasm_module_free(types);
        g_wasm_test_alloc_fail_countdown = 0;
        CHECK(t, rt.type_owners != NULL);
        wasm_test_kinds_garbage(t, other, 64);
        wasm_gc_collect(&rt);
        wasm_test_kinds_check_all(t, other, held, 0);

        wasm_test_kinds_rewrite_mixed(t, other, held[0]);
        wasm_test_kinds_rewrite_described(t, other, held[1]);
        for (kind = 0; kind < WASM_TEST_KIND_ARRAYS; kind++) wasm_test_kinds_rewrite_array(t, other, held[2 + kind], kind);
        wasm_test_kinds_garbage(t, other, 64);
        wasm_gc_collect(&rt);
        wasm_test_kinds_check_all(t, other, held, 1);
        wasm_gc_collect(&rt);
        wasm_test_kinds_check_all(t, other, held, 1);
    } else {
        wasm_module_free(types);
    }
    for (i = 0; i < 2 + WASM_TEST_KIND_ARRAYS; i++) wasm_ref_release(&rt, held[i]);
    wasm_gc_collect(&rt);
    CHECK(t, rt.type_owners == NULL);
    wasm_module_free(other);
    wasm_destroy(&rt);
}

/* (module (type $f (func (result i32)))
     (type $s (struct (field (mut funcref)) (field (mut (ref null $f))) (field (mut f32)) (field (mut f64))
       (field (mut i64)) (field (mut i32)) (field (mut externref)) (field (mut anyref)) (field (mut i16))))
     (type $a (array (mut funcref)))
     (func $seven (type $f) (i32.const 7)) (elem declare func $seven)
     (func (export "seven") (result (ref $f)) (ref.func $seven))) */
static const uint8_t wasm_test_gc_roundtrip_sections[] = {
    0x01, 0x22, 0x04, 0x60, 0x00, 0x01, 0x7F, 0x5F, 0x09, 0x70, 0x01, 0x63, 0x00, 0x01, 0x7D, 0x01,
    0x7C, 0x01, 0x7E, 0x01, 0x7F, 0x01, 0x6F, 0x01, 0x6E, 0x01, 0x77, 0x01, 0x5E, 0x70, 0x01, 0x60,
    0x00, 0x01, 0x64, 0x00, 0x03, 0x03, 0x02, 0x00, 0x03, 0x07, 0x09, 0x01, 0x05, 0x73, 0x65, 0x76,
    0x65, 0x6E, 0x00, 0x01, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x0B, 0x02, 0x04, 0x00,
    0x41, 0x07, 0x0B, 0x04, 0x00, 0xD2, 0x00, 0x0B,
};

TEST(test_gc_host_field_accessors_round_trip_every_kind) {
    static const wasm_valtype_t tags[] = { WASM_TYPE_FUNCREF, WASM_TYPE_FUNCREF, WASM_TYPE_F32,
                                           WASM_TYPE_F64,     WASM_TYPE_I64,     WASM_TYPE_I32,
                                           WASM_TYPE_EXTERNREF, WASM_TYPE_ANYREF, WASM_TYPE_I32 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t values[9];
    wasm_value_t elems[2];
    wasm_value_t got;
    wasm_value_t seven = wasm_ref_null(WASM_TYPE_FUNCREF);
    wasm_value_t ref = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t* obj = NULL;
    wasm_value_t* array = NULL;
    uint32_t f32_bits = 0x7FA00001u;
    uint64_t f64_bits = UINT64_C(0x7FF4000000000001);
    uint32_t bits32;
    uint64_t bits64;
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_gc_roundtrip_sections, sizeof(wasm_test_gc_roundtrip_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "seven", NULL, 0, &seven, 1));
    values[0] = seven;
    values[1] = seven;
    values[2] = wasm_f32(0.0f);
    memcpy(&values[2].of.f32, &f32_bits, sizeof(f32_bits));
    values[3] = wasm_f64(0.0);
    memcpy(&values[3].of.f64, &f64_bits, sizeof(f64_bits));
    values[4] = wasm_i64(INT64_MIN + 1);
    values[5] = wasm_i32(-2);
    values[6] = wasm_externref((uintptr_t)0x1235u);
    values[7] = wasm_ref_null(WASM_TYPE_ANYREF);
    values[7].of.gc_ref = (uintptr_t)0x30000u;
    values[8] = wasm_i32(0x18001);
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, values, 9, &ref));
    obj = wasm_ref_hold(&rt, ref);
    elems[0] = wasm_ref_null(WASM_TYPE_FUNCREF);
    elems[1] = seven;
    WASM_CHECK_OK(t, wasm_array_new(m, 2, elems, 2, &ref));
    array = wasm_ref_hold(&rt, ref);
    CHECK(t, obj != NULL && array != NULL);
    if (obj != NULL && array != NULL) {
        wasm_gc_collect(&rt);
        for (i = 0; i < 9; i++) {
            WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, i, &got));
            CHECK_MSG(t, got.type == tags[i], "field %u: type 0x%X", (unsigned)i, (unsigned)got.type);
        }
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 1, &got));
        CHECK(t, got.of.funcref == seven.of.funcref && seven.of.funcref != UINT32_MAX);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 2, &got));
        memcpy(&bits32, &got.of.f32, sizeof(bits32));
        CHECK(t, bits32 == f32_bits);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 3, &got));
        memcpy(&bits64, &got.of.f64, sizeof(bits64));
        CHECK(t, bits64 == f64_bits);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 4, &got));
        WASM_CHECK_I64(t, got.of.i64, INT64_MIN + 1);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 5, &got));
        WASM_CHECK_I32(t, got.of.i32, -2);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 6, &got));
        CHECK(t, got.of.externref == (uintptr_t)0x1235u);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 7, &got));
        CHECK(t, got.of.gc_ref == (uintptr_t)0x30000u);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 8, &got));
        WASM_CHECK_I32(t, got.of.i32, 0x8001);

        WASM_CHECK_OK(t, wasm_struct_set_field(m, *obj, 0, wasm_ref_null(WASM_TYPE_NOFUNC)));
        WASM_CHECK_OK(t, wasm_struct_set_field(m, *obj, 7, wasm_i31ref(-3)));
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 0, &got));
        CHECK(t, got.type == WASM_TYPE_FUNCREF && got.of.funcref == UINT32_MAX);
        WASM_CHECK_OK(t, wasm_struct_get_field(m, *obj, 7, &got));
        CHECK(t, got.type == WASM_TYPE_ANYREF && got.of.gc_ref == wasm_i31ref(-3).of.gc_ref);

        WASM_CHECK_OK(t, wasm_array_get_elem(m, *array, 0, &got));
        CHECK(t, got.type == WASM_TYPE_FUNCREF && got.of.funcref == UINT32_MAX);
        WASM_CHECK_OK(t, wasm_array_get_elem(m, *array, 1, &got));
        CHECK(t, got.type == WASM_TYPE_FUNCREF && got.of.funcref == seven.of.funcref);
    }
    wasm_ref_release(&rt, obj);
    wasm_ref_release(&rt, array);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module
     (type $box (struct (field i32)))
     (type $f1 (func (param i32) (result i32))) (type $c1 (cont $f1))
     (tag $y (param i32) (result i32))
     (global $k (mut (ref null $c1)) (ref.null $c1))
     (func $field_plus (param $b (ref null $box)) (param $v i32) (result i32)
       (i32.add (struct.get $box 0 (local.get $b)) (local.get $v)))
     (func $holder (type $f1) (param $x i32) (result i32) (local $b (ref null $box))
       (local.set $b (struct.new $box (local.get $x)))
       (i32.add (struct.get $box 0 (local.get $b))
         (call $field_plus (struct.new $box (i32.mul (local.get $x) (i32.const 2)))
                           (suspend $y (i32.const 0)))))
     (elem declare func $holder)
     (func (export "start") (param $x i32)
       (block $h (result i32 (ref $c1))
         (drop (resume $c1 (on $y $h) (local.get $x) (cont.new $c1 (ref.func $holder))))
         (return))
       (global.set $k) (drop))
     (func (export "finish") (param $v i32) (result i32) (resume $c1 (local.get $v) (global.get $k)))
     (func (export "garbage") (param $n i32)
       (loop $l (drop (struct.new $box (local.get $n)))
         (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))
     (func (export "abandon") (param $n i32)
       (loop $l
         (block $h (result i32 (ref $c1))
           (drop (resume $c1 (on $y $h) (local.get $n) (cont.new $c1 (ref.func $holder))))
           (unreachable))
         (drop) (drop)
         (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))) */
static const uint8_t wasm_test_gc_suspended_sections[] = {
    0x01, 0x1D, 0x06, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x01, 0x60, 0x02,
    0x63, 0x00, 0x7F, 0x01, 0x7F, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x02, 0x7F, 0x64, 0x02, 0x03,
    0x07, 0x06, 0x03, 0x01, 0x04, 0x01, 0x04, 0x04, 0x0D, 0x03, 0x01, 0x00, 0x01, 0x06, 0x07, 0x01,
    0x63, 0x02, 0x01, 0xD0, 0x02, 0x0B, 0x07, 0x26, 0x04, 0x05, 0x73, 0x74, 0x61, 0x72, 0x74, 0x00,
    0x02, 0x06, 0x66, 0x69, 0x6E, 0x69, 0x73, 0x68, 0x00, 0x03, 0x07, 0x67, 0x61, 0x72, 0x62, 0x61,
    0x67, 0x65, 0x00, 0x04, 0x07, 0x61, 0x62, 0x61, 0x6E, 0x64, 0x6F, 0x6E, 0x00, 0x05, 0x09, 0x05,
    0x01, 0x03, 0x00, 0x01, 0x01, 0x0A, 0x87, 0x01, 0x06, 0x0B, 0x00, 0x20, 0x00, 0xFB, 0x02, 0x00,
    0x00, 0x20, 0x01, 0x6A, 0x0B, 0x21, 0x01, 0x01, 0x63, 0x00, 0x20, 0x00, 0xFB, 0x00, 0x00, 0x21,
    0x01, 0x20, 0x01, 0xFB, 0x02, 0x00, 0x00, 0x20, 0x00, 0x41, 0x02, 0x6C, 0xFB, 0x00, 0x00, 0x41,
    0x00, 0xE2, 0x00, 0x10, 0x00, 0x6A, 0x0B, 0x16, 0x00, 0x02, 0x05, 0x20, 0x00, 0xD2, 0x01, 0xE0,
    0x02, 0xE3, 0x02, 0x01, 0x00, 0x00, 0x00, 0x1A, 0x0F, 0x0B, 0x24, 0x00, 0x1A, 0x0B, 0x09, 0x00,
    0x20, 0x00, 0x23, 0x00, 0xE3, 0x02, 0x00, 0x0B, 0x14, 0x00, 0x03, 0x40, 0x20, 0x00, 0xFB, 0x00,
    0x00, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x21, 0x00, 0x03,
    0x40, 0x02, 0x05, 0x20, 0x00, 0xD2, 0x01, 0xE0, 0x02, 0xE3, 0x02, 0x01, 0x00, 0x00, 0x00, 0x1A,
    0x00, 0x0B, 0x1A, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B,
};
TEST(test_gc_traces_references_only_suspended_frames_hold) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    uint32_t i;

    wasm_config_default(&cfg);
    cfg.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_sections(&rt, wasm_test_gc_suspended_sections, sizeof(wasm_test_gc_suspended_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "start", &arg, 1, NULL, 0));
    for (i = 0; i < 3; i++) {
        uint8_t* heap;

        arg = wasm_i32(200);
        WASM_CHECK_OK(t, wasm_call(m, "garbage", &arg, 1, NULL, 0));
        heap = rt.gc_heap;
        wasm_gc_collect(&rt);
        CHECK_MSG(t, rt.gc_heap != heap, "%s", "expected the collection to move the suspended frames' objects");
    }
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call(m, "finish", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 35);
    CHECK(t, rt.num_cont_stacks == 0 && rt.conts.live == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_frees_abandoned_continuations_with_their_stacks) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    uint32_t count;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_gc_suspended_sections, sizeof(wasm_test_gc_suspended_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(50);
    WASM_CHECK_OK(t, wasm_call(m, "abandon", &arg, 1, NULL, 0));
#ifndef WASM_GC_STRESS
    CHECK_MSG(t, rt.num_cont_stacks == 50 && rt.conts.live == 50, "%u stacks, %u continuations",
              (unsigned)rt.num_cont_stacks, (unsigned)rt.conts.live);
#endif
    count = rt.conts.count;
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0 && rt.conts.live == 0 && rt.gc_heap_offset == 0,
              "%u stacks, %u continuations, %zu heap bytes left", (unsigned)rt.num_cont_stacks,
              (unsigned)rt.conts.live, rt.gc_heap_offset);
#ifndef WASM_GC_STRESS
    CHECK(t, rt.num_pooled_cont_stacks == 16);
#endif
    WASM_CHECK_OK(t, wasm_call(m, "abandon", &arg, 1, NULL, 0));
    CHECK_MSG(t, rt.conts.count == count, "expected freed entries to be reused, %u after %u", (unsigned)rt.conts.count,
              (unsigned)count);
    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_config_default(&cfg);
    cfg.cont_stack_values = 65536;
    cfg.cont_initial_stack_values = 65536;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_sections(&rt, wasm_test_gc_suspended_sections, sizeof(wasm_test_gc_suspended_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(100);
    WASM_CHECK_OK(t, wasm_call(m, "abandon", &arg, 1, NULL, 0));
#ifndef WASM_STACK_STRESS
    CHECK_MSG(t, rt.num_cont_stacks <= 8 && rt.conts.count <= 16, "%u stacks, %u entries",
              (unsigned)rt.num_cont_stacks, (unsigned)rt.conts.count);
#endif
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module
     (type $s (struct (field i32)))
     (type $f (func)) (type $c (cont $f))
     (import "host" "t" (table $t 1 anyref))
     (import "host" "k" (table $k 2 contref))
     (func $nop (type $f)) (elem declare func $nop)
     (func (export "store") (param $v i32)
       (table.set $t (i32.const 0) (struct.new $s (local.get $v)))
       (table.set $k (i32.const 0) (cont.new $c (ref.func $nop))))
     (func (export "consume") (local $x (ref null $c))
       (local.set $x (cont.new $c (ref.func $nop)))
       (table.set $k (i32.const 1) (local.get $x))
       (resume $c (local.get $x)))
     (func (export "garbage") (param $n i32)
       (loop $l (drop (struct.new $s (local.get $n)))
         (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))
     (func (export "load") (result i32) (struct.get $s 0 (ref.cast (ref $s) (table.get $t (i32.const 0)))))
     (func (export "get") (result contref) (table.get $k (i32.const 0)))
     (func (export "run") (param $k (ref null $c)) (resume $c (local.get $k)))) */
static const uint8_t wasm_test_gc_host_table_sections[] = {
    0x01, 0x1B, 0x07, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x00, 0x5D, 0x01, 0x60, 0x01, 0x7F, 0x00,
    0x60, 0x00, 0x01, 0x7F, 0x60, 0x00, 0x01, 0x68, 0x60, 0x01, 0x63, 0x02, 0x00, 0x02, 0x17, 0x02,
    0x04, 0x68, 0x6F, 0x73, 0x74, 0x01, 0x74, 0x01, 0x6E, 0x00, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74,
    0x01, 0x6B, 0x01, 0x68, 0x00, 0x02, 0x03, 0x08, 0x07, 0x01, 0x03, 0x01, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x30, 0x06, 0x05, 0x73, 0x74, 0x6F, 0x72, 0x65, 0x00, 0x01, 0x07, 0x63, 0x6F, 0x6E, 0x73,
    0x75, 0x6D, 0x65, 0x00, 0x02, 0x07, 0x67, 0x61, 0x72, 0x62, 0x61, 0x67, 0x65, 0x00, 0x03, 0x04,
    0x6C, 0x6F, 0x61, 0x64, 0x00, 0x04, 0x03, 0x67, 0x65, 0x74, 0x00, 0x05, 0x03, 0x72, 0x75, 0x6E,
    0x00, 0x06, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x61, 0x07, 0x02, 0x00, 0x0B, 0x13,
    0x00, 0x41, 0x00, 0x20, 0x00, 0xFB, 0x00, 0x00, 0x26, 0x00, 0x41, 0x00, 0xD2, 0x00, 0xE0, 0x02,
    0x26, 0x01, 0x0B, 0x16, 0x01, 0x01, 0x63, 0x02, 0xD2, 0x00, 0xE0, 0x02, 0x21, 0x00, 0x41, 0x01,
    0x20, 0x00, 0x26, 0x01, 0x20, 0x00, 0xE3, 0x02, 0x00, 0x0B, 0x14, 0x00, 0x03, 0x40, 0x20, 0x00,
    0xFB, 0x00, 0x00, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x0D,
    0x00, 0x41, 0x00, 0x25, 0x00, 0xFB, 0x16, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x0B, 0x06, 0x00, 0x41,
    0x00, 0x25, 0x01, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xE3, 0x02, 0x00, 0x0B,
};

TEST(test_gc_registered_host_tables_are_roots) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_table_t* refs;
    wasm_table_t* conts;
    wasm_table_import_t imp;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    wasm_value_t k;
    wasm_error_t err;
    uint8_t* heap;
    uint32_t i;

    wasm_config_default(&cfg);
    cfg.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &cfg);
    refs = wasm_table_new(&wasm_test_allocator, WASM_TYPE_ANYREF, 1, UINT32_MAX);
    conts = wasm_table_new(&wasm_test_allocator, WASM_TYPE_CONTREF, 2, UINT32_MAX);
    CHECK(t, refs != NULL && conts != NULL);
    if (!refs || !conts) goto done;
    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = "t";
    imp.table = refs;
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &imp));
    imp.name = "k";
    imp.table = conts;
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &imp));

    m = wasm_test_load_sections(&rt, wasm_test_gc_host_table_sections, sizeof(wasm_test_gc_host_table_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) goto done;
    arg = wasm_i32(42);
    WASM_CHECK_OK(t, wasm_call(m, "store", &arg, 1, NULL, 0));
    WASM_CHECK_OK(t, wasm_call(m, "consume", NULL, 0, NULL, 0));
    for (i = rt.conts.free_head; i; i = wasm__cont_at(&rt.conts, i - 1u)->next_free)
        wasm__cont_at(&rt.conts, i - 1u)->gen = WASM__CONT_GEN_MAX;
    WASM_CHECK_OK(t, wasm_call(m, "consume", NULL, 0, NULL, 0));
    CHECK(t, rt.conts.live == 1 && rt.conts.retired == 1);
    wasm_module_free(m);

    heap = rt.gc_heap;
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.gc_heap != heap && rt.gc_heap_offset != 0, "%s", "expected the collection to move the struct");
    CHECK_MSG(t, rt.conts.live == 1 && rt.conts.retired == 1, "expected the tables to keep their continuations, %u live, %u retired",
              (unsigned)rt.conts.live, (unsigned)rt.conts.retired);

    m = wasm_test_load_sections(&rt, wasm_test_gc_host_table_sections, sizeof(wasm_test_gc_host_table_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) goto done;
    arg = wasm_i32(500);
    WASM_CHECK_OK(t, wasm_call(m, "garbage", &arg, 1, NULL, 0));
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_call(m, "load", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_OK(t, wasm_call(m, "get", NULL, 0, &k, 1));
    err = wasm_call(m, "run", &k, 1, NULL, 0);
    CHECK_MSG(t, err == WASM_ERR_TRAP && strstr(rt.error_msg, "freed module") != NULL,
              "expected the kept continuation to need its freed module, got %d (%s)", (int)err, rt.error_msg);
    wasm_module_free(m);

done:
    wasm__runtime_destroy(&rt);
    wasm_table_free(conts);
    wasm_table_free(refs);
}

static const test_case gc_cases[] = {
    { "wasmgc: runtime init allocates a tracked GC heap arena", test_gc_runtime_initializes_heap_arena },
    { "wasmgc: GC allocator tracks struct and array objects", test_gc_allocator_tracks_struct_and_array_objects },
    { "wasmgc: collector traces roots and rewrites live graphs", test_gc_collect_marks_runtime_roots_and_preserves_object_graphs },
    { "GC: collection marks deep object chains without recursion", test_gc_collect_marks_deep_object_chains_iteratively },
    { "wasmgc: allocator retries after collecting unreachable objects", test_gc_allocator_retries_after_collection_on_heap_exhaustion },
    { "gc: the heap grows to fit live data and keeps objects intact", test_gc_heap_grows_with_live_data },
    { "gc: heap growth stops at max_gc_heap_bytes with WASM_ERR_OOM", test_gc_heap_growth_respects_max_gc_heap_bytes },
    { "gc: host externrefs round-trip exactly through anyref and are never i31 or eq", test_gc_host_externrefs_round_trip_through_anyref },
    { "gc: host references whose bits fall inside a later GC heap stay host values", test_gc_host_refs_inside_a_later_heap_stay_host_values },
    { "gc: init expressions keep their operands, globals, tables, and segments alive", test_gc_init_expressions_keep_their_operands_alive },
    { "gc: wasm_struct_new and wasm_array_new keep their inputs alive", test_gc_host_constructors_keep_their_inputs_alive },
    { "gc: unreachable externref boxes and exnrefs are freed", test_gc_collects_unreachable_externref_boxes_and_exnrefs },
    { "gc: objects keep the types of a freed module", test_gc_objects_outlive_their_type_module },
    { "gc: descriptors move with their objects and stay reachable through them", test_gc_descriptors_move_with_their_objects },
    { "gc: wasm_struct_new_desc keeps its descriptor alive and wasm_struct_get_desc reads it", test_gc_host_api_allocates_and_reads_descriptors },
    { "gc: host values of bottom reference types must be null", test_gc_host_values_of_bottom_types_must_be_null },
    { "gc: fields and elements of every kind survive collections after their module is freed",
      test_gc_objects_of_every_field_kind_survive_their_module },
    { "gc: host field accessors round-trip every field kind with its declared type", test_gc_host_field_accessors_round_trip_every_kind },
    { "gc: references only suspended continuation frames hold survive collections and move", test_gc_traces_references_only_suspended_frames_hold },
    { "gc: collections free abandoned continuations with their stacks and reuse their entries", test_gc_frees_abandoned_continuations_with_their_stacks },
    { "gc: registered host tables keep and move what they hold while no module imports them", test_gc_registered_host_tables_are_roots },
};
const test_suite gc_suite = { "gc", gc_cases, COUNTOF(gc_cases) };
