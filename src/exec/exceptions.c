#include "exceptions.h"
#include <stdio.h>
#include "calls.h"
#include "cont.h"
#include "stack.h"
#include "translate.h"
#include "../gc.h"
#include "../internal/alloc.h"
#include "../internal/gc_types.h"
#include "../module.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include <string.h>

/* Bytes of the entries references are made to since the last collection below which the exception table never
 * asks for one. */
#define WASM__EXN_GC_MIN_BYTES ((size_t)256u * 1024u)

#define WASM__EXN_KINDS_PER_WORD (32u / WASM__SLOT_KIND_BITS)
#define WASM__EXN_KIND_WORDS(slots) (((slots) + WASM__EXN_KINDS_PER_WORD - 1u) / WASM__EXN_KINDS_PER_WORD)

/* Makes *slots, with room for *cap slots and then their kind words, hold count slots; returns 0 if out of memory.
 * The storage keeps its contents only if it already had room. */
static int wasm__exn_reserve(wasm_runtime_t* rt, wasm__slot_t** slots, uint32_t* cap, uint32_t count) {
    uint32_t new_cap = count < 4u ? 4u : count;
    size_t words = WASM__EXN_KIND_WORDS((size_t)new_cap);
    wasm__slot_t* storage;

    if (count <= *cap) return 1;
    if ((size_t)new_cap > (SIZE_MAX - words * sizeof(uint32_t)) / sizeof(wasm__slot_t)) return 0;
    storage = (wasm__slot_t*)wasm__malloc(&rt->allocator,
                                          (size_t)new_cap * sizeof(wasm__slot_t) + words * sizeof(uint32_t));
    if (!storage) return 0;
    wasm__free(&rt->allocator, *slots);
    *slots = storage;
    *cap = new_cap;
    return 1;
}

static void wasm__exn_set_pending_object(wasm__exception_state_t* ex, wasm__exception_object_t* exception) {
    ex->tag_identity = exception->tag_identity;
    ex->tag_index = exception->tag_index;
    ex->slots = exception->slots;
    ex->kinds = exception->kinds;
    ex->source = exception;
    ex->num_slots = exception->num_slots;
    ex->is_pending = 1;
}

/* kinds receives the kind of each of the first slots slots of a payload of type type. */
static void wasm__exn_payload_kinds(const wasm_functype_t* type, uint32_t* kinds, uint32_t slots) {
    uint32_t slot = 0;
    uint32_t i;

    memset(kinds, 0, WASM__EXN_KIND_WORDS(slots) * sizeof(*kinds));
    for (i = 0; i < type->num_params && slot < slots; i++) {
        uint32_t kind = wasm__slot_kind(type->params[i]);

        if (kind != WASM__SLOT_KIND_NONE)
            kinds[slot / WASM__EXN_KINDS_PER_WORD] |= kind << (slot % WASM__EXN_KINDS_PER_WORD * WASM__SLOT_KIND_BITS);
        slot += wasm__valtype_slots(type->params[i]);
    }
}

/* Sets tag's payload_slots and payload_kinds; returns 0 if its type is malformed. */
static int wasm__exn_describe_tag(const wasm_module_t* mod, wasm_tag_t* tag) {
    const wasm_functype_t* type = wasm__module_functype(mod, tag->type_idx);
    uint32_t slots;
    uint32_t kinds = 0;

    if (!type) return 0;
    slots = wasm__valtypes_slots(type->params, type->num_params);
    wasm__exn_payload_kinds(type, &kinds, slots < WASM__EXN_KINDS_PER_WORD ? slots : WASM__EXN_KINDS_PER_WORD);
    tag->payload_kinds = kinds;
    tag->payload_slots = slots + 1u;
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__exn_throw_slots(wasm_runtime_t* rt, wasm_module_t* mod, uint32_t tag_index,
                                                const wasm__slot_t* top, uint32_t* out_slots) {
    wasm__exception_state_t* ex = &rt->pending_exception;
    wasm_tag_t* tag = &mod->tags[tag_index];
    uint32_t* kinds = NULL;
    uint32_t slots;

    if (tag->payload_slots == 0 && !wasm__exn_describe_tag(mod, tag)) return WASM_ERR_MALFORMED;
    slots = tag->payload_slots - 1u;
    if (slots > 0) {
        const wasm__slot_t* src = top - slots;
        uint32_t i;

        if (slots > ex->buffer_cap && !wasm__exn_reserve(rt, &ex->buffer, &ex->buffer_cap, slots)) return WASM_ERR_OOM;
        kinds = (uint32_t*)(ex->buffer + ex->buffer_cap);
        for (i = 0; i < slots; i++) ex->buffer[i] = src[i];
        if (slots <= WASM__EXN_KINDS_PER_WORD)
            kinds[0] = tag->payload_kinds;
        else
            wasm__exn_payload_kinds(wasm__module_functype(mod, tag->type_idx), kinds, slots);
    }
    ex->tag_identity = tag->identity;
    ex->tag_index = tag_index;
    ex->slots = ex->buffer;
    ex->kinds = kinds;
    ex->source = NULL;
    ex->num_slots = slots;
    ex->is_pending = 1;
    *out_slots = slots;
    return WASM_OK;
}

WASM_PRIVATE void wasm__exn_clear_pending(wasm_runtime_t* rt) {
    wasm__exception_state_t* ex = &rt->pending_exception;

    ex->slots = NULL;
    ex->kinds = NULL;
    ex->source = NULL;
    ex->num_slots = 0;
    ex->is_pending = 0;
}

/* Returns the entry of rt's exception table ref names, with ref's generation in *out_gen, or NULL if ref is no
 * exception reference of rt. */
static wasm__exception_object_t* wasm__exn_decode(const wasm_runtime_t* rt, uintptr_t ref, uint32_t* out_gen) {
    uint32_t index = ((uint32_t)(ref >> 1) & (WASM__CONT_MAX_ENTRIES - 1u)) - rt->conts.base;

    if ((ref & 1u) != 0 || (ref >> WASM__CONT_TAG_SHIFT) != rt->conts.tag || index >= rt->exns.count) return NULL;
    *out_gen = (uint32_t)(ref >> WASM__CONT_GEN_SHIFT) & WASM__CONT_GEN_MAX;
    return wasm__exn_at(&rt->exns, index);
}

static wasm__exception_object_t* wasm__exn_get(const wasm_runtime_t* rt, uintptr_t ref) {
    uint32_t gen = 0;
    wasm__exception_object_t* exception = wasm__exn_decode(rt, ref, &gen);

    return exception && exception->in_use && exception->gen == gen ? exception : NULL;
}

static uintptr_t wasm__exn_ref(const wasm_runtime_t* rt, const wasm__exception_object_t* exception) {
    return (rt->conts.tag << WASM__CONT_TAG_SHIFT) | ((uintptr_t)exception->gen << WASM__CONT_GEN_SHIFT) |
           ((uintptr_t)(rt->conts.base + exception->index) << 1);
}

WASM_PRIVATE const wasm__exception_object_t* wasm__exn_live(const wasm_runtime_t* rt, uintptr_t ref) {
    return wasm__exn_get(rt, ref);
}

#ifndef NDEBUG
WASM_PRIVATE int wasm__exn_issued(const wasm_runtime_t* rt, uintptr_t ref) {
    uint32_t gen = 0;

    return wasm__exn_decode(rt, ref, &gen) != NULL;
}
#endif

WASM_PRIVATE WASM__COLD int wasm__exn_throw_ref(wasm_runtime_t* rt, uintptr_t ref) {
    wasm__exception_object_t* exception = wasm__exn_get(rt, ref);

    if (!exception) return 0;
    wasm__exn_set_pending_object(&rt->pending_exception, exception);
    return 1;
}

WASM_PRIVATE WASM__COLD wasm_error_t wasm__exn_rethrow(wasm_runtime_t* rt, wasm__exception_object_t* exception) {
    wasm__exception_state_t* ex = &rt->pending_exception;
    uint32_t* kinds = NULL;

    if (!exception->held) {
        wasm__exn_set_pending_object(ex, exception);
        return WASM_OK;
    }
    if (exception->num_slots > 0) {
        if (!wasm__exn_reserve(rt, &ex->buffer, &ex->buffer_cap, exception->num_slots)) return WASM_ERR_OOM;
        kinds = (uint32_t*)(ex->buffer + ex->buffer_cap);
        memcpy(ex->buffer, exception->slots, exception->num_slots * sizeof(wasm__slot_t));
        memcpy(kinds, exception->kinds, WASM__EXN_KIND_WORDS(exception->num_slots) * sizeof(*kinds));
    }
    ex->tag_identity = exception->tag_identity;
    ex->tag_index = exception->tag_index;
    ex->slots = ex->buffer;
    ex->kinds = kinds;
    ex->source = NULL;
    ex->num_slots = exception->num_slots;
    ex->is_pending = 1;
    return WASM_OK;
}

/* Returns a free entry of table, which may grow to limit entries, now in use, or NULL. */
static wasm__exception_object_t* wasm__exn_new_entry(wasm_runtime_t* rt, wasm__exn_table_t* table, uint32_t limit,
                                                     uint8_t held) {
    wasm__exception_object_t* exception;

    if (table->free_head) {
        exception = wasm__exn_at(table, table->free_head - 1u);
        table->free_head = exception->next_free;
    } else {
        if (table->count >= limit) return NULL;
        if (table->count == table->num_blocks * WASM__EXN_BLOCK_ENTRIES) {
            if (table->num_blocks == table->cap_blocks) {
                uint32_t cap = table->cap_blocks ? table->cap_blocks * 2u : 4u;
                wasm__exception_object_t** blocks = (wasm__exception_object_t**)wasm__realloc_array(
                    &rt->allocator, table->blocks, cap, sizeof(*blocks));

                if (!blocks) return NULL;
                table->blocks = blocks;
                table->cap_blocks = cap;
            }
            table->blocks[table->num_blocks] = (wasm__exception_object_t*)wasm__malloc_array(
                &rt->allocator, WASM__EXN_BLOCK_ENTRIES, sizeof(wasm__exception_object_t));
            if (!table->blocks[table->num_blocks]) return NULL;
            table->num_blocks++;
        }
        exception = wasm__exn_at(table, table->count);
        memset(exception, 0, sizeof(*exception));
        exception->index = table->count++;
        exception->held = held;
    }
    exception->next_free = 0;
    exception->in_use = 1;
    table->live++;
    return exception;
}

static void wasm__exn_free_entry(wasm__exn_table_t* table, wasm__exception_object_t* exception) {
    exception->in_use = 0;
    exception->num_slots = 0;
    exception->gen = (exception->gen + 1u) & WASM__CONT_GEN_MAX;
    exception->next_free = table->free_head;
    table->free_head = exception->index + 1u;
    table->live--;
}

/* Copies rt's pending exception into exception; returns 0 if out of memory. */
static int wasm__exn_fill(wasm_runtime_t* rt, wasm__exception_object_t* exception) {
    const wasm__exception_state_t* ex = &rt->pending_exception;
    uint32_t i;

    if (ex->num_slots > 0) {
        if (ex->num_slots > exception->cap_slots) {
            if (!wasm__exn_reserve(rt, &exception->slots, &exception->cap_slots, ex->num_slots)) return 0;
            exception->kinds = (uint32_t*)(exception->slots + exception->cap_slots);
        }
        for (i = 0; i < ex->num_slots; i++) exception->slots[i] = ex->slots[i];
        for (i = 0; i < WASM__EXN_KIND_WORDS(ex->num_slots); i++) exception->kinds[i] = ex->kinds[i];
    }
    exception->tag_identity = ex->tag_identity;
    exception->tag_index = ex->tag_index;
    exception->num_slots = ex->num_slots;
    return 1;
}

/* Returns in *out_ref a reference to rt's pending exception. */
static wasm_error_t wasm__exn_ref_pending(wasm_runtime_t* rt, uintptr_t* out_ref) {
    wasm__exception_object_t* exception = rt->pending_exception.source;

    if (!exception) {
        if (!wasm__cont_claim_tag(rt)) {
            WASM__SET_ERR(rt, WASM_ERR_OOM, "%s",
                          "out of reference tags: too many runtimes hold exceptions or continuations");
            return WASM_ERR_OOM;
        }
        exception = wasm__exn_new_entry(rt, &rt->exns, rt->conts.limit, 0);
        if (!exception) {
            if (rt->exns.count >= rt->conts.limit)
                WASM__SET_ERR(rt, WASM_ERR_OOM, "exception limit %u reached", (unsigned)rt->conts.limit);
            return WASM_ERR_OOM;
        }
        if (!wasm__exn_fill(rt, exception)) {
            wasm__exn_free_entry(&rt->exns, exception);
            return WASM_ERR_OOM;
        }
        rt->exns.new_bytes += sizeof(*exception) + (size_t)exception->num_slots * sizeof(wasm__slot_t);
    }
    *out_ref = wasm__exn_ref(rt, exception);
    return WASM_OK;
}

/* Returns an exception object holding rt's pending exception for a legacy catch clause, or NULL. */
static wasm__exception_object_t* wasm__exn_hold_pending(wasm_runtime_t* rt) {
    wasm__exception_object_t* exception = rt->pending_exception.source;

    if (exception) return exception;
    exception = wasm__exn_new_entry(rt, &rt->held_exns, UINT32_MAX, 1);
    if (exception && !wasm__exn_fill(rt, exception)) {
        wasm__exn_free_entry(&rt->held_exns, exception);
        return NULL;
    }
    return exception;
}

WASM_PRIVATE void wasm__exn_release(wasm_runtime_t* rt, wasm__exception_object_t* exception) {
    if (exception->held) wasm__exn_free_entry(&rt->held_exns, exception);
}

WASM_PRIVATE wasm__exception_object_t* wasm__exn_mark(wasm_runtime_t* rt, uintptr_t ref) {
    wasm__exception_object_t* exception = wasm__exn_get(rt, ref);

    if (!exception || exception->marked) return NULL;
    exception->marked = 1;
    return exception;
}

WASM_PRIVATE void wasm__exn_clear_marks(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->exns.count; i++) wasm__exn_at(&rt->exns, i)->marked = 0;
    for (i = 0; i < rt->held_exns.count; i++) wasm__exn_at(&rt->held_exns, i)->marked = 0;
}

WASM_PRIVATE void wasm__exn_sweep(wasm_runtime_t* rt) {
    wasm__exn_table_t* table = &rt->exns;
    uint32_t i;

    for (i = 0; i < table->count; i++) {
        wasm__exception_object_t* exception = wasm__exn_at(table, i);

        if (exception->in_use && !exception->marked) wasm__exn_free_entry(table, exception);
    }
    table->new_bytes = 0;
    table->swept_live = table->live;
}

WASM_PRIVATE int wasm__exn_collection_due(const wasm_runtime_t* rt) {
    const wasm__exn_table_t* table = &rt->exns;

    if (rt->conts.limit != 0 && !table->free_head && table->count >= rt->conts.limit) return 1;
    return table->new_bytes >= WASM__EXN_GC_MIN_BYTES && table->new_bytes >= rt->gc_heap_offset &&
           table->new_bytes / sizeof(wasm__exception_object_t) >= table->swept_live;
}

static void wasm__exn_free_table(wasm_runtime_t* rt, wasm__exn_table_t* table) {
    uint32_t i;

    for (i = 0; i < table->count; i++) wasm__free(&rt->allocator, wasm__exn_at(table, i)->slots);
    for (i = 0; i < table->num_blocks; i++) wasm__free(&rt->allocator, table->blocks[i]);
    wasm__free(&rt->allocator, table->blocks);
    memset(table, 0, sizeof(*table));
}

WASM_PRIVATE void wasm__exn_free_all(wasm_runtime_t* rt) {
    wasm__exn_free_table(rt, &rt->exns);
    wasm__exn_free_table(rt, &rt->held_exns);
    wasm__free(&rt->allocator, rt->pending_exception.buffer);
    memset(&rt->pending_exception, 0, sizeof(rt->pending_exception));
}

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt) {
    WASM__SET_ERR(rt, WASM_ERR_TRAP, "uncaught exception tag %u", (unsigned)rt->pending_exception.tag_index);
    wasm__exn_clear_pending(rt);
    return WASM_ERR_TRAP;
}

WASM_PRIVATE wasm_error_t wasm__branch_frame(wasm__call_frame_t* cf, const uint32_t* desc) {
    wasm_runtime_t* rt = cf->module->rt;
    wasm__exec_stack_t* exec = rt->exec;
    uint32_t arity = desc[1];
    uint32_t pops = desc[3] >> WASM__BRANCH_POPS_SHIFT;
    uint32_t dst = cf->sp_base + desc[2];

    if (desc[3] & WASM__BRANCH_RETURN) {
        cf->pc = NULL;
        return WASM_OK;
    }
    if (arity > 0 && exec->sp - arity != dst)
        memmove(&exec->stack[dst], &exec->stack[exec->sp - arity], arity * sizeof(wasm__slot_t));
    exec->sp = dst + arity;
    while (pops-- > 0 && cf->lsp > 0) {
        wasm__clear_label(rt, &cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
    cf->pc = cf->func->ops + desc[0];
    if ((desc[3] & WASM__BRANCH_LOOP) && wasm__runtime_take_interrupt(rt)) return WASM_ERR_INTERRUPTED;
    return WASM_OK;
}

static void wasm__unwind_labels(wasm_runtime_t* rt, wasm__call_frame_t* cf, uint32_t keep) {
    while (cf->lsp > keep) {
        wasm__clear_label(rt, &cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
}

static int wasm__tag_matches(const wasm_module_t* mod, uint32_t tag_index, const wasm__exception_state_t* ex) {
    return tag_index < mod->num_tags && mod->tags[tag_index].identity == ex->tag_identity;
}

/* A legacy try running one of its catch clauses does not handle exceptions. */
static int wasm__catch_legacy(wasm__call_frame_t* cf, uint32_t idx, int* delegated, uint32_t* next_idx,
                              wasm_error_t* out_err) {
    wasm_module_t* mod = cf->module;
    wasm_runtime_t* rt = mod->rt;
    wasm__exec_stack_t* exec = rt->exec;
    const wasm__exception_state_t* ex = &rt->pending_exception;
    wasm__label_t* label = &cf->labels[idx - 1];
    const uint32_t* clause = label->handler + 1;
    uint32_t count = label->handler[0];
    uint32_t i;

    for (i = 0; i < count; i++, clause += 3) {
        if (clause[0] == WASM__CLAUSE_DELEGATE) {
            uint32_t skip = clause[1];

            wasm__clear_label(rt, label);
            idx--;
            while (skip > 0 && idx > 0) {
                wasm__clear_label(rt, &cf->labels[idx - 1]);
                idx--;
                skip--;
            }
            *delegated = 1;
            *next_idx = idx;
            return 0;
        }
        if (clause[0] == WASM__CLAUSE_CATCH && !wasm__tag_matches(mod, clause[1], ex)) continue;

        wasm__unwind_labels(rt, cf, idx);
        exec->sp = label->sp_base;
        label->caught = wasm__exn_hold_pending(rt);
        if (!label->caught) {
            wasm__exn_clear_pending(rt);
            *out_err = WASM_ERR_OOM;
            return 1;
        }
        label->active_catch = 1;
        if (clause[0] == WASM__CLAUSE_CATCH) {
            wasm_error_t err = wasm__exec_stack_reserve(rt, exec, ex->num_slots);

            if (err != WASM_OK) {
                wasm__exn_clear_pending(rt);
                *out_err = err;
                return 1;
            }
            if (ex->num_slots > 0) memcpy(&exec->stack[exec->sp], ex->slots, ex->num_slots * sizeof(wasm__slot_t));
            exec->sp += ex->num_slots;
        }
        wasm__exn_clear_pending(rt);
        cf->pc = cf->func->ops + clause[2];
        *out_err = WASM_OK;
        return 1;
    }
    return 0;
}

static int wasm__catch_try_table(wasm__call_frame_t* cf, uint32_t idx, wasm_error_t* out_err) {
    wasm_module_t* mod = cf->module;
    wasm_runtime_t* rt = mod->rt;
    wasm__exec_stack_t* exec = rt->exec;
    const wasm__exception_state_t* ex = &rt->pending_exception;
    wasm__label_t* label = &cf->labels[idx - 1];
    const uint32_t* clause = label->handler + 1;
    uint32_t count = label->handler[0];
    uint32_t i;

    for (i = 0; i < count; i++, clause += 6) {
        uint32_t kind = clause[0];
        uint32_t slots = 0;
        uint32_t j;

        if (kind == 0x00 || kind == 0x01) {
            if (!wasm__tag_matches(mod, clause[1], ex)) continue;
            slots = ex->num_slots;
        } else if (kind != 0x02 && kind != 0x03) {
            *out_err = WASM_ERR_MALFORMED;
            return 1;
        }

        if (kind & 1u) {
#ifdef WASM_GC_STRESS
            (void)wasm__gc_collect(rt, 0);
#else
            wasm__gc_collect_handles_if_needed(rt);
#endif
        }
        wasm__unwind_labels(rt, cf, idx - 1);
        if (slots + 1u > exec->max_stack - label->sp_base) {
            exec->sp = label->sp_base;
            wasm__exn_clear_pending(rt);
            *out_err = WASM_ERR_STACK_OVERFLOW;
            return 1;
        }
        exec->sp = (clause[5] & WASM__BRANCH_RETURN) ? label->sp_base : cf->sp_base + clause[4];
        *out_err = wasm__exec_stack_reserve(rt, exec, slots + (kind & 1u));
        if (*out_err != WASM_OK) {
            wasm__exn_clear_pending(rt);
            return 1;
        }
        for (j = 0; j < slots; j++) exec->stack[exec->sp + j] = ex->slots[j];
        exec->sp += slots;
        if (kind & 1u) {
            uintptr_t ref = 0;

            *out_err = wasm__exn_ref_pending(rt, &ref);
            if (*out_err != WASM_OK) {
                wasm__exn_clear_pending(rt);
                return 1;
            }
            wasm__slot_set_ref(&exec->stack[exec->sp++], ref);
        }
        wasm__exn_clear_pending(rt);
        *out_err = wasm__branch_frame(cf, clause + 2);
        return 1;
    }
    return 0;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = cf->module->rt;
    wasm__exec_stack_t* exec = rt->exec;
    wasm_error_t err = WASM_OK;
    uint32_t idx = cf->lsp;

    while (idx > 0) {
        wasm__label_t* label = &cf->labels[idx - 1];

        if (label->opcode == 0x06 && !label->active_catch) {
            int delegated = 0;
            uint32_t next_idx = idx;

            if (wasm__catch_legacy(cf, idx, &delegated, &next_idx, &err)) return err;
            if (delegated) {
                idx = next_idx;
                continue;
            }
        } else if (label->opcode == 0x1F) {
            if (wasm__catch_try_table(cf, idx, &err)) return err;
        }
        wasm__clear_label(rt, label);
        exec->sp = label->sp_base;
        idx--;
    }

    cf->lsp = 0;
    exec->sp = cf->sp_base;
    return WASM__ERR_EXCEPTION;
}
