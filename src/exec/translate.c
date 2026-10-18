#include "translate.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../portable.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../values.h"
#include <string.h>

/* A block being translated; fixups chains unresolved target words as index + 1, 0 ending it. */
typedef struct wasm__xframe_t {
    uint8_t opcode;    /* 0x02, 0x03, 0x04, 0x06, or 0x1F; 0 for the function body */
    uint8_t in_catch;  /* a try past its first catch clause */
    uint32_t height;   /* operand stack slots below the block's parameters */
    uint32_t params;   /* slots of the block's parameters */
    uint32_t results;  /* slots of the block's results */
    uint32_t start;    /* loop: index of the body */
    uint32_t fixups;
    uint32_t if_fixup; /* if: index + 1 of the word that takes the else body or continuation */
    uint32_t handlers; /* handler labels open outside the block */
    uint32_t try_id;   /* try: its index in try_operands */
} wasm__xframe_t;

typedef struct wasm__xclause_t {
    uint32_t try_id;
    uint32_t kind;
    uint32_t imm;
    uint32_t body;
} wasm__xclause_t;

typedef struct wasm__translator_t {
    const wasm_allocator_t* allocator;
    uint32_t* ops;
    uint32_t len;
    uint32_t cap;
    uint32_t* offsets;
    uint32_t num_offsets;
    uint32_t offsets_cap;
    wasm__xframe_t* frames;
    uint32_t fp;
    uint32_t frames_cap;
    wasm__xclause_t* clauses;
    uint32_t num_clauses;
    uint32_t clauses_cap;
    uint32_t* try_operands;
    uint32_t num_tries;
    uint32_t tries_cap;
    uint32_t* local_slots;
    uint32_t num_local_slots;
    uint32_t handlers;
    uint32_t max_handlers;
    int failed;
} wasm__translator_t;

static void* wasm__x_grow(wasm__translator_t* t, void* buf, uint32_t* cap, uint32_t need, size_t size) {
    uint32_t new_cap = *cap ? *cap : 16u;
    void* grown;

    if (need <= *cap) return buf;
    while (new_cap < need) {
        if (new_cap > UINT32_MAX / 2u) {
            t->failed = 1;
            return buf;
        }
        new_cap *= 2u;
    }
    grown = wasm__realloc_array(t->allocator, buf, new_cap, size);
    if (!grown) {
        t->failed = 1;
        return buf;
    }
    *cap = new_cap;
    return grown;
}

static uint32_t wasm__x_emit(wasm__translator_t* t, uint32_t word) {
    if (t->len >= t->cap) {
        t->ops = (uint32_t*)wasm__x_grow(t, t->ops, &t->cap, t->len + 1u, sizeof(uint32_t));
        if (t->len >= t->cap) return t->len;
    }
    t->ops[t->len] = word;
    return t->len++;
}

static void wasm__x_patch(wasm__translator_t* t, uint32_t chain, uint32_t target) {
    while (chain != 0) {
        uint32_t index = chain - 1u;

        chain = t->ops[index];
        t->ops[index] = target;
    }
}

static uint32_t wasm__x_arity(const wasm__xframe_t* frame) {
    return frame->opcode == 0x03 ? frame->params : frame->results;
}

static void wasm__x_target(wasm__translator_t* t, wasm__xframe_t* frame) {
    if (frame->opcode == 0x03) {
        wasm__x_emit(t, frame->start);
    } else {
        uint32_t index = wasm__x_emit(t, frame->fixups);

        if (!t->failed) frame->fixups = index + 1u;
    }
}

/* Emits the branch descriptor for label depth, counted from the innermost open block. */
static void wasm__x_descriptor(wasm__translator_t* t, uint32_t depth) {
    wasm__xframe_t* frame = &t->frames[t->fp - 1u - depth];

    if (frame->opcode == 0) {
        wasm__x_emit(t, 0);
        wasm__x_emit(t, frame->results);
        wasm__x_emit(t, 0);
        wasm__x_emit(t, WASM__BRANCH_RETURN);
        return;
    }
    wasm__x_target(t, frame);
    wasm__x_emit(t, wasm__x_arity(frame));
    wasm__x_emit(t, frame->height);
    wasm__x_emit(t, (frame->opcode == 0x03 ? WASM__BRANCH_LOOP : 0u) |
                        ((t->handlers - frame->handlers) << WASM__BRANCH_POPS_SHIFT));
}

static void wasm__x_branch(wasm__translator_t* t,
                           uint32_t short_op,
                           uint32_t loop_op,
                           uint32_t long_op,
                           uint32_t depth,
                           uint32_t height) {
    wasm__xframe_t* frame = &t->frames[t->fp - 1u - depth];

    if (frame->opcode != 0 && t->handlers == frame->handlers &&
        height == frame->height + wasm__x_arity(frame)) {
        wasm__x_emit(t, frame->opcode == 0x03 ? loop_op : short_op);
        wasm__x_target(t, frame);
        return;
    }
    wasm__x_emit(t, long_op);
    wasm__x_descriptor(t, depth);
}

static wasm__xframe_t* wasm__x_push_frame(wasm__translator_t* t, uint8_t opcode, uint32_t height,
                                          const wasm__blocktype_t* blocktype) {
    wasm__xframe_t* frame;

    t->frames = (wasm__xframe_t*)wasm__x_grow(t, t->frames, &t->frames_cap, t->fp + 1u, sizeof(*t->frames));
    if (t->failed) return NULL;
    frame = &t->frames[t->fp++];
    memset(frame, 0, sizeof(*frame));
    frame->opcode = opcode;
    frame->params = wasm__valtypes_slots(blocktype->params, blocktype->param_arity);
    frame->results = wasm__valtypes_slots(blocktype->results, blocktype->result_arity);
    frame->height = height - frame->params;
    frame->handlers = t->handlers;
    frame->start = t->len;
    return frame;
}

static void wasm__x_open_handler(wasm__translator_t* t) {
    t->handlers++;
    if (t->handlers > t->max_handlers) t->max_handlers = t->handlers;
}

static void wasm__x_add_clause(wasm__translator_t* t, uint32_t try_id, uint32_t kind, uint32_t imm, uint32_t body) {
    t->clauses = (wasm__xclause_t*)wasm__x_grow(t, t->clauses, &t->clauses_cap, t->num_clauses + 1u,
                                               sizeof(*t->clauses));
    if (t->failed) return;
    t->clauses[t->num_clauses].try_id = try_id;
    t->clauses[t->num_clauses].kind = kind;
    t->clauses[t->num_clauses].imm = imm;
    t->clauses[t->num_clauses].body = body;
    t->num_clauses++;
}

static void wasm__x_record_offset(wasm__translator_t* t, uint32_t offset) {
    t->offsets = (uint32_t*)wasm__x_grow(t, t->offsets, &t->offsets_cap, t->num_offsets * 2u + 2u,
                                         sizeof(uint32_t));
    if (t->failed) return;
    t->offsets[t->num_offsets * 2u] = t->len;
    t->offsets[t->num_offsets * 2u + 1u] = offset;
    t->num_offsets++;
}

static void wasm__x_free(wasm__translator_t* t) {
    wasm__free(t->allocator, t->ops);
    wasm__free(t->allocator, t->offsets);
    wasm__free(t->allocator, t->frames);
    wasm__free(t->allocator, t->clauses);
    wasm__free(t->allocator, t->try_operands);
    wasm__free(t->allocator, t->local_slots);
}

static void wasm__x_local_slots(wasm__translator_t* t, const wasm_func_t* func) {
    uint32_t i;

    if (func->num_locals == 0) return;
    t->local_slots = (uint32_t*)wasm__malloc_array(t->allocator, func->num_locals, sizeof(uint32_t));
    if (!t->local_slots) {
        t->failed = 1;
        return;
    }
    for (i = 0; i < func->num_locals; i++) {
        t->local_slots[i] = t->num_local_slots;
        t->num_local_slots += wasm__valtype_slots(func->locals[i]);
    }
}

/* Returns a copy of count words of src, or NULL with *failed set when out of memory. */
static uint32_t* wasm__x_copy_words(const wasm_allocator_t* a, const uint32_t* src, uint32_t count, int* failed) {
    uint32_t* copy;

    if (count == 0) return NULL;
    copy = (uint32_t*)wasm__malloc_array(a, count, sizeof(uint32_t));
    if (!copy) {
        *failed = 1;
        return NULL;
    }
    memcpy(copy, src, (size_t)count * sizeof(uint32_t));
    return copy;
}

static void wasm__x_param_slots(wasm__translator_t* t, wasm_module_t* mod, uint32_t type_idx) {
    const wasm_functype_t* type = wasm__module_functype(mod, type_idx);

    wasm__x_emit(t, type ? wasm__valtypes_slots(type->params, type->num_params) : 0u);
}

static int wasm__x_is_i32_bin(uint32_t op) {
    return (op >= 0x46 && op <= 0x4F) || (op >= 0x6A && op <= 0x6C) || (op >= 0x71 && op <= 0x78);
}

static int wasm__x_is_i32_cmp(uint32_t op) {
    return op >= 0x46 && op <= 0x4F;
}

static int wasm__x_is_i64_bin(uint32_t op) {
    return (op >= 0x51 && op <= 0x5A) || (op >= 0x7C && op <= 0x7E) || (op >= 0x83 && op <= 0x8A);
}

static int wasm__x_is_f64_bin(uint32_t op) {
    return op >= 0xA0 && op <= 0xA3;
}

/* Returns the fused op that runs i32.const and then op, or local.get, i32.const, and op if lc;
 * 0 if none does. */
static uint32_t wasm__x_const_bin(uint32_t op, int lc) {
    switch (op) {
        case 0x6A:
            return lc ? WASM__OP_LC_ADD : WASM__OP_C_ADD;
        case 0x71:
            return lc ? WASM__OP_LC_AND : WASM__OP_C_AND;
        case 0x73:
            return lc ? WASM__OP_LC_XOR : WASM__OP_C_XOR;
        case 0x74:
            return lc ? WASM__OP_LC_SHL : WASM__OP_C_SHL;
        case 0x76:
            return lc ? WASM__OP_LC_SHR_U : WASM__OP_C_SHR_U;
        case 0x77:
            return lc ? WASM__OP_LC_ROTL : WASM__OP_C_ROTL;
        default:
            if (!wasm__x_is_i32_bin(op)) return 0;
            return lc ? WASM__OP_LC_BIN : WASM__OP_C_BIN;
    }
}

/* Returns the fused op that runs the len instructions with translated opcodes o, or 0. */
static uint32_t wasm__x_fusion(const uint32_t* o, uint32_t len) {
    int brif = o[len - 1u] == WASM__OP_BR_IF || o[len - 1u] == WASM__OP_BR_IF_LOOP;
    int loop = o[len - 1u] == WASM__OP_BR_IF_LOOP;

    if (len == 2) {
        if (o[0] == 0x41 && wasm__x_const_bin(o[1], 0)) return wasm__x_const_bin(o[1], 0);
        if (o[0] == 0x20 && o[1] == 0x6A) return WASM__OP_L_ADD;
        if (o[0] == 0x20 && wasm__x_is_i32_bin(o[1])) return WASM__OP_L_BIN;
        if (o[0] == 0x20 && wasm__x_is_f64_bin(o[1])) return WASM__OP_L_F64BIN;
        if (o[0] == 0x44 && wasm__x_is_f64_bin(o[1])) return WASM__OP_D_F64BIN;
        if (o[0] == 0x42 && o[1] == 0x7C) return WASM__OP_J_ADD;
        if (o[0] == 0x42 && wasm__x_is_i64_bin(o[1])) return WASM__OP_J_I64BIN;
        if (o[0] == 0x20 && o[1] == 0x21) return WASM__OP_L_S;
        if (o[0] == 0x20 && o[1] == 0x22) return WASM__OP_L_T;
        if (o[0] == 0x20 && (o[1] == 0x28 || o[1] == 0x2A || o[1] == 0x35)) return WASM__OP_L_LOAD32;
        if (o[0] == 0x20 && (o[1] == 0x29 || o[1] == 0x2B)) return WASM__OP_L_LOAD64;
        if (o[0] == 0x20 && o[1] == 0x2D) return WASM__OP_L_LOAD8U;
        if (o[0] == 0x6A && o[1] == 0x21) return WASM__OP_ADD_S;
        if (o[0] == 0x6A && o[1] == 0x22) return WASM__OP_ADD_T;
        if (o[0] == 0x21 && o[1] == 0x20) return WASM__OP_S_L;
        if (o[0] == 0x22 && o[1] == 0x20) return WASM__OP_T_L;
        if (o[0] == 0x22 && loop) return WASM__OP_T_BRIF_LOOP;
        return 0;
    }
    if (len == 3) {
        if (o[0] == 0x20 && o[1] == 0x41 && wasm__x_const_bin(o[2], 1)) return wasm__x_const_bin(o[2], 1);
        if (o[0] == 0x20 && o[1] == 0x20 && o[2] == 0x6A) return WASM__OP_LL_ADD;
        if (o[0] == 0x20 && o[1] == 0x20 && wasm__x_is_i32_bin(o[2])) return WASM__OP_LL_BIN;
        if (o[0] == 0x20 && o[1] == 0x20 && wasm__x_is_f64_bin(o[2])) return WASM__OP_LL_F64BIN;
        if (o[0] == 0x20 && o[1] == 0x42 && o[2] == 0x83) return WASM__OP_LJ_AND;
        if (o[0] == 0x20 && o[1] == 0x42 && o[2] == 0x7E) return WASM__OP_LJ_MUL;
        if (o[0] == 0x20 && o[1] == 0x42 && o[2] == 0x88) return WASM__OP_LJ_SHR_U;
        if (o[0] == 0x20 && o[1] == 0x42 && wasm__x_is_i64_bin(o[2])) return WASM__OP_LJ_I64BIN;
        if (o[0] == 0x41 && wasm__x_is_i32_cmp(o[1]) && brif)
            return loop ? WASM__OP_C_CMP_BRIF_LOOP : WASM__OP_C_CMP_BRIF;
        if (o[0] == 0x20 && wasm__x_is_i32_cmp(o[1]) && brif)
            return loop ? WASM__OP_L_CMP_BRIF_LOOP : WASM__OP_L_CMP_BRIF;
        return 0;
    }
    if (o[0] != 0x20 || (o[1] != 0x20 && o[1] != 0x41)) return 0;
    if (o[1] == 0x41 && o[2] == 0x6A && o[3] == 0x21) return WASM__OP_LC_ADD_S;
    if (o[1] == 0x41 && o[2] == 0x6A && o[3] == 0x22) return WASM__OP_LC_ADD_T;
    if (wasm__x_is_i32_bin(o[2]) && o[3] == 0x21) return o[1] == 0x41 ? WASM__OP_LC_BIN_S : WASM__OP_LL_BIN_S;
    if (!wasm__x_is_i32_cmp(o[2])) return 0;
    if (o[3] == 0x04) return o[1] == 0x41 ? WASM__OP_LC_CMP_IF : 0u;
    if (!brif) return 0;
    if (o[1] == 0x41) return loop ? WASM__OP_LC_CMP_BRIF_LOOP : WASM__OP_LC_CMP_BRIF;
    return loop ? WASM__OP_LL_CMP_BRIF_LOOP : WASM__OP_LL_CMP_BRIF;
}

/* Rewrites the first opcode word of each run of instructions that a fused op can run, choosing
 * the runs that leave the fewest ops to dispatch. */
static void wasm__x_fuse(wasm__translator_t* t) {
    uint32_t n = t->num_offsets;
    uint32_t* cost;
    uint32_t* pick;
    uint32_t i;

    if (n < 2u) return;
    cost = (uint32_t*)wasm__malloc_array(t->allocator, (size_t)n * 2u + 1u, sizeof(uint32_t));
    if (!cost) {
        t->failed = 1;
        return;
    }
    pick = cost + n + 1u;
    cost[n] = 0;
    for (i = n; i-- > 0;) {
        uint32_t o[4] = { 0, 0, 0, 0 };
        uint32_t len;

        cost[i] = cost[i + 1u] + 1u;
        pick[i] = 0;
        for (len = 0; len < 4u && i + len < n; len++) o[len] = t->ops[t->offsets[(i + len) * 2u]];
        for (len = 4u; len >= 2u; len--) {
            uint32_t fused = i + len <= n ? wasm__x_fusion(o, len) : 0u;

            if (fused != 0 && cost[i + len] + 1u < cost[i]) {
                cost[i] = cost[i + len] + 1u;
                pick[i] = fused | (len << 16);
            }
        }
    }
    for (i = 0; i < n;) {
        if (pick[i] == 0) {
            i++;
            continue;
        }
        t->ops[t->offsets[i * 2u]] = pick[i] & 0xFFFFu;
        i += pick[i] >> 16;
    }
    wasm__free(t->allocator, cost);
}

static wasm_error_t wasm__x_body(wasm__translator_t* t, wasm_module_t* mod, wasm_func_t* func,
                                 const wasm__body_info_t* info) {
    const uint32_t* heights = info->heights;
    int mem0_is_32 = mod->num_memories > 0 && !mod->memories[0].is_64;
    wasm__blocktype_t no_block;
    wasm__reader_t r;
    wasm_functype_t* ft = wasm__module_functype(mod, func->type_idx);

    if (!ft) return WASM_ERR_MALFORMED;
    memset(&no_block, 0, sizeof(no_block));
    no_block.result_arity = ft->num_results;
    no_block.results = ft->results;
    if (!wasm__x_push_frame(t, 0, 0, &no_block)) return WASM_ERR_OOM;

    r.ptr = func->code;
    r.end = func->code + func->code_len;
    r.malformed = 0;
    while (r.ptr < r.end && !t->failed) {
        const uint8_t* at = r.ptr;
        uint32_t start = t->len;
        uint32_t height = heights[at - func->code];
        uint8_t op = wasm__read_u8(&r);
        wasm__xframe_t* top = &t->frames[t->fp - 1u];

        if (op >= 0xBC && op <= 0xBF) continue;
        wasm__x_record_offset(t, (uint32_t)(at - func->code));
        switch (op) {
            case 0x00:
            case 0x01:
            case 0x0A:
            case 0x0F:
            case 0xD1:
            case 0xD3:
            case 0xD4:
                wasm__x_emit(t, op);
                break;
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x06: {
                wasm__blocktype_t blocktype;
                wasm__xframe_t* frame;

                if (wasm__read_blocktype(mod, &r, &blocktype) != WASM_OK) return WASM_ERR_MALFORMED;
                if (op == 0x04) height--;
                frame = wasm__x_push_frame(t, op, height, &blocktype);
                if (!frame) break;
                if (op == 0x04) {
                    wasm__x_emit(t, 0x04);
                    frame->if_fixup = wasm__x_emit(t, 0) + 1u;
                } else if (op == 0x06) {
                    t->try_operands = (uint32_t*)wasm__x_grow(t, t->try_operands, &t->tries_cap,
                                                              t->num_tries + 1u, sizeof(uint32_t));
                    if (t->failed) break;
                    frame->try_id = t->num_tries;
                    wasm__x_emit(t, WASM__OP_TRY);
                    t->try_operands[t->num_tries++] = wasm__x_emit(t, 0);
                    wasm__x_emit(t, frame->height);
                    wasm__x_open_handler(t);
                }
                frame->start = t->len;
                break;
            }
            case 0x05:
                wasm__x_emit(t, WASM__OP_BR);
                wasm__x_target(t, top);
                if (top->if_fixup) t->ops[top->if_fixup - 1u] = t->len;
                top->if_fixup = 0;
                break;
            case 0x07:
            case 0x19: {
                uint32_t tag_index = op == 0x07 ? wasm__read_leb128_u32(&r) : 0u;

                wasm__x_emit(t, WASM__OP_TRY_LEAVE);
                wasm__x_target(t, top);
                wasm__x_add_clause(t, top->try_id, op == 0x07 ? WASM__CLAUSE_CATCH : WASM__CLAUSE_CATCH_ALL,
                                   tag_index, t->len);
                top->in_catch = 1;
                break;
            }
            case 0x18: {
                uint32_t depth = wasm__read_leb128_u32(&r);
                const wasm__xframe_t* target;

                if (depth + 2u > t->fp) return WASM_ERR_MALFORMED;
                target = &t->frames[t->fp - 2u - depth];
                wasm__x_emit(t, WASM__OP_TRY_LEAVE);
                wasm__x_target(t, top);
                wasm__x_add_clause(t, top->try_id, WASM__CLAUSE_DELEGATE,
                                   top->handlers - target->handlers -
                                       ((target->opcode == 0x06 || target->opcode == 0x1F) ? 1u : 0u),
                                   0);
                break;
            }
            case 0x10:
            case 0x12: {
                uint32_t callee = wasm__read_leb128_u32(&r);

                if (callee >= mod->num_funcs) return WASM_ERR_MALFORMED;
                wasm__x_emit(t, op);
                wasm__x_emit(t, callee);
                wasm__x_param_slots(t, mod, mod->funcs[callee].type_idx);
                break;
            }
            case 0x14:
            case 0x15: {
                uint32_t type_idx = wasm__read_leb128_u32(&r);

                wasm__x_emit(t, op);
                wasm__x_emit(t, type_idx);
                wasm__x_param_slots(t, mod, type_idx);
                break;
            }
            case 0x1A:
            case 0x1B:
            case 0x1C: {
                uint32_t width = wasm__valtype_slots((wasm_valtype_t)info->operand_types[at - func->code]);

                if (op == 0x1C) wasm__skip_immediates(op, &r);
                if (info->operand_types[at - func->code] == WASM_TYPE_V128) {
                    wasm__x_emit(t, op == 0x1A ? WASM__OP_DROP_N : WASM__OP_SELECT_N);
                    wasm__x_emit(t, width);
                } else {
                    wasm__x_emit(t, op == 0x1A ? 0x1A : 0x1B);
                }
                break;
            }
            case 0x08:
            case 0x23:
            case 0x24:
            case 0x25:
            case 0x26:
            case 0x3F:
            case 0x40:
            case 0xD2:
                wasm__x_emit(t, op);
                wasm__x_emit(t, wasm__read_leb128_u32(&r));
                break;
            case 0x09: {
                uint32_t depth = wasm__read_leb128_u32(&r);
                uint32_t i;

                for (i = t->fp; i > 0; i--) {
                    const wasm__xframe_t* frame = &t->frames[i - 1u];

                    if (frame->opcode != 0x06 || !frame->in_catch) continue;
                    if (depth-- == 0) break;
                }
                if (i == 0) return WASM_ERR_MALFORMED;
                wasm__x_emit(t, 0x09);
                wasm__x_emit(t, t->handlers - t->frames[i - 1u].handlers - 1u);
                break;
            }
            case 0x0B:
                if (top->opcode == 0) {
                    wasm__x_emit(t, WASM__OP_END);
                } else if (top->opcode == 0x06 || top->opcode == 0x1F) {
                    wasm__x_emit(t, WASM__OP_TRY_END);
                    t->handlers--;
                }
                if (top->if_fixup) t->ops[top->if_fixup - 1u] = t->len;
                wasm__x_patch(t, top->fixups, t->len);
                t->fp--;
                break;
            case 0x0C: {
                uint32_t depth = wasm__read_leb128_u32(&r);

                if (depth >= t->fp) return WASM_ERR_MALFORMED;
                if (t->frames[t->fp - 1u - depth].opcode == 0)
                    wasm__x_emit(t, 0x0F);
                else
                    wasm__x_branch(t, WASM__OP_BR, WASM__OP_BR_LOOP, WASM__OP_BR_X, depth, height);
                break;
            }
            case 0x0D: {
                uint32_t depth = wasm__read_leb128_u32(&r);

                if (depth >= t->fp) return WASM_ERR_MALFORMED;
                wasm__x_branch(t, WASM__OP_BR_IF, WASM__OP_BR_IF_LOOP, WASM__OP_BR_IF_X, depth, height - 1u);
                break;
            }
            case 0x0E: {
                uint32_t count = wasm__read_leb128_u32(&r);
                uint32_t i;

                wasm__x_emit(t, WASM__OP_BR_TABLE);
                wasm__x_emit(t, count);
                for (i = 0; i <= count && !t->failed; i++) {
                    uint32_t depth = wasm__read_leb128_u32(&r);

                    if (depth >= t->fp || r.malformed) return WASM_ERR_MALFORMED;
                    wasm__x_descriptor(t, depth);
                }
                break;
            }
            case 0x11:
            case 0x13: {
                uint32_t type_idx = wasm__read_leb128_u32(&r);

                wasm__x_emit(t, op);
                wasm__x_emit(t, type_idx);
                wasm__x_emit(t, wasm__read_leb128_u32(&r));
                wasm__x_param_slots(t, mod, type_idx);
                break;
            }
            case 0x1F: {
                wasm__blocktype_t blocktype;
                wasm__xframe_t* frame;
                uint32_t count;
                uint32_t i;

                if (wasm__read_blocktype(mod, &r, &blocktype) != WASM_OK) return WASM_ERR_MALFORMED;
                count = wasm__read_leb128_u32(&r);
                wasm__x_emit(t, WASM__OP_TRY_TABLE);
                wasm__x_emit(t, height - wasm__valtypes_slots(blocktype.params, blocktype.param_arity));
                wasm__x_emit(t, count);
                for (i = 0; i < count && !t->failed; i++) {
                    wasm__try_table_clause_t clause;

                    if (wasm__read_try_table_clause(&r, &clause) != WASM_OK || clause.label_depth >= t->fp)
                        return WASM_ERR_MALFORMED;
                    wasm__x_emit(t, clause.kind);
                    wasm__x_emit(t, clause.tag_index);
                    wasm__x_descriptor(t, clause.label_depth);
                }
                frame = wasm__x_push_frame(t, op, height, &blocktype);
                if (!frame) break;
                wasm__x_open_handler(t);
                break;
            }
            case 0x20:
            case 0x21:
            case 0x22: {
                uint32_t index = wasm__read_leb128_u32(&r);

                if (index >= func->num_locals) return WASM_ERR_MALFORMED;
                if (func->locals[index] == WASM_TYPE_V128) {
                    wasm__x_emit(t, WASM__OP_LOCAL_GET_N + (uint32_t)(op - 0x20));
                    wasm__x_emit(t, t->local_slots[index]);
                    wasm__x_emit(t, wasm__valtype_slots(WASM_TYPE_V128));
                } else {
                    wasm__x_emit(t, op);
                    wasm__x_emit(t, t->local_slots[index]);
                }
                break;
            }
            case 0x41:
                wasm__x_emit(t, op);
                wasm__x_emit(t, (uint32_t)wasm__read_leb128_i32(&r));
                break;
            case 0x42: {
                uint64_t value = (uint64_t)wasm__read_leb128_i64(&r);

                wasm__x_emit(t, op);
                wasm__x_emit(t, (uint32_t)value);
                wasm__x_emit(t, (uint32_t)(value >> 32));
                break;
            }
            case 0x43:
                if (!wasm__has(&r, 4)) return WASM_ERR_MALFORMED;
                wasm__x_emit(t, op);
                wasm__x_emit(t, wasm__load_le32(r.ptr));
                r.ptr += 4;
                break;
            case 0x44: {
                uint64_t bits;

                if (!wasm__has(&r, 8)) return WASM_ERR_MALFORMED;
                bits = wasm__load_le64(r.ptr);
                r.ptr += 8;
                wasm__x_emit(t, op);
                wasm__x_emit(t, (uint32_t)bits);
                wasm__x_emit(t, (uint32_t)(bits >> 32));
                break;
            }
            case 0xD0: /* every null reference is a zero slot */
                wasm__x_emit(t, 0x42);
                wasm__x_emit(t, 0);
                wasm__x_emit(t, 0);
                wasm__skip_immediates(op, &r);
                break;
            case 0xD5:
            case 0xD6: {
                uint32_t depth = wasm__read_leb128_u32(&r);

                if (depth >= t->fp) return WASM_ERR_MALFORMED;
                wasm__x_emit(t, op);
                wasm__x_descriptor(t, depth);
                break;
            }
            case 0xFB:
            case 0xFC:
            case 0xFD:
            case 0xFE: {
                const uint8_t* immediates = r.ptr;
                uint32_t subop = wasm__read_leb128_u32(&r);

                if (op == 0xFB && (subop == 0x18 || subop == 0x19)) {
                    uint32_t flags_offset = (uint32_t)(r.ptr - func->code);
                    uint32_t depth;

                    (void)wasm__read_u8(&r);
                    depth = wasm__read_leb128_u32(&r);
                    if (depth >= t->fp || r.malformed) return WASM_ERR_MALFORMED;
                    wasm__x_emit(t, WASM__OP_BR_ON_CAST);
                    wasm__x_emit(t, subop);
                    wasm__x_emit(t, flags_offset);
                    wasm__x_descriptor(t, depth);
                } else {
                    wasm__x_emit(t, op);
                    wasm__x_emit(t, (uint32_t)(immediates - func->code));
                }
                r.ptr = immediates;
                wasm__skip_immediates(op, &r);
                break;
            }
            default:
                if (op >= 0x28 && op <= 0x3E) {
                    wasm__memarg_t memarg;

                    if (wasm__read_memarg(&r, &memarg) != WASM_OK) return WASM_ERR_MALFORMED;
                    if (memarg.memory_index == 0 && memarg.offset <= UINT32_MAX && mem0_is_32) {
                        wasm__x_emit(t, op);
                        wasm__x_emit(t, (uint32_t)memarg.offset);
                    } else {
                        wasm__x_emit(t, WASM__OP_MEMORY_X);
                        wasm__x_emit(t, op);
                        wasm__x_emit(t, memarg.memory_index);
                        wasm__x_emit(t, (uint32_t)memarg.offset);
                        wasm__x_emit(t, (uint32_t)(memarg.offset >> 32));
                    }
                    break;
                }
                if (op >= 0x45 && op <= 0xC4) {
                    wasm__x_emit(t, op);
                    break;
                }
                return WASM_ERR_MALFORMED;
        }
        if (r.malformed) return WASM_ERR_MALFORMED;
        if (t->len == start && !t->failed) t->num_offsets--;
    }
    if (t->failed) return WASM_ERR_OOM;
    if (t->fp != 0) return WASM_ERR_MALFORMED;
    wasm__x_fuse(t);
    return t->failed ? WASM_ERR_OOM : WASM_OK;
}

/* Appends each legacy try's clause table after the body and points its WASM__OP_TRY at it. */
static void wasm__x_try_tables(wasm__translator_t* t) {
    uint32_t id;

    for (id = 0; id < t->num_tries && !t->failed; id++) {
        uint32_t count = 0;
        uint32_t i;

        t->ops[t->try_operands[id]] = t->len;
        for (i = 0; i < t->num_clauses; i++) count += t->clauses[i].try_id == id;
        wasm__x_emit(t, count);
        for (i = 0; i < t->num_clauses; i++) {
            if (t->clauses[i].try_id != id) continue;
            wasm__x_emit(t, t->clauses[i].kind);
            wasm__x_emit(t, t->clauses[i].imm);
            wasm__x_emit(t, t->clauses[i].body);
        }
    }
}

WASM_PRIVATE wasm_error_t wasm__translate_function(wasm_module_t* mod, uint32_t func_idx, const wasm__body_info_t* info) {
    wasm__translator_t t;
    wasm_func_t* func = &mod->funcs[func_idx];
    const wasm_functype_t* ft = wasm__module_functype(mod, func->type_idx);
    uint32_t* maps;
    uint32_t* map_bits;
    wasm_error_t err = WASM_OK;

    if (!ft) return WASM_ERR_MALFORMED;
    memset(&t, 0, sizeof(t));
    t.allocator = &mod->rt->allocator;
    wasm__x_local_slots(&t, func);
    maps = wasm__x_copy_words(t.allocator, info->maps, info->num_maps * 3u, &t.failed);
    map_bits = wasm__x_copy_words(t.allocator, info->map_bits, info->num_map_bits, &t.failed);
    if (t.failed) err = WASM_ERR_OOM;
    if (err == WASM_OK) err = wasm__x_body(&t, mod, func, info);
    if (err == WASM_OK) {
        wasm__x_try_tables(&t);
        if (t.failed) err = WASM_ERR_OOM;
    }
    if (err != WASM_OK) {
        wasm__free(t.allocator, maps);
        wasm__free(t.allocator, map_bits);
        wasm__x_free(&t);
        return err;
    }
    wasm__free_translation(&mod->rt->allocator, func);
    func->ops = t.ops;
    func->num_ops = t.len;
    func->op_offsets = t.offsets;
    func->num_op_offsets = t.num_offsets;
    func->max_handlers = t.max_handlers;
    func->max_stack = info->max_height;
    func->local_slots = t.local_slots;
    func->num_local_slots = t.num_local_slots;
    func->param_slots = wasm__valtypes_slots(ft->params, ft->num_params);
    func->result_slots = wasm__valtypes_slots(ft->results, ft->num_results);
    func->stack_maps = maps;
    func->num_stack_maps = info->num_maps;
    func->stack_map_bits = map_bits;
    func->num_stack_map_bits = info->num_map_bits;
    t.ops = NULL;
    t.offsets = NULL;
    t.local_slots = NULL;
    wasm__x_free(&t);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__copy_translation(const wasm_allocator_t* a, wasm_func_t* dst, const wasm_func_t* src) {
    uint32_t* ops = NULL;
    uint32_t* offsets = NULL;
    uint32_t* local_slots = NULL;
    uint32_t* maps = NULL;
    uint32_t* map_bits = NULL;
    int failed = 0;

    if (src->ops) {
        ops = wasm__x_copy_words(a, src->ops, src->num_ops, &failed);
        offsets = wasm__x_copy_words(a, src->op_offsets, src->num_op_offsets * 2u, &failed);
        local_slots = src->local_slots ? wasm__x_copy_words(a, src->local_slots, src->num_locals, &failed) : NULL;
        maps = wasm__x_copy_words(a, src->stack_maps, src->num_stack_maps * 3u, &failed);
        map_bits = wasm__x_copy_words(a, src->stack_map_bits, src->num_stack_map_bits, &failed);
        if (failed) {
            wasm__free(a, ops);
            wasm__free(a, offsets);
            wasm__free(a, local_slots);
            wasm__free(a, maps);
            wasm__free(a, map_bits);
            return WASM_ERR_OOM;
        }
    }
    wasm__free_translation(a, dst);
    dst->ops = ops;
    dst->num_ops = src->num_ops;
    dst->op_offsets = offsets;
    dst->num_op_offsets = src->num_op_offsets;
    dst->max_handlers = src->max_handlers;
    dst->max_stack = src->max_stack;
    dst->local_slots = local_slots;
    dst->num_local_slots = src->num_local_slots;
    dst->param_slots = src->param_slots;
    dst->result_slots = src->result_slots;
    dst->stack_maps = maps;
    dst->num_stack_maps = src->num_stack_maps;
    dst->stack_map_bits = map_bits;
    dst->num_stack_map_bits = src->num_stack_map_bits;
    return WASM_OK;
}

WASM_PRIVATE void wasm__free_translation(const wasm_allocator_t* a, wasm_func_t* func) {
    wasm__free(a, func->ops);
    wasm__free(a, func->op_offsets);
    wasm__free(a, func->local_slots);
    wasm__free(a, func->stack_maps);
    wasm__free(a, func->stack_map_bits);
    func->ops = NULL;
    func->op_offsets = NULL;
    func->local_slots = NULL;
    func->stack_maps = NULL;
    func->stack_map_bits = NULL;
    func->num_ops = 0;
    func->num_op_offsets = 0;
    func->max_handlers = 0;
    func->max_stack = 0;
    func->num_local_slots = 0;
    func->num_stack_maps = 0;
    func->num_stack_map_bits = 0;
}

WASM_PRIVATE uint32_t wasm__translated_offset(const wasm_func_t* func, const uint32_t* op) {
    uint32_t index;
    uint32_t lo = 0;
    uint32_t hi;

    if (!func || !func->ops || !op || func->num_op_offsets == 0 || op < func->ops) return 0;
    index = (uint32_t)(op - func->ops);
    hi = func->num_op_offsets;
    while (hi - lo > 1u) {
        uint32_t mid = lo + (hi - lo) / 2u;

        if (func->op_offsets[mid * 2u] <= index)
            lo = mid;
        else
            hi = mid;
    }
    return func->op_offsets[lo * 2u + 1u];
}
