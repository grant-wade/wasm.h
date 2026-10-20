#include "cont.h"
#include "calls.h"
#include "state.h"
#include "translate.h"
#include "../gc.h"
#include "../handles.h"
#include "../internal/alloc.h"
#include "../internal/gc_types.h"
#include "../link.h"
#include "../module.h"
#include "../runtime.h"
#include <string.h>

/* Most released continuation stacks the runtime keeps for reuse. */
#define WASM__CONT_STACK_POOL_MAX 16u

/* Bytes of continuation stacks in use below which starting a continuation never collects first. */
#define WASM__CONT_GC_MIN_BYTES ((size_t)4u * 1024u * 1024u)

static void wasm__cont_stack_clear(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    while (s->call_frame_sp > 0) {
        wasm__call_frame_t* cf = &s->call_frames[--s->call_frame_sp];

        wasm__clear_call_frame(cf);
        wasm__arena_reset(rt, s, cf->arena_saved);
    }
    s->sp = 0;
    s->parked = 0;
    s->parent = NULL;
    s->handlers = NULL;
    s->start_module = NULL;
    s->start_func = 0;
}

static void wasm__cont_stack_free(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    wasm__cont_stack_clear(rt, s);
    wasm__exec_stack_destroy(rt, s);
    wasm__free(&rt->allocator, s);
}

static wasm__exec_stack_t* wasm__cont_stack_new(wasm_runtime_t* rt) {
    wasm__exec_stack_t* s = rt->cont_stack_pool;

    if (rt->max_cont_stacks != 0 && rt->num_cont_stacks >= rt->max_cont_stacks) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "continuation stack limit %u reached", (unsigned)rt->max_cont_stacks);
        return NULL;
    }
    if (s) {
        rt->cont_stack_pool = s->next;
        rt->num_pooled_cont_stacks--;
    } else {
        s = (wasm__exec_stack_t*)wasm__calloc(&rt->allocator, 1u, sizeof(*s));
        if (!s || wasm__exec_stack_init(rt, s, rt->cont_stack_values, rt->cont_call_depth,
                                        rt->cont_frame_arena_size) != WASM_OK) {
            wasm__free(&rt->allocator, s);
            WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for a continuation stack");
            return NULL;
        }
    }
    s->frame_floor = 1u;
    s->prev = NULL;
    s->next = rt->cont_stacks;
    if (rt->cont_stacks) rt->cont_stacks->prev = s;
    rt->cont_stacks = s;
    rt->num_cont_stacks++;
    return s;
}

WASM_PRIVATE void wasm__cont_stack_release(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    if (s->prev)
        s->prev->next = s->next;
    else
        rt->cont_stacks = s->next;
    if (s->next) s->next->prev = s->prev;
    rt->num_cont_stacks--;
    if (rt->num_pooled_cont_stacks >= WASM__CONT_STACK_POOL_MAX) {
        wasm__cont_stack_free(rt, s);
        return;
    }
    wasm__cont_stack_clear(rt, s);
    s->prev = NULL;
    s->next = rt->cont_stack_pool;
    rt->cont_stack_pool = s;
    rt->num_pooled_cont_stacks++;
}

static wasm__cont_object_t* wasm__cont_new(wasm_runtime_t* rt, wasm__exec_stack_t* top, wasm__exec_stack_t* bottom,
                                           wasm_module_t* mod, uint32_t type_index) {
    wasm__cont_object_t* k = (wasm__cont_object_t*)wasm__calloc(&rt->allocator, 1u, sizeof(*k));

    if (!k || wasm__handle_set_insert(&rt->allocator, &rt->cont_objects, k) != WASM_OK) {
        wasm__free(&rt->allocator, k);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for a continuation");
        return NULL;
    }
    k->top = top;
    k->bottom = bottom;
    if (mod && type_index < mod->num_types) {
        k->module = mod;
        k->type_index = type_index;
    }
    return k;
}

WASM_PRIVATE wasm__cont_object_t* wasm__cont_check(wasm_runtime_t* rt, uintptr_t ref, wasm_error_t* out_err) {
    wasm__cont_object_t* k = (wasm__cont_object_t*)ref;

    *out_err = WASM_ERR_TRAP;
    if (!k) {
        WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null continuation reference");
        return NULL;
    }
    if (!wasm__handle_set_contains(&rt->cont_objects, k)) {
        *out_err = WASM_ERR_MALFORMED;
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "invalid continuation reference");
        return NULL;
    }
    if (k->stale) {
        WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "continuation runs code of a freed module");
        return NULL;
    }
    if (!k->top && !k->start_module) {
        WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "continuation already consumed");
        return NULL;
    }
    *out_err = WASM_OK;
    return k;
}

WASM_PRIVATE wasm_error_t wasm__cont_new_func(wasm_runtime_t* rt, wasm_module_t* mod, uint32_t type_index,
                                              wasm_module_t* func_mod, uint32_t func_idx,
                                              wasm__cont_object_t** out) {
    (void)wasm__linked_func_resolve(&func_mod, &func_idx);
    wasm__gc_collect_handles_if_needed(rt);
    *out = wasm__cont_new(rt, NULL, NULL, mod, type_index);
    if (!*out) return WASM_ERR_OOM;
    (*out)->start_module = func_mod;
    (*out)->start_func = func_idx;
    return WASM_OK;
}

WASM_PRIVATE const wasm_functype_t* wasm__cont_start_type(const wasm__cont_object_t* k) {
    if (!k->start_module) return NULL;
    return wasm__module_functype(k->start_module, k->start_module->funcs[k->start_func].type_idx);
}

static void wasm__cont_collect_if_needed(wasm_runtime_t* rt) {
    size_t stack_bytes = (size_t)rt->cont_stack_values * sizeof(wasm__slot_t) +
                         (size_t)rt->cont_call_depth * sizeof(wasm__call_frame_t) + rt->cont_frame_arena_size;
    uint32_t min_stacks = stack_bytes < WASM__CONT_GC_MIN_BYTES ? (uint32_t)(WASM__CONT_GC_MIN_BYTES / stack_bytes) : 1u;
    uint32_t threshold = rt->cont_gc_threshold ? rt->cont_gc_threshold : min_stacks;

    if (rt->num_cont_stacks < threshold && (rt->max_cont_stacks == 0 || rt->num_cont_stacks < rt->max_cont_stacks))
        return;
    (void)wasm__gc_collect(rt, 0);
    rt->cont_gc_threshold = rt->num_cont_stacks > min_stacks / 2u ? rt->num_cont_stacks * 2u : min_stacks;
}

WASM_PRIVATE wasm_error_t wasm__cont_start(wasm_runtime_t* rt, wasm__cont_object_t* k) {
    wasm__exec_stack_t* s;

    wasm__cont_collect_if_needed(rt);
    s = wasm__cont_stack_new(rt);
    if (!s) return WASM_ERR_OOM;
    if (k->num_args > s->max_stack) {
        wasm__cont_stack_release(rt, s);
        WASM__SET_ERR(rt, WASM_ERR_STACK_OVERFLOW, "continuation operand stack limit %u exceeded",
                      (unsigned)rt->cont_stack_values);
        return WASM_ERR_STACK_OVERFLOW;
    }
    if (k->num_args > 0) memcpy(s->stack, k->args, k->num_args * sizeof(*s->stack));
    s->sp = k->num_args;
    s->start_module = k->start_module;
    s->start_func = k->start_func;
    wasm__free(&rt->allocator, k->args);
    k->args = NULL;
    k->num_args = 0;
    k->start_module = NULL;
    k->top = s;
    k->bottom = s;
    return WASM_OK;
}

static int wasm__cont_push(wasm_runtime_t* rt, wasm__exec_stack_t* s, const wasm__slot_t* args, uint32_t slots) {
    if (slots > s->max_stack - s->sp) {
        WASM__SET_ERR(rt, WASM_ERR_STACK_OVERFLOW, "continuation operand stack limit %u exceeded",
                      (unsigned)s->max_stack);
        return 0;
    }
    if (slots > 0) memcpy(&s->stack[s->sp], args, slots * sizeof(*args));
    s->sp += slots;
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__cont_bind(wasm_runtime_t* rt, wasm__cont_object_t* k, wasm_module_t* mod,
                                          uint32_t type_index, const wasm__slot_t* args, uint32_t slots,
                                          wasm__cont_object_t** out) {
    wasm__exec_stack_t* top = k->top;
    wasm__slot_t* bound = k->args;

    if (top) {
        if (!wasm__cont_push(rt, top, args, slots)) return WASM_ERR_STACK_OVERFLOW;
    } else if (slots > 0) {
        bound = (wasm__slot_t*)wasm__realloc_array(&rt->allocator, k->args, k->num_args + slots, sizeof(*bound));
        if (!bound) {
            WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for continuation arguments");
            return WASM_ERR_OOM;
        }
        k->args = bound;
        memcpy(&bound[k->num_args], args, slots * sizeof(*args));
    }
    *out = wasm__cont_new(rt, top, k->bottom, mod, type_index);
    if (!*out) {
        if (top) top->sp -= slots;
        return WASM_ERR_OOM;
    }
    if (!top) {
        (*out)->start_module = k->start_module;
        (*out)->start_func = k->start_func;
        (*out)->args = bound;
        (*out)->num_args = k->num_args + slots;
    }
    k->top = NULL;
    k->bottom = NULL;
    k->start_module = NULL;
    k->args = NULL;
    k->num_args = 0;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__cont_resume(wasm_runtime_t* rt, wasm__cont_object_t* k, const wasm__slot_t* args,
                                            uint32_t slots, const uint32_t* handlers) {
    wasm__exec_stack_t* top = k->top;
    wasm__exec_stack_t* parent = rt->exec;

    if (!wasm__cont_push(rt, top, args, slots)) return WASM_ERR_STACK_OVERFLOW;
    if (top->call_frame_sp == 0) {
        uint32_t bound = top->sp - slots;
        wasm_error_t err;

        rt->exec = top;
        top->sp = 0;
        err = wasm__push_call_frame(top->start_module, top->start_func, top->stack, NULL);
        if (err != WASM_OK) {
            rt->exec = parent;
            top->sp = bound;
            return err;
        }
        top->start_module = NULL;
    }
    top->parked = 0;
    k->bottom->parent = parent;
    k->bottom->handlers = handlers;
    k->top = NULL;
    k->bottom = NULL;
    rt->exec = top;
    return WASM_OK;
}

WASM_PRIVATE wasm__exec_stack_t* wasm__cont_find_handler(wasm_runtime_t* rt, const wasm__exec_stack_t* barrier,
                                                         uint64_t tag_identity, uint32_t kind,
                                                         const uint32_t** out_handler) {
    wasm__exec_stack_t* s;

    for (s = rt->exec; s != barrier && s->parent; s = s->parent) {
        const wasm__exec_stack_t* parent = s->parent;
        const wasm_module_t* mod = parent->call_frames[parent->call_frame_sp - 1u].module;
        const uint32_t* entry = s->handlers + 1;
        uint32_t i;

        for (i = 0; i < s->handlers[0]; i++, entry += WASM__HANDLER_WORDS) {
            if (entry[0] == kind && entry[1] < mod->num_tags && mod->tags[entry[1]].identity == tag_identity) {
                *out_handler = entry;
                return s;
            }
        }
    }
    if (s->parent)
        WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "unhandled tag: a suspension cannot leave a host function call");
    else
        WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "unhandled tag");
    return NULL;
}

WASM_PRIVATE wasm_error_t wasm__cont_suspend(wasm_runtime_t* rt, wasm__exec_stack_t* outer, const uint32_t* handler,
                                             uint32_t payload_slots) {
    wasm__exec_stack_t* inner = rt->exec;
    wasm__exec_stack_t* parent = outer->parent;
    const wasm__call_frame_t* resumer = &parent->call_frames[parent->call_frame_sp - 1u];
    const uint32_t* desc = handler + 3;
    uint32_t dst = resumer->sp_base + desc[2];
    wasm__cont_object_t* k;

    if (desc[1] != payload_slots + 1u || dst > parent->max_stack || desc[1] > parent->max_stack - dst)
        return WASM_ERR_MALFORMED;
    k = wasm__cont_new(rt, inner, outer, resumer->module, handler[2]);
    if (!k) return WASM_ERR_OOM;
    inner->sp -= payload_slots;
    if (payload_slots > 0)
        memcpy(&parent->stack[dst], &inner->stack[inner->sp], payload_slots * sizeof(wasm__slot_t));
    wasm__slot_set_ref(&parent->stack[dst + payload_slots], (uintptr_t)k);
    parent->sp = dst + desc[1];
    inner->parked = 1;
    outer->parent = NULL;
    outer->handlers = NULL;
    rt->exec = parent;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__cont_switch(wasm_runtime_t* rt, wasm__cont_object_t* k, wasm__exec_stack_t* outer,
                                            wasm_module_t* mod, uint32_t type_index, uint32_t slots,
                                            wasm_module_t** out_mod, uint32_t* out_func_idx, uint32_t* out_slots) {
    wasm__exec_stack_t* inner = rt->exec;
    wasm__exec_stack_t* parent = outer->parent;
    wasm__exec_stack_t* start = k->top;
    wasm__slot_t* args = &inner->stack[inner->sp - slots - 1u];
    wasm__slot_t target = args[slots];
    wasm__cont_object_t* captured = wasm__cont_new(rt, inner, outer, mod, type_index);
    wasm_error_t err = WASM__ERR_CALL_FRAME;

    if (!captured) return WASM_ERR_OOM;
    wasm__slot_set_ref(&args[slots], (uintptr_t)captured);
    if (!start) {
        uint32_t bound = k->num_args;

        if (bound > parent->max_stack - parent->sp || slots + 1u > parent->max_stack - parent->sp - bound) {
            WASM__SET_ERR(rt, WASM_ERR_STACK_OVERFLOW, "operand stack limit %u exceeded",
                          (unsigned)parent->max_stack);
            err = WASM_ERR_STACK_OVERFLOW;
        } else {
            if (bound > 0) memcpy(&parent->stack[parent->sp], k->args, bound * sizeof(*args));
            memcpy(&parent->stack[parent->sp + bound], args, (slots + 1u) * sizeof(*args));
            parent->sp += bound + slots + 1u;
            *out_mod = k->start_module;
            *out_func_idx = k->start_func;
            *out_slots = bound + slots + 1u;
            wasm__cont_drop_stacks(rt, k);
            rt->exec = parent;
        }
    } else {
        rt->exec = parent;
        err = wasm__cont_resume(rt, k, args, slots + 1u, outer->handlers);
    }
    if (err != WASM_OK && err != WASM__ERR_CALL_FRAME) {
        rt->exec = inner;
        args[slots] = target;
        captured->top = NULL;
        captured->bottom = NULL;
        return err;
    }
    inner->sp -= slots + 1u;
    inner->parked = 1;
    outer->parent = NULL;
    outer->handlers = NULL;
    return err;
}

WASM_PRIVATE void wasm__cont_unwind(wasm_runtime_t* rt, wasm__exec_stack_t* base) {
    while (rt->exec != base && rt->exec->parent) {
        wasm__exec_stack_t* parent = rt->exec->parent;

        wasm__cont_stack_release(rt, rt->exec);
        rt->exec = parent;
    }
}

WASM_PRIVATE void wasm__cont_drop_stacks(wasm_runtime_t* rt, wasm__cont_object_t* k) {
    wasm__exec_stack_t* s = k->top;

    while (s) {
        wasm__exec_stack_t* next = s == k->bottom ? NULL : s->parent;

        wasm__cont_stack_release(rt, s);
        s = next;
    }
    wasm__free(&rt->allocator, k->args);
    k->top = NULL;
    k->bottom = NULL;
    k->start_module = NULL;
    k->args = NULL;
    k->num_args = 0;
}

static int wasm__cont_stack_uses(const wasm__exec_stack_t* s, const wasm_module_t* mod) {
    uint32_t i;

    if (s->start_module == mod) return 1;
    for (i = 0; i < s->call_frame_sp; i++) {
        if (s->call_frames[i].module == mod) return 1;
    }
    return 0;
}

WASM_PRIVATE void wasm__cont_invalidate_using(wasm_runtime_t* rt, const wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < rt->cont_objects.capacity; i++) {
        wasm__cont_object_t* k = (wasm__cont_object_t*)rt->cont_objects.slots[i];
        const wasm__exec_stack_t* s;

        if (!k) continue;
        if (k->start_module == mod) {
            wasm__cont_drop_stacks(rt, k);
            k->stale = 1;
            continue;
        }
        for (s = k->top; s; s = s == k->bottom ? NULL : s->parent) {
            if (!wasm__cont_stack_uses(s, mod)) continue;
            wasm__cont_drop_stacks(rt, k);
            k->stale = 1;
            break;
        }
    }
}

WASM_PRIVATE void wasm__cont_free_all(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->cont_objects.capacity; i++) {
        wasm__cont_object_t* k = (wasm__cont_object_t*)rt->cont_objects.slots[i];

        if (k) wasm__free(&rt->allocator, k->args);
        wasm__free(&rt->allocator, k);
    }
    wasm__handle_set_free(&rt->allocator, &rt->cont_objects);
    while (rt->cont_stacks) {
        wasm__exec_stack_t* s = rt->cont_stacks;

        rt->cont_stacks = s->next;
        wasm__cont_stack_free(rt, s);
    }
    while (rt->cont_stack_pool) {
        wasm__exec_stack_t* s = rt->cont_stack_pool;

        rt->cont_stack_pool = s->next;
        wasm__cont_stack_free(rt, s);
    }
    rt->num_cont_stacks = 0;
    rt->num_pooled_cont_stacks = 0;
    rt->exec = &rt->main_stack;
}
