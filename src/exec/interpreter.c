#include "interpreter.h"
#include <stdio.h>
#include "bulk.h"
#include "calls.h"
#include "translate.h"
#include "exceptions.h"
#include "frames.h"
#include "gc.h"
#include "simd.h"
#include "slots.h"
#include "atomic.h"
#include "../atomics.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../memory_internal.h"
#include "../module.h"
#include "../portable.h"
#include "../reader.h"
#include "../runtime.h"
#include "../support.h"
#include "../types/canonical.h"
#include "../types/cross_module.h"
#include "../types/references.h"
#include "../values.h"
#include <string.h>

WASM_INLINE void wasm__drop_labels(wasm__label_t* labels, uint32_t lsp, uint32_t count) {
    while (count-- > 0) {
        if (labels[lsp - 1].caught_values || labels[lsp - 1].active_catch) wasm__clear_label(&labels[lsp - 1]);
        lsp--;
    }
}

WASM_INLINE int wasm__interrupt_pending(wasm_runtime_t* rt) {
#if defined(__GNUC__) || defined(__clang__)
    if (__atomic_load_n(&rt->interrupt_requested, __ATOMIC_RELAXED) == 0u) return 0;
#else
    if (*(const volatile size_t*)&rt->interrupt_requested == 0u) return 0;
#endif
    return wasm__runtime_take_interrupt(rt);
}

/* Calls a host import of type ft, whose type indices belong to mod. Results start as typed
 * defaults, so a host may set only the payload, and must match ft when it returns. */
static wasm_error_t wasm__invoke_host_func(wasm_module_t* mod,
                                           const wasm_functype_t* ft,
                                           wasm_host_func_t host_func,
                                           const wasm_value_t* args,
                                           wasm_value_t* results,
                                           void* userdata) {
    wasm_error_t err;
    uint32_t i;

    if (!mod || !mod->rt || !ft || !host_func) return WASM_ERR_MALFORMED;
    for (i = 0; i < ft->num_results; i++) {
        const wasm_reftype_t* reftype = ft->result_reftypes ? &ft->result_reftypes[i] : NULL;

        results[i] = wasm__is_ref_type(ft->results[i]) ? wasm__typed_ref_null(ft->results[i], reftype)
                                                       : wasm__default_value(ft->results[i]);
    }
    mod->rt->nonresumable_depth++;
    err = host_func(mod, args, ft->num_params, results, ft->num_results, userdata);
    mod->rt->nonresumable_depth--;
    if (err != WASM_OK) return err;
    for (i = 0; i < ft->num_results; i++) {
        const wasm_reftype_t* reftype = ft->result_reftypes ? &ft->result_reftypes[i] : NULL;

        if (!wasm__runtime_value_matches_type(mod, &results[i], ft->results[i], reftype)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "host function result %u does not match its declared type", (unsigned)i);
            return WASM_ERR_TYPE_MISMATCH;
        }
    }
    return WASM_OK;
}

/* Returns the address of width bytes at base + offset, or NULL if out of bounds. */
static uint8_t* wasm__memory_access_slow(wasm_module_t* mod,
                                         uint32_t memory_index,
                                         const wasm__slot_t* base,
                                         uint64_t offset,
                                         uint32_t width,
                                         uint8_t** mem0_data,
                                         uint64_t* mem0_size) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    uint64_t addr;

    if (!memory) return NULL;
    if (memory_index == 0 && !memory->is_64) {
        *mem0_data = memory->data;
        *mem0_size = wasm__memory_byte_size(memory);
    }
    if (!wasm__memory_effective_addr(memory, wasm__slot_index(base, memory->is_64), offset, &addr)) return NULL;
    if (!wasm__memory_access_in_bounds(addr, width, wasm__memory_byte_size(memory))) return NULL;
    return memory->data + addr;
}

static uint32_t wasm__memory_width(uint32_t op) {
    static const uint8_t widths[] = { 4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4, 4, 8, 4, 8, 1, 2, 1, 2, 4 };

    return widths[op - 0x28];
}

/* op is a load or store opcode (0x28 to 0x3E) and p the address it accesses. */
static void wasm__memory_transfer(uint32_t op, uint8_t* p, wasm__slot_t* sp) {
    switch (op) {
        case 0x28:
            wasm__slot_set_u32(sp - 1, wasm__load_le32(p));
            break;
        case 0x29:
            wasm__slot_set_u64(sp - 1, wasm__load_le64(p));
            break;
        case 0x2A:
            wasm__slot_set_f32_bits(sp - 1, wasm__load_le32(p));
            break;
        case 0x2B:
            wasm__slot_set_f64_bits(sp - 1, wasm__load_le64(p));
            break;
        case 0x2C:
            wasm__slot_set_i32(sp - 1, (int8_t)*p);
            break;
        case 0x2D:
            wasm__slot_set_u32(sp - 1, *p);
            break;
        case 0x2E:
            wasm__slot_set_i32(sp - 1, (int16_t)wasm__load_le16(p));
            break;
        case 0x2F:
            wasm__slot_set_u32(sp - 1, wasm__load_le16(p));
            break;
        case 0x30:
            wasm__slot_set_i64(sp - 1, (int8_t)*p);
            break;
        case 0x31:
            wasm__slot_set_u64(sp - 1, *p);
            break;
        case 0x32:
            wasm__slot_set_i64(sp - 1, (int16_t)wasm__load_le16(p));
            break;
        case 0x33:
            wasm__slot_set_u64(sp - 1, wasm__load_le16(p));
            break;
        case 0x34:
            wasm__slot_set_i64(sp - 1, (int32_t)wasm__load_le32(p));
            break;
        case 0x35:
            wasm__slot_set_u64(sp - 1, wasm__load_le32(p));
            break;
        case 0x36:
            wasm__store_le32(p, wasm__slot_u32(sp - 1));
            break;
        case 0x37:
            wasm__store_le64(p, wasm__slot_u64(sp - 1));
            break;
        case 0x38:
            wasm__store_le32(p, wasm__slot_f32_bits(sp - 1));
            break;
        case 0x39:
            wasm__store_le64(p, wasm__slot_f64_bits(sp - 1));
            break;
        case 0x3A:
            *p = (uint8_t)wasm__slot_u32(sp - 1);
            break;
        case 0x3B:
            wasm__store_le16(p, (uint16_t)wasm__slot_u32(sp - 1));
            break;
        case 0x3C:
            *p = (uint8_t)wasm__slot_u64(sp - 1);
            break;
        case 0x3D:
            wasm__store_le16(p, (uint16_t)wasm__slot_u64(sp - 1));
            break;
        case 0x3E:
            wasm__store_le32(p, (uint32_t)wasm__slot_u64(sp - 1));
            break;
        default:
            break;
    }
}

/* Returns the bits of r, an f64 result computed from operands with bits a and b; a NaN result
 * takes the payload of a if a is a NaN, else of b if b is one, made quiet. */
WASM_INLINE uint64_t wasm__f64_result_bits(double r, uint64_t a, uint64_t b) {
    uint64_t bits;

    if (r != r) {
        if ((a & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000))
            return a | UINT64_C(0x0008000000000000);
        if ((b & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000))
            return b | UINT64_C(0x0008000000000000);
    }
    memcpy(&bits, &r, sizeof(bits));
    return bits;
}

/* args holds the slots of ft's parameters just above rt->sp. */
static wasm_error_t wasm__call_host_from_stack(wasm_module_t* mod,
                                               const wasm_functype_t* ft,
                                               const wasm_func_t* func,
                                               const wasm__slot_t* args) {
    wasm_runtime_t* rt = mod->rt;
    wasm_value_t args_buf[8];
    wasm_value_t results_buf[8];
    wasm_value_t* call_args = args_buf;
    wasm_value_t* call_results = results_buf;
    wasm_error_t err = WASM_ERR_OOM;

    if (ft->num_params > 8) call_args = (wasm_value_t*)wasm__malloc_array(&mod->rt->allocator, ft->num_params, sizeof(wasm_value_t));
    if (ft->num_results > 8) call_results = (wasm_value_t*)wasm__malloc_array(&mod->rt->allocator, ft->num_results, sizeof(wasm_value_t));
    if (call_args && call_results) {
        wasm__values_from_slots(rt, ft->params, ft->param_reftypes, ft->num_params, args, call_args);
        wasm__values_to_host(rt, call_args, ft->num_params);
        err = wasm__invoke_host_func(mod, ft, func->host_func, call_args, call_results, func->host_userdata);
        if (err == WASM_OK) err = wasm__values_from_host(rt, call_results, ft->num_results);
        if (err == WASM_OK && wasm__valtypes_slots(ft->results, ft->num_results) > rt->max_stack - rt->sp)
            err = WASM_ERR_STACK_OVERFLOW;
        if (err == WASM_OK) rt->sp += wasm__slots_from_values(&rt->stack[rt->sp], call_results, ft->num_results);
    }
    if (call_args != args_buf) wasm__free(&mod->rt->allocator, call_args);
    if (call_results != results_buf) wasm__free(&mod->rt->allocator, call_results);
    return err;
}

/* Hands an exception raised by a host import to the nearest Wasm handler. A tail call has
   already given up its frame, so that frame's handlers are skipped. Returns
   WASM__ERR_RESUME_FRAME once a handler is found. */
static wasm_error_t wasm__unwind_host_exception(wasm_module_t* mod,
                                                wasm__call_frame_t* cf,
                                                uint32_t base_frame_sp,
                                                int is_tail_call) {
    wasm_error_t err;

    if (is_tail_call) {
        while (cf->lsp > 0) {
            wasm__clear_label(&cf->labels[cf->lsp - 1]);
            cf->lsp--;
        }
    }
    err = wasm__handle_exception_frames(mod, base_frame_sp);
    return err == WASM_OK ? WASM__ERR_RESUME_FRAME : err;
}

/* Must run before anything outside the loop reads the frame or rt->sp. */
#define WASM__SAVE_STATE()                   \
    do {                                     \
        cf->pc = pc;                         \
        cf->op_at = op_at;                   \
        rt->sp = (uint32_t)(sp - stack);     \
        cf->lsp = lsp;                       \
    } while (0)
#define WASM__LOAD_STATE()                   \
    do {                                     \
        pc = cf->pc;                         \
        sp = stack + rt->sp;                 \
        lsp = cf->lsp;                       \
        WASM__LOAD_FUEL();                   \
    } while (0)
#define WASM__ENTER_FRAME()                           \
    do {                                              \
        cf = &rt->call_frames[rt->call_frame_sp - 1]; \
        mod = cf->module;                             \
        func = cf->func;                              \
        ops = func->ops;                              \
        labels = cf->labels;                          \
        locals = cf->locals;                          \
        sp_base = cf->sp_base;                        \
        op_at = cf->op_at;                            \
        mem0_size = 0;                                \
        WASM__LOAD_STATE();                           \
    } while (0)
/* Prefixed instructions must set cf->r to their immediates first. A fuel or interrupt stop from expr
 * must leave the instruction's operands below their original stack height untouched. */
#define WASM__CALL_OUT(expr)                                                    \
    do {                                                                        \
        wasm__slot_t* call_sp_ = sp;                                            \
        WASM__SAVE_STATE();                                                     \
        err = (expr);                                                           \
        WASM__LOAD_STATE();                                                     \
        mem0_size = 0;                                                          \
        if (err != WASM_OK) {                                                   \
            if (err != WASM_ERR_OUT_OF_FUEL && err != WASM_ERR_INTERRUPTED)     \
                goto cleanup;                                                   \
            sp = call_sp_;                                                      \
            pc = op_at;                                                         \
            goto suspend;                                                       \
        }                                                                       \
    } while (0)
/* Only for 32-bit memory 0 with an offset below 2^32; a zero mem0_size forces a refresh. */
#define WASM__MEMORY_PTR(p, base, width)                                                         \
    do {                                                                                         \
        uint64_t addr_ = (uint64_t)wasm__slot_u32(base) + *pc++;                                 \
        if (addr_ + (width) <= mem0_size) {                                                      \
            (p) = mem0_data + addr_;                                                             \
        } else {                                                                                 \
            (p) = wasm__memory_access_slow(mod, 0, (base), pc[-1], (width), &mem0_data, &mem0_size); \
            if (!(p)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);                                        \
        }                                                                                        \
    } while (0)
/* Replaces the top slot, read as get into a of type, with expr stored by set. */
#define WASM__UNARY(type, get, set, expr)        \
    do {                                         \
        type a = wasm__slot_##get(sp - 1);       \
        wasm__slot_set_##set(sp - 1, expr);      \
    } while (0)
/* Replaces the top two f64 slots, a below b, with expr, whose NaN results follow wasm__f64_result_bits. */
#define WASM__F64_BINARY(expr)                                                              \
    do {                                                                                    \
        double b = wasm__slot_f64(sp - 1);                                                  \
        double a = wasm__slot_f64(sp - 2);                                                  \
        uint64_t bits_ = wasm__f64_result_bits(expr, wasm__slot_f64_bits(sp - 2),           \
                                               wasm__slot_f64_bits(sp - 1));                \
        sp--;                                                                               \
        wasm__slot_set_f64_bits(sp - 1, bits_);                                             \
    } while (0)
/* Replaces the top two slots, a below b, with expr. */
#define WASM__BINARY(type, get, set, expr)       \
    do {                                         \
        type b = wasm__slot_##get(sp - 1);       \
        type a = wasm__slot_##get(sp - 2);       \
        sp--;                                    \
        wasm__slot_set_##set(sp - 1, expr);      \
    } while (0)

#if defined(__GNUC__) || defined(__clang__)
#define WASM__THREADED_DISPATCH 1
#define WASM__CASE(op) wasm__op_##op
#define WASM__DEFAULT wasm__op_default
/* fuel_prev holds the instruction an out-of-fuel trap reports. */
#define WASM__NEXT()        \
    do {                    \
        op_at = pc;         \
        op = *pc++;         \
        goto* dispatch[op]; \
    } while (0)
#define WASM__LOAD_FUEL()                                                         \
    do {                                                                          \
        dispatch = rt->fuel_enabled ? wasm__fuel_dispatch : wasm__dispatch;       \
        fuel_prev = op_at;                                                        \
    } while (0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#else
#define WASM__THREADED_DISPATCH 0
#define WASM__CASE(op) case op
#define WASM__DEFAULT default
#define WASM__NEXT() continue
#define WASM__LOAD_FUEL() (fuel_on = rt->fuel_enabled)
#endif

/* Returns WASM__ERR_RETURN_FRAME with results on the stack once the frame above base_frame_sp returns.
 * If resumable, fuel, interrupt, and yield stops leave the frames in place with rt->suspended set. */
static wasm_error_t wasm__interp_loop(wasm_runtime_t* rt, uint32_t base_frame_sp, int resumable) {
    wasm__slot_t* const stack = rt->stack;
    wasm__call_frame_t* cf;
    wasm_module_t* mod;
    wasm_func_t* func;
    wasm__label_t* labels;
    wasm__slot_t* locals;
    wasm__slot_t* sp;
    const uint32_t* ops;
    const uint32_t* pc;
    const uint32_t* op_at;
    const uint32_t* desc;
    wasm_module_t* call_mod;
    wasm_functype_t* call_type;
    uint32_t call_idx;
    uint32_t call_slots;
    uint8_t* mem0_data = NULL;
    uint64_t mem0_size = 0;
    uint32_t lsp;
    uint32_t sp_base;
    uint32_t op;
    uint32_t i;
    wasm_error_t err = WASM_OK;
#if WASM__THREADED_DISPATCH
    const void* const* dispatch;
    const uint32_t* fuel_prev;
    static const void* const wasm__dispatch[WASM__OP_COUNT] = {
        [0x00] = &&wasm__op_0x00, [0x01] = &&wasm__op_0x01, [0x02] = &&wasm__op_default,
        [0x03] = &&wasm__op_default, [0x04] = &&wasm__op_0x04, [0x05] = &&wasm__op_default,
        [0x06] = &&wasm__op_default, [0x07] = &&wasm__op_default, [0x08] = &&wasm__op_0x08,
        [0x09] = &&wasm__op_0x09, [0x0A] = &&wasm__op_0x0A, [0x0B] = &&wasm__op_default,
        [0x0C] = &&wasm__op_default, [0x0D] = &&wasm__op_default, [0x0E] = &&wasm__op_default,
        [0x0F] = &&wasm__op_0x0F, [0x10] = &&wasm__op_0x10, [0x11] = &&wasm__op_0x11, [0x12] = &&wasm__op_0x12,
        [0x13] = &&wasm__op_0x13, [0x14] = &&wasm__op_0x14, [0x15] = &&wasm__op_0x15, [0x16] = &&wasm__op_default,
        [0x17] = &&wasm__op_default, [0x18] = &&wasm__op_default, [0x19] = &&wasm__op_default,
        [0x1A] = &&wasm__op_0x1A, [0x1B] = &&wasm__op_0x1B, [0x1C] = &&wasm__op_default,
        [0x1D] = &&wasm__op_default, [0x1E] = &&wasm__op_default, [0x1F] = &&wasm__op_default,
        [0x20] = &&wasm__op_0x20, [0x21] = &&wasm__op_0x21, [0x22] = &&wasm__op_0x22, [0x23] = &&wasm__op_0x23,
        [0x24] = &&wasm__op_0x24, [0x25] = &&wasm__op_0x25, [0x26] = &&wasm__op_0x26, [0x27] = &&wasm__op_default,
        [0x28] = &&wasm__op_0x28, [0x29] = &&wasm__op_0x29, [0x2A] = &&wasm__op_0x2A, [0x2B] = &&wasm__op_0x2B,
        [0x2C] = &&wasm__op_0x2C, [0x2D] = &&wasm__op_0x2D, [0x2E] = &&wasm__op_0x2E, [0x2F] = &&wasm__op_0x2F,
        [0x30] = &&wasm__op_0x30, [0x31] = &&wasm__op_0x31, [0x32] = &&wasm__op_0x32, [0x33] = &&wasm__op_0x33,
        [0x34] = &&wasm__op_0x34, [0x35] = &&wasm__op_0x35, [0x36] = &&wasm__op_0x36, [0x37] = &&wasm__op_0x37,
        [0x38] = &&wasm__op_0x38, [0x39] = &&wasm__op_0x39, [0x3A] = &&wasm__op_0x3A, [0x3B] = &&wasm__op_0x3B,
        [0x3C] = &&wasm__op_0x3C, [0x3D] = &&wasm__op_0x3D, [0x3E] = &&wasm__op_0x3E, [0x3F] = &&wasm__op_0x3F,
        [0x40] = &&wasm__op_0x40, [0x41] = &&wasm__op_0x41, [0x42] = &&wasm__op_0x42, [0x43] = &&wasm__op_0x43,
        [0x44] = &&wasm__op_0x44, [0x45] = &&wasm__op_0x45, [0x46] = &&wasm__op_0x46, [0x47] = &&wasm__op_0x47,
        [0x48] = &&wasm__op_0x48, [0x49] = &&wasm__op_0x49, [0x4A] = &&wasm__op_0x4A, [0x4B] = &&wasm__op_0x4B,
        [0x4C] = &&wasm__op_0x4C, [0x4D] = &&wasm__op_0x4D, [0x4E] = &&wasm__op_0x4E, [0x4F] = &&wasm__op_0x4F,
        [0x50] = &&wasm__op_0x50, [0x51] = &&wasm__op_0x51, [0x52] = &&wasm__op_0x52, [0x53] = &&wasm__op_0x53,
        [0x54] = &&wasm__op_0x54, [0x55] = &&wasm__op_0x55, [0x56] = &&wasm__op_0x56, [0x57] = &&wasm__op_0x57,
        [0x58] = &&wasm__op_0x58, [0x59] = &&wasm__op_0x59, [0x5A] = &&wasm__op_0x5A, [0x5B] = &&wasm__op_0x5B,
        [0x5C] = &&wasm__op_0x5C, [0x5D] = &&wasm__op_0x5D, [0x5E] = &&wasm__op_0x5E, [0x5F] = &&wasm__op_0x5F,
        [0x60] = &&wasm__op_0x60, [0x61] = &&wasm__op_0x61, [0x62] = &&wasm__op_0x62, [0x63] = &&wasm__op_0x63,
        [0x64] = &&wasm__op_0x64, [0x65] = &&wasm__op_0x65, [0x66] = &&wasm__op_0x66, [0x67] = &&wasm__op_0x67,
        [0x68] = &&wasm__op_0x68, [0x69] = &&wasm__op_0x69, [0x6A] = &&wasm__op_0x6A, [0x6B] = &&wasm__op_0x6B,
        [0x6C] = &&wasm__op_0x6C, [0x6D] = &&wasm__op_0x6D, [0x6E] = &&wasm__op_0x6E, [0x6F] = &&wasm__op_0x6F,
        [0x70] = &&wasm__op_0x70, [0x71] = &&wasm__op_0x71, [0x72] = &&wasm__op_0x72, [0x73] = &&wasm__op_0x73,
        [0x74] = &&wasm__op_0x74, [0x75] = &&wasm__op_0x75, [0x76] = &&wasm__op_0x76, [0x77] = &&wasm__op_0x77,
        [0x78] = &&wasm__op_0x78, [0x79] = &&wasm__op_0x79, [0x7A] = &&wasm__op_0x7A, [0x7B] = &&wasm__op_0x7B,
        [0x7C] = &&wasm__op_0x7C, [0x7D] = &&wasm__op_0x7D, [0x7E] = &&wasm__op_0x7E, [0x7F] = &&wasm__op_0x7F,
        [0x80] = &&wasm__op_0x80, [0x81] = &&wasm__op_0x81, [0x82] = &&wasm__op_0x82, [0x83] = &&wasm__op_0x83,
        [0x84] = &&wasm__op_0x84, [0x85] = &&wasm__op_0x85, [0x86] = &&wasm__op_0x86, [0x87] = &&wasm__op_0x87,
        [0x88] = &&wasm__op_0x88, [0x89] = &&wasm__op_0x89, [0x8A] = &&wasm__op_0x8A, [0x8B] = &&wasm__op_0x8B,
        [0x8C] = &&wasm__op_0x8C, [0x8D] = &&wasm__op_0x8D, [0x8E] = &&wasm__op_0x8E, [0x8F] = &&wasm__op_0x8F,
        [0x90] = &&wasm__op_0x90, [0x91] = &&wasm__op_0x91, [0x92] = &&wasm__op_0x92, [0x93] = &&wasm__op_0x93,
        [0x94] = &&wasm__op_0x94, [0x95] = &&wasm__op_0x95, [0x96] = &&wasm__op_0x96, [0x97] = &&wasm__op_0x97,
        [0x98] = &&wasm__op_0x98, [0x99] = &&wasm__op_0x99, [0x9A] = &&wasm__op_0x9A, [0x9B] = &&wasm__op_0x9B,
        [0x9C] = &&wasm__op_0x9C, [0x9D] = &&wasm__op_0x9D, [0x9E] = &&wasm__op_0x9E, [0x9F] = &&wasm__op_0x9F,
        [0xA0] = &&wasm__op_0xA0, [0xA1] = &&wasm__op_0xA1, [0xA2] = &&wasm__op_0xA2, [0xA3] = &&wasm__op_0xA3,
        [0xA4] = &&wasm__op_0xA4, [0xA5] = &&wasm__op_0xA5, [0xA6] = &&wasm__op_0xA6, [0xA7] = &&wasm__op_0xA7,
        [0xA8] = &&wasm__op_0xA8, [0xA9] = &&wasm__op_0xA9, [0xAA] = &&wasm__op_0xAA, [0xAB] = &&wasm__op_0xAB,
        [0xAC] = &&wasm__op_0xAC, [0xAD] = &&wasm__op_0xAD, [0xAE] = &&wasm__op_0xAE, [0xAF] = &&wasm__op_0xAF,
        [0xB0] = &&wasm__op_0xB0, [0xB1] = &&wasm__op_0xB1, [0xB2] = &&wasm__op_0xB2, [0xB3] = &&wasm__op_0xB3,
        [0xB4] = &&wasm__op_0xB4, [0xB5] = &&wasm__op_0xB5, [0xB6] = &&wasm__op_0xB6, [0xB7] = &&wasm__op_0xB7,
        [0xB8] = &&wasm__op_0xB8, [0xB9] = &&wasm__op_0xB9, [0xBA] = &&wasm__op_0xBA, [0xBB] = &&wasm__op_0xBB,
        [0xBC] = &&wasm__op_default, [0xBD] = &&wasm__op_default, [0xBE] = &&wasm__op_default, [0xBF] = &&wasm__op_default,
        [0xC0] = &&wasm__op_0xC0, [0xC1] = &&wasm__op_0xC1, [0xC2] = &&wasm__op_0xC2, [0xC3] = &&wasm__op_0xC3,
        [0xC4] = &&wasm__op_0xC4, [0xC5] = &&wasm__op_default, [0xC6] = &&wasm__op_default,
        [0xC7] = &&wasm__op_default, [0xC8] = &&wasm__op_default, [0xC9] = &&wasm__op_default,
        [0xCA] = &&wasm__op_default, [0xCB] = &&wasm__op_default, [0xCC] = &&wasm__op_default,
        [0xCD] = &&wasm__op_default, [0xCE] = &&wasm__op_default, [0xCF] = &&wasm__op_default,
        [0xD0] = &&wasm__op_default, [0xD1] = &&wasm__op_0xD1, [0xD2] = &&wasm__op_0xD2, [0xD3] = &&wasm__op_0xD3,
        [0xD4] = &&wasm__op_0xD4, [0xD5] = &&wasm__op_0xD5, [0xD6] = &&wasm__op_0xD6, [0xD7] = &&wasm__op_default,
        [0xD8] = &&wasm__op_default, [0xD9] = &&wasm__op_default, [0xDA] = &&wasm__op_default,
        [0xDB] = &&wasm__op_default, [0xDC] = &&wasm__op_default, [0xDD] = &&wasm__op_default,
        [0xDE] = &&wasm__op_default, [0xDF] = &&wasm__op_default, [0xE0] = &&wasm__op_default,
        [0xE1] = &&wasm__op_default, [0xE2] = &&wasm__op_default, [0xE3] = &&wasm__op_default,
        [0xE4] = &&wasm__op_default, [0xE5] = &&wasm__op_default, [0xE6] = &&wasm__op_default,
        [0xE7] = &&wasm__op_default, [0xE8] = &&wasm__op_default, [0xE9] = &&wasm__op_default,
        [0xEA] = &&wasm__op_default, [0xEB] = &&wasm__op_default, [0xEC] = &&wasm__op_default,
        [0xED] = &&wasm__op_default, [0xEE] = &&wasm__op_default, [0xEF] = &&wasm__op_default,
        [0xF0] = &&wasm__op_default, [0xF1] = &&wasm__op_default, [0xF2] = &&wasm__op_default,
        [0xF3] = &&wasm__op_default, [0xF4] = &&wasm__op_default, [0xF5] = &&wasm__op_default,
        [0xF6] = &&wasm__op_default, [0xF7] = &&wasm__op_default, [0xF8] = &&wasm__op_default,
        [0xF9] = &&wasm__op_default, [0xFA] = &&wasm__op_default, [0xFB] = &&wasm__op_0xFB,
        [0xFC] = &&wasm__op_0xFC, [0xFD] = &&wasm__op_0xFD, [0xFE] = &&wasm__op_0xFE, [0xFF] = &&wasm__op_default,
        [WASM__OP_BR] = &&wasm__op_WASM__OP_BR, [WASM__OP_BR_LOOP] = &&wasm__op_WASM__OP_BR_LOOP,
        [WASM__OP_BR_X] = &&wasm__op_WASM__OP_BR_X, [WASM__OP_BR_IF] = &&wasm__op_WASM__OP_BR_IF,
        [WASM__OP_BR_IF_LOOP] = &&wasm__op_WASM__OP_BR_IF_LOOP, [WASM__OP_BR_IF_X] = &&wasm__op_WASM__OP_BR_IF_X,
        [WASM__OP_BR_TABLE] = &&wasm__op_WASM__OP_BR_TABLE,
        [WASM__OP_LOCAL_GET_N] = &&wasm__op_WASM__OP_LOCAL_GET_N,
        [WASM__OP_LOCAL_SET_N] = &&wasm__op_WASM__OP_LOCAL_SET_N,
        [WASM__OP_LOCAL_TEE_N] = &&wasm__op_WASM__OP_LOCAL_TEE_N,
        [WASM__OP_DROP_N] = &&wasm__op_WASM__OP_DROP_N, [WASM__OP_SELECT_N] = &&wasm__op_WASM__OP_SELECT_N,
        [WASM__OP_MEMORY_X] = &&wasm__op_WASM__OP_MEMORY_X, [WASM__OP_TRY] = &&wasm__op_WASM__OP_TRY,
        [WASM__OP_TRY_TABLE] = &&wasm__op_WASM__OP_TRY_TABLE, [WASM__OP_TRY_LEAVE] = &&wasm__op_WASM__OP_TRY_LEAVE,
        [WASM__OP_TRY_END] = &&wasm__op_WASM__OP_TRY_END, [WASM__OP_BR_ON_CAST] = &&wasm__op_WASM__OP_BR_ON_CAST
    };
#define WASM__FUEL4 &&wasm__op_fuel, &&wasm__op_fuel, &&wasm__op_fuel, &&wasm__op_fuel
#define WASM__FUEL16 WASM__FUEL4, WASM__FUEL4, WASM__FUEL4, WASM__FUEL4
    static const void* const wasm__fuel_dispatch[18 * 16] = {
        WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16,
        WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16,
        WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16, WASM__FUEL16
    };
#undef WASM__FUEL16
#undef WASM__FUEL4
    enum { wasm__fuel_dispatch_covers_ops = 1 / (WASM__OP_COUNT <= 18 * 16) };
#else
    int fuel_on;
#endif

    WASM__ENTER_FRAME();

    for (;;) {
#if WASM__THREADED_DISPATCH
        WASM__NEXT();
    wasm__op_fuel:
        if (rt->fuel == 0) {
            pc = op_at;
            op_at = fuel_prev;
            err = WASM_ERR_OUT_OF_FUEL;
            goto suspend;
        }
        rt->fuel--;
        fuel_prev = op_at;
        goto* wasm__dispatch[op];
        do {
#else
        if (fuel_on) {
            if (rt->fuel == 0) {
                err = WASM_ERR_OUT_OF_FUEL;
                goto suspend;
            }
            rt->fuel--;
        }
        op_at = pc;
        op = *pc++;
        switch (op) {
#endif
            WASM__CASE(0x00):
                WASM__TRAP(WASM_ERR_UNREACHABLE);
            WASM__CASE(0x01): /* nop, and the block, loop, and end instructions that need no work */
                WASM__NEXT();
            WASM__CASE(0x04): /* if */
                if (wasm__slot_u32(--sp))
                    pc++;
                else
                    pc = ops + *pc;
                WASM__NEXT();
            WASM__CASE(0x08): { /* throw */
                uint32_t tag_index = *pc++;
                wasm_functype_t* tag_type;
                wasm_value_t payload_buf[8];
                wasm_value_t* payload = payload_buf;

                if (tag_index >= mod->num_tags) WASM__TRAP(WASM_ERR_MALFORMED);
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (tag_type->num_params > 8) {
                    payload = (wasm_value_t*)wasm__malloc_array(&rt->allocator, tag_type->num_params, sizeof(wasm_value_t));
                    if (!payload) WASM__TRAP(WASM_ERR_OOM);
                }
                sp -= wasm__valtypes_slots(tag_type->params, tag_type->num_params);
                wasm__values_from_slots(rt, tag_type->params, tag_type->param_reftypes, tag_type->num_params, sp, payload);
                err = wasm__set_exception_state(&rt->allocator, &rt->pending_exception,
                                                tag_index,
                                                mod->tags[tag_index].identity,
                                                payload,
                                                tag_type->num_params);
                if (payload != payload_buf) wasm__free(&rt->allocator, payload);
                if (err != WASM_OK) goto cleanup;
                goto throw_pending;
            }
            WASM__CASE(0x09): { /* rethrow */
                uint32_t target_index = lsp - 1u - *pc++;

                err = wasm__set_exception_state(&rt->allocator, &rt->pending_exception,
                                                labels[target_index].caught_tag,
                                                labels[target_index].caught_tag_identity,
                                                labels[target_index].caught_values,
                                                labels[target_index].caught_count);
                if (err != WASM_OK) goto cleanup;
                while (lsp > target_index) {
                    wasm__clear_label(&labels[lsp - 1]);
                    sp = stack + labels[lsp - 1].sp_base;
                    lsp--;
                }
                goto throw_pending;
            }
            WASM__CASE(0x0A): { /* throw_ref */
                wasm_value_t exnref_value = wasm__value_from_slot(rt, WASM_TYPE_EXNREF, NULL, --sp);
                wasm__exception_object_t* exn = wasm__exception_object_from_exnref(rt, &exnref_value);

                if (!exn) WASM__TRAP(WASM_ERR_TRAP);
                err = wasm__set_exception_state(&rt->allocator, &rt->pending_exception,
                                                exn->tag_index,
                                                exn->tag_identity,
                                                exn->values,
                                                exn->num_values);
                if (err != WASM_OK) goto cleanup;
                goto throw_pending;
            }
            WASM__CASE(WASM__OP_TRY): {
                wasm__label_t* label = &labels[lsp++];

                label->handler = ops + pc[0];
                label->sp_base = sp_base + pc[1];
                label->caught_count = 0;
                label->caught_values = NULL;
                label->opcode = 0x06;
                label->active_catch = 0;
                pc += 2;
                WASM__NEXT();
            }
            WASM__CASE(WASM__OP_TRY_TABLE): {
                wasm__label_t* label = &labels[lsp++];

                label->handler = pc + 1;
                label->sp_base = sp_base + pc[0];
                label->caught_count = 0;
                label->caught_values = NULL;
                label->opcode = 0x1F;
                label->active_catch = 0;
                pc += 2u + pc[1] * 6u;
                WASM__NEXT();
            }
            WASM__CASE(WASM__OP_TRY_LEAVE):
                wasm__drop_labels(labels, lsp, 1);
                lsp--;
                pc = ops + *pc;
                WASM__NEXT();
            WASM__CASE(WASM__OP_TRY_END):
                wasm__drop_labels(labels, lsp, 1);
                lsp--;
                WASM__NEXT();

            WASM__CASE(WASM__OP_BR):
                pc = ops + *pc;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_LOOP):
                pc = ops + *pc;
                if (wasm__interrupt_pending(rt)) goto interrupted;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_X):
                desc = pc;
                goto do_branch;
            WASM__CASE(WASM__OP_BR_IF):
                if (wasm__slot_u32(--sp))
                    pc = ops + *pc;
                else
                    pc++;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_IF_LOOP):
                if (wasm__slot_u32(--sp)) {
                    pc = ops + *pc;
                    if (wasm__interrupt_pending(rt)) goto interrupted;
                } else {
                    pc++;
                }
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_IF_X):
                if (wasm__slot_u32(--sp)) {
                    desc = pc;
                    goto do_branch;
                }
                pc += 4;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_TABLE): {
                uint32_t selected = wasm__slot_u32(--sp);

                if (selected > pc[0]) selected = pc[0];
                desc = pc + 1u + selected * 4u;
                goto do_branch;
            }
            WASM__CASE(0x0F): /* return */
                goto return_frame;

            WASM__CASE(0x10): /* call */
            WASM__CASE(0x12): /* return_call */
                call_idx = *pc++;
                call_slots = *pc++;
                call_mod = mod;
                call_type = wasm__module_functype(mod, mod->funcs[call_idx].type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (op == 0x12) goto do_tail_call;
                goto do_call;
            WASM__CASE(0x11):   /* call_indirect */
            WASM__CASE(0x13): { /* return_call_indirect */
                uint32_t type_idx = *pc++;
                uint32_t table_idx = *pc++;
                wasm_table_t* table;
                uint64_t elem_index;

                call_slots = *pc++;
                sp--;
                if (type_idx >= mod->num_types || table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                elem_index = wasm__slot_index(sp, table->is_64);
                if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (elem_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                if (wasm__is_null_ref(&table->elems[elem_index])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                if (!wasm__resolve_func_ref(rt, table->elems[elem_index].of.funcref, &call_mod, &call_idx))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!call_mod || call_idx >= call_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                call_type = wasm__module_functype(mod, type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if ((call_mod != mod || call_mod->funcs[call_idx].type_idx != type_idx) &&
                    !wasm__functype_is_subtype_across_modules(call_mod, call_mod->funcs[call_idx].type_idx, mod,
                                                              type_idx))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (op == 0x13) goto do_tail_call;
                goto do_call;
            }
            WASM__CASE(0x14):   /* call_ref */
            WASM__CASE(0x15): { /* return_call_ref */
                uint32_t type_idx = *pc++;

                call_slots = *pc++;
                if (type_idx >= mod->num_types) WASM__TRAP(WASM_ERR_MALFORMED);
                call_type = wasm__module_functype(mod, type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (wasm__slot_u64(--sp) == 0) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null function reference");
                    WASM__TRAP(WASM_ERR_TRAP);
                }
                if (!wasm__resolve_func_ref(rt, wasm__slot_funcref(sp), &call_mod, &call_idx))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!call_mod || call_idx >= call_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                if ((call_mod != mod || call_mod->funcs[call_idx].type_idx != type_idx) &&
                    !wasm__functype_is_subtype_across_modules(call_mod, call_mod->funcs[call_idx].type_idx, mod,
                                                              type_idx))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (op == 0x15) goto do_tail_call;
                goto do_call;
            }
            WASM__CASE(0xFB):
                cf->r.ptr = func->code + *pc++;
                WASM__CALL_OUT(wasm__exec_gc(mod, cf));
                WASM__NEXT();
            WASM__CASE(0xFC):
                cf->r.ptr = func->code + *pc++;
                WASM__CALL_OUT(wasm__exec_bulk(mod, cf));
                WASM__NEXT();
            WASM__CASE(0xFD):
                cf->r.ptr = func->code + *pc++;
                WASM__CALL_OUT(wasm__exec_simd(mod, cf));
                WASM__NEXT();
            WASM__CASE(0xFE):
                cf->r.ptr = func->code + *pc++;
                WASM__CALL_OUT(wasm__exec_atomic(mod, cf));
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_ON_CAST): {
                int taken = 0;

                err = wasm__exec_br_on_cast(mod, pc[0], func->code + pc[1], func->code + func->code_len, sp - 1,
                                            &taken);
                if (err != WASM_OK) goto cleanup;
                if (taken) {
                    desc = pc + 2;
                    goto do_branch;
                }
                pc += 6;
                WASM__NEXT();
            }

            /* Parametric */
            WASM__CASE(0x1A):
                sp--;
                WASM__NEXT();
            WASM__CASE(0x1B):
                sp -= 2;
                if (!wasm__slot_u32(sp + 1)) sp[-1] = sp[0];
                WASM__NEXT();
            WASM__CASE(WASM__OP_DROP_N):
                sp -= *pc++;
                WASM__NEXT();
            WASM__CASE(WASM__OP_SELECT_N): {
                uint32_t n = *pc++;

                sp -= n + 1u;
                if (!wasm__slot_u32(sp + n)) memcpy(sp - n, sp, n * sizeof(wasm__slot_t));
                WASM__NEXT();
            }

            /* Variables */
            WASM__CASE(0x20):
                *sp++ = locals[*pc++];
                WASM__NEXT();
            WASM__CASE(0x21):
                locals[*pc++] = *--sp;
                WASM__NEXT();
            WASM__CASE(0x22):
                locals[*pc++] = sp[-1];
                WASM__NEXT();
            WASM__CASE(WASM__OP_LOCAL_GET_N):
                memcpy(sp, locals + pc[0], pc[1] * sizeof(wasm__slot_t));
                sp += pc[1];
                pc += 2;
                WASM__NEXT();
            WASM__CASE(WASM__OP_LOCAL_SET_N):
            WASM__CASE(WASM__OP_LOCAL_TEE_N):
                memcpy(locals + pc[0], sp - pc[1], pc[1] * sizeof(wasm__slot_t));
                if (op == WASM__OP_LOCAL_SET_N) sp -= pc[1];
                pc += 2;
                WASM__NEXT();
            WASM__CASE(0x23): {
                wasm_value_t value = wasm__global_get_value(&mod->globals[*pc++]);

                sp += wasm__slot_from_value(sp, &value);
                WASM__NEXT();
            }
            WASM__CASE(0x24): {
                uint32_t x = *pc++;
                wasm_value_t value;

                if (!mod->globals[x].is_mutable) {
                    WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "global %u is immutable", (unsigned)x);
                    err = WASM_ERR_TYPE_MISMATCH;
                    goto cleanup;
                }
                sp -= wasm__valtype_slots(mod->globals[x].type);
                value = wasm__value_from_slot(rt, mod->globals[x].type, &mod->globals[x].ref_type, sp);
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      mod->globals[x].type,
                                                      &mod->globals[x].ref_type))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                wasm__global_set_value(&mod->globals[x], value);
                WASM__NEXT();
            }
            WASM__CASE(0x25): {
                uint32_t table_idx = *pc++;
                wasm_table_t* table;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                element_index = wasm__slot_index(sp - 1, table->is_64);
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                wasm__slot_from_value(sp - 1, &table->elems[element_index]);
                WASM__NEXT();
            }
            WASM__CASE(0x26): {
                uint32_t table_idx = *pc++;
                wasm_table_t* table;
                wasm_value_t value;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                value = wasm__value_from_slot(rt, table->reftype, &table->reftype_info, --sp);
                element_index = wasm__slot_index(--sp, table->is_64);
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      table->reftype,
                                                      &table->reftype_info))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                table->elems[element_index] = value;
                WASM__NEXT();
            }

            /* Memory access */
            WASM__CASE(0x28): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 4u);
                wasm__slot_set_u32(sp - 1, wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x29): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 8u);
                wasm__slot_set_u64(sp - 1, wasm__load_le64(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2A): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 4u);
                wasm__slot_set_f32_bits(sp - 1, wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2B): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 8u);
                wasm__slot_set_f64_bits(sp - 1, wasm__load_le64(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2C): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 1u);
                wasm__slot_set_i32(sp - 1, (int8_t)*p);
                WASM__NEXT();
            }
            WASM__CASE(0x2D): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 1u);
                wasm__slot_set_u32(sp - 1, *p);
                WASM__NEXT();
            }
            WASM__CASE(0x2E): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 2u);
                wasm__slot_set_i32(sp - 1, (int16_t)wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2F): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 2u);
                wasm__slot_set_u32(sp - 1, wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x30): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 1u);
                wasm__slot_set_i64(sp - 1, (int8_t)*p);
                WASM__NEXT();
            }
            WASM__CASE(0x31): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 1u);
                wasm__slot_set_u64(sp - 1, *p);
                WASM__NEXT();
            }
            WASM__CASE(0x32): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 2u);
                wasm__slot_set_i64(sp - 1, (int16_t)wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x33): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 2u);
                wasm__slot_set_u64(sp - 1, wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x34): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 4u);
                wasm__slot_set_i64(sp - 1, (int32_t)wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x35): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 1, 4u);
                wasm__slot_set_u64(sp - 1, wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x36): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 4u);
                wasm__store_le32(p, wasm__slot_u32(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x37): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 8u);
                wasm__store_le64(p, wasm__slot_u64(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x38): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 4u);
                wasm__store_le32(p, wasm__slot_f32_bits(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x39): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 8u);
                wasm__store_le64(p, wasm__slot_f64_bits(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3A): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 1u);
                *p = (uint8_t)wasm__slot_u32(sp - 1);
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3B): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 2u);
                wasm__store_le16(p, (uint16_t)wasm__slot_u32(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3C): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 1u);
                *p = (uint8_t)wasm__slot_u64(sp - 1);
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3D): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 2u);
                wasm__store_le16(p, (uint16_t)wasm__slot_u64(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3E): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp - 2, 4u);
                wasm__store_le32(p, (uint32_t)wasm__slot_u64(sp - 1));
                sp -= 2;
                WASM__NEXT();
            }

            WASM__CASE(WASM__OP_MEMORY_X): {
                uint32_t memory_op = pc[0];
                const wasm__slot_t* base = memory_op >= 0x36 ? sp - 2 : sp - 1;
                uint8_t* p = wasm__memory_access_slow(mod, pc[1], base, (uint64_t)pc[2] | ((uint64_t)pc[3] << 32),
                                                      wasm__memory_width(memory_op), &mem0_data, &mem0_size);

                if (!p) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                wasm__memory_transfer(memory_op, p, sp);
                if (memory_op >= 0x36) sp -= 2;
                pc += 4;
                WASM__NEXT();
            }
            WASM__CASE(0x3F): {
                uint32_t mem_idx = *pc++;
                wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);

                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                wasm__slot_set_index(sp++, wasm__memory_pages(memory), memory->is_64);
                WASM__NEXT();
            }
            WASM__CASE(0x40): {
                int64_t res;
                uint32_t mem_idx = *pc++;
                wasm_memory_t* memory;
                uint64_t pages;
                uint64_t units = 0;

                memory = wasm__memory_at(mod, mem_idx);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                pages = wasm__slot_index(sp - 1, memory->is_64);
                if (rt->fuel_enabled && pages <= memory->max_pages - wasm__memory_pages(memory)) {
                    uint64_t page_size = memory->page_size ? memory->page_size : WASM_PAGE_SIZE;

                    units = pages > UINT64_MAX / page_size ? UINT64_MAX : pages * page_size / WASM__FUEL_BYTES_PER_UNIT;
                    if (rt->fuel < units) {
                        rt->fuel = 0;
                        pc = op_at;
                        err = WASM_ERR_OUT_OF_FUEL;
                        goto suspend;
                    }
                }
                res = wasm_memory_grow(mod, mem_idx, pages);
                if (res >= 0) rt->fuel -= units;
                mem0_size = 0;
                wasm__slot_set_index(sp - 1, (uint64_t)res, memory->is_64);
                WASM__NEXT();
            }

            /* Reference instructions */
            WASM__CASE(0xD1):
                wasm__slot_set_i32(sp - 1, wasm__slot_u64(sp - 1) == 0);
                WASM__NEXT();
            WASM__CASE(0xD2): {
                uint32_t func_index = *pc++;
                wasm_value_t funcref;
                if (func_index >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__CALL_OUT(wasm__make_funcref(mod, func_index, &funcref));
                sp += wasm__slot_from_value(sp, &funcref);
                WASM__NEXT();
            }
            WASM__CASE(0xD3):
                WASM__BINARY(uint64_t, u64, i32, a == b);
                WASM__NEXT();
            WASM__CASE(0xD4):
                if (wasm__slot_u64(sp - 1) == 0) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null reference");
                    err = WASM_ERR_TRAP;
                    goto cleanup;
                }
                WASM__NEXT();
            WASM__CASE(0xD5):
            WASM__CASE(0xD6):
                desc = pc;
                pc += 4;
                if (wasm__slot_u64(sp - 1) == 0) {
                    sp--;
                    if (op == 0xD5) goto do_branch;
                    WASM__NEXT();
                }
                if (op == 0xD6) goto do_branch;
                WASM__NEXT();

            /* Constants */
            WASM__CASE(0x41):
                wasm__slot_set_u32(sp++, *pc++);
                WASM__NEXT();
            WASM__CASE(0x43):
                wasm__slot_set_f32_bits(sp++, *pc++);
                WASM__NEXT();
            WASM__CASE(0x42):
                wasm__slot_set_u64(sp++, (uint64_t)pc[0] | ((uint64_t)pc[1] << 32));
                pc += 2;
                WASM__NEXT();
            WASM__CASE(0x44):
                wasm__slot_set_f64_bits(sp++, (uint64_t)pc[0] | ((uint64_t)pc[1] << 32));
                pc += 2;
                WASM__NEXT();

            WASM__CASE(0x45):
                WASM__UNARY(uint32_t, u32, i32, a == 0);
                WASM__NEXT();
            WASM__CASE(0x46):
                WASM__BINARY(uint32_t, u32, i32, a == b);
                WASM__NEXT();
            WASM__CASE(0x47):
                WASM__BINARY(uint32_t, u32, i32, a != b);
                WASM__NEXT();
            WASM__CASE(0x48):
                WASM__BINARY(int32_t, i32, i32, a < b);
                WASM__NEXT();
            WASM__CASE(0x49):
                WASM__BINARY(uint32_t, u32, i32, a < b);
                WASM__NEXT();
            WASM__CASE(0x4A):
                WASM__BINARY(int32_t, i32, i32, a > b);
                WASM__NEXT();
            WASM__CASE(0x4B):
                WASM__BINARY(uint32_t, u32, i32, a > b);
                WASM__NEXT();
            WASM__CASE(0x4C):
                WASM__BINARY(int32_t, i32, i32, a <= b);
                WASM__NEXT();
            WASM__CASE(0x4D):
                WASM__BINARY(uint32_t, u32, i32, a <= b);
                WASM__NEXT();
            WASM__CASE(0x4E):
                WASM__BINARY(int32_t, i32, i32, a >= b);
                WASM__NEXT();
            WASM__CASE(0x4F):
                WASM__BINARY(uint32_t, u32, i32, a >= b);
                WASM__NEXT();

            /* i64 comparison */
            WASM__CASE(0x50):
                WASM__UNARY(uint64_t, u64, i32, a == 0);
                WASM__NEXT();
            WASM__CASE(0x51):
                WASM__BINARY(uint64_t, u64, i32, a == b);
                WASM__NEXT();
            WASM__CASE(0x52):
                WASM__BINARY(uint64_t, u64, i32, a != b);
                WASM__NEXT();
            WASM__CASE(0x53):
                WASM__BINARY(int64_t, i64, i32, a < b);
                WASM__NEXT();
            WASM__CASE(0x54):
                WASM__BINARY(uint64_t, u64, i32, a < b);
                WASM__NEXT();
            WASM__CASE(0x55):
                WASM__BINARY(int64_t, i64, i32, a > b);
                WASM__NEXT();
            WASM__CASE(0x56):
                WASM__BINARY(uint64_t, u64, i32, a > b);
                WASM__NEXT();
            WASM__CASE(0x57):
                WASM__BINARY(int64_t, i64, i32, a <= b);
                WASM__NEXT();
            WASM__CASE(0x58):
                WASM__BINARY(uint64_t, u64, i32, a <= b);
                WASM__NEXT();
            WASM__CASE(0x59):
                WASM__BINARY(int64_t, i64, i32, a >= b);
                WASM__NEXT();
            WASM__CASE(0x5A):
                WASM__BINARY(uint64_t, u64, i32, a >= b);
                WASM__NEXT();

            /* f32/f64 comparison */
            WASM__CASE(0x5B):
                WASM__BINARY(float, f32, i32, a == b);
                WASM__NEXT();
            WASM__CASE(0x5C):
                WASM__BINARY(float, f32, i32, a != b);
                WASM__NEXT();
            WASM__CASE(0x5D):
                WASM__BINARY(float, f32, i32, a < b);
                WASM__NEXT();
            WASM__CASE(0x5E):
                WASM__BINARY(float, f32, i32, a > b);
                WASM__NEXT();
            WASM__CASE(0x5F):
                WASM__BINARY(float, f32, i32, a <= b);
                WASM__NEXT();
            WASM__CASE(0x60):
                WASM__BINARY(float, f32, i32, a >= b);
                WASM__NEXT();
            WASM__CASE(0x61):
                WASM__BINARY(double, f64, i32, a == b);
                WASM__NEXT();
            WASM__CASE(0x62):
                WASM__BINARY(double, f64, i32, a != b);
                WASM__NEXT();
            WASM__CASE(0x63):
                WASM__BINARY(double, f64, i32, a < b);
                WASM__NEXT();
            WASM__CASE(0x64):
                WASM__BINARY(double, f64, i32, a > b);
                WASM__NEXT();
            WASM__CASE(0x65):
                WASM__BINARY(double, f64, i32, a <= b);
                WASM__NEXT();
            WASM__CASE(0x66):
                WASM__BINARY(double, f64, i32, a >= b);
                WASM__NEXT();

            /* i32 arithmetic */
            WASM__CASE(0x67):
                WASM__UNARY(uint32_t, u32, u32, wasm__clz32(a));
                WASM__NEXT();
            WASM__CASE(0x68):
                WASM__UNARY(uint32_t, u32, u32, wasm__ctz32(a));
                WASM__NEXT();
            WASM__CASE(0x69):
                WASM__UNARY(uint32_t, u32, u32, wasm__popcnt32(a));
                WASM__NEXT();
            WASM__CASE(0x6A):
                WASM__BINARY(uint32_t, u32, u32, a + b);
                WASM__NEXT();
            WASM__CASE(0x6B):
                WASM__BINARY(uint32_t, u32, u32, a - b);
                WASM__NEXT();
            WASM__CASE(0x6C):
                WASM__BINARY(uint32_t, u32, u32, a * b);
                WASM__NEXT();
            WASM__CASE(0x6D): {
                int32_t b = wasm__slot_i32(sp - 1), a = wasm__slot_i32(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                if (a == INT32_MIN && b == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                wasm__slot_set_i32(--sp - 1, a / b);
                WASM__NEXT();
            }
            WASM__CASE(0x6E): {
                uint32_t b = wasm__slot_u32(sp - 1), a = wasm__slot_u32(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                wasm__slot_set_u32(--sp - 1, a / b);
                WASM__NEXT();
            }
            WASM__CASE(0x6F): {
                int32_t b = wasm__slot_i32(sp - 1), a = wasm__slot_i32(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                wasm__slot_set_i32(--sp - 1, (a == INT32_MIN && b == -1) ? 0 : a % b);
                WASM__NEXT();
            }
            WASM__CASE(0x70): {
                uint32_t b = wasm__slot_u32(sp - 1), a = wasm__slot_u32(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                wasm__slot_set_u32(--sp - 1, a % b);
                WASM__NEXT();
            }
            WASM__CASE(0x71):
                WASM__BINARY(uint32_t, u32, u32, a & b);
                WASM__NEXT();
            WASM__CASE(0x72):
                WASM__BINARY(uint32_t, u32, u32, a | b);
                WASM__NEXT();
            WASM__CASE(0x73):
                WASM__BINARY(uint32_t, u32, u32, a ^ b);
                WASM__NEXT();
            WASM__CASE(0x74):
                WASM__BINARY(uint32_t, u32, u32, a << (b & 31u));
                WASM__NEXT();
            WASM__CASE(0x75):
                WASM__BINARY(uint32_t, u32, u32, wasm__shr_s32_bits(a, b));
                WASM__NEXT();
            WASM__CASE(0x76):
                WASM__BINARY(uint32_t, u32, u32, a >> (b & 31u));
                WASM__NEXT();
            WASM__CASE(0x77):
                WASM__BINARY(uint32_t, u32, u32, (a << (b & 31u)) | (a >> ((32u - (b & 31u)) & 31u)));
                WASM__NEXT();
            WASM__CASE(0x78):
                WASM__BINARY(uint32_t, u32, u32, (a >> (b & 31u)) | (a << ((32u - (b & 31u)) & 31u)));
                WASM__NEXT();

            /* i64 arithmetic */
            WASM__CASE(0x79):
                WASM__UNARY(uint64_t, u64, u64, wasm__clz64(a));
                WASM__NEXT();
            WASM__CASE(0x7A):
                WASM__UNARY(uint64_t, u64, u64, wasm__ctz64(a));
                WASM__NEXT();
            WASM__CASE(0x7B):
                WASM__UNARY(uint64_t, u64, u64, wasm__popcnt64(a));
                WASM__NEXT();
            WASM__CASE(0x7C):
                WASM__BINARY(uint64_t, u64, u64, a + b);
                WASM__NEXT();
            WASM__CASE(0x7D):
                WASM__BINARY(uint64_t, u64, u64, a - b);
                WASM__NEXT();
            WASM__CASE(0x7E):
                WASM__BINARY(uint64_t, u64, u64, a * b);
                WASM__NEXT();
            WASM__CASE(0x7F): {
                int64_t b = wasm__slot_i64(sp - 1), a = wasm__slot_i64(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                if (a == INT64_MIN && b == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                wasm__slot_set_i64(--sp - 1, a / b);
                WASM__NEXT();
            }
            WASM__CASE(0x80): {
                uint64_t b = wasm__slot_u64(sp - 1), a = wasm__slot_u64(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                wasm__slot_set_u64(--sp - 1, a / b);
                WASM__NEXT();
            }
            WASM__CASE(0x81): {
                int64_t b = wasm__slot_i64(sp - 1), a = wasm__slot_i64(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                wasm__slot_set_i64(--sp - 1, (a == INT64_MIN && b == -1) ? 0 : a % b);
                WASM__NEXT();
            }
            WASM__CASE(0x82): {
                uint64_t b = wasm__slot_u64(sp - 1), a = wasm__slot_u64(sp - 2);

                if (b == 0) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                wasm__slot_set_u64(--sp - 1, a % b);
                WASM__NEXT();
            }
            WASM__CASE(0x83):
                WASM__BINARY(uint64_t, u64, u64, a & b);
                WASM__NEXT();
            WASM__CASE(0x84):
                WASM__BINARY(uint64_t, u64, u64, a | b);
                WASM__NEXT();
            WASM__CASE(0x85):
                WASM__BINARY(uint64_t, u64, u64, a ^ b);
                WASM__NEXT();
            WASM__CASE(0x86):
                WASM__BINARY(uint64_t, u64, u64, a << (b & 63u));
                WASM__NEXT();
            WASM__CASE(0x87):
                WASM__BINARY(uint64_t, u64, u64, wasm__shr_s64_bits(a, (uint32_t)b));
                WASM__NEXT();
            WASM__CASE(0x88):
                WASM__BINARY(uint64_t, u64, u64, a >> (b & 63u));
                WASM__NEXT();
            WASM__CASE(0x89):
                WASM__BINARY(uint64_t, u64, u64, (a << (b & 63u)) | (a >> ((64u - (b & 63u)) & 63u)));
                WASM__NEXT();
            WASM__CASE(0x8A):
                WASM__BINARY(uint64_t, u64, u64, (a >> (b & 63u)) | (a << ((64u - (b & 63u)) & 63u)));
                WASM__NEXT();

            /* f32 arithmetic */
            WASM__CASE(0x8B):
                WASM__UNARY(uint32_t, f32_bits, f32_bits, a & 0x7FFFFFFFu);
                WASM__NEXT();
            WASM__CASE(0x8C):
                WASM__UNARY(uint32_t, f32_bits, f32_bits, a ^ 0x80000000u);
                WASM__NEXT();
            WASM__CASE(0x8D):
                WASM__UNARY(float, f32, f32, wasm__ceil_f32(a));
                WASM__NEXT();
            WASM__CASE(0x8E):
                WASM__UNARY(float, f32, f32, wasm__floor_f32(a));
                WASM__NEXT();
            WASM__CASE(0x8F):
                WASM__UNARY(float, f32, f32, wasm__trunc_f32(a));
                WASM__NEXT();
            WASM__CASE(0x90):
                WASM__UNARY(float, f32, f32, wasm__nearest_f32(a));
                WASM__NEXT();
            WASM__CASE(0x91):
                WASM__UNARY(float, f32, f32, wasm__sqrt_f32(a));
                WASM__NEXT();
            WASM__CASE(0x92):
                WASM__BINARY(float, f32, f32, a + b);
                WASM__NEXT();
            WASM__CASE(0x93):
                WASM__BINARY(float, f32, f32, a - b);
                WASM__NEXT();
            WASM__CASE(0x94):
                WASM__BINARY(float, f32, f32, a * b);
                WASM__NEXT();
            WASM__CASE(0x95):
                WASM__BINARY(float, f32, f32, a / b);
                WASM__NEXT();
            WASM__CASE(0x96):
                WASM__BINARY(float, f32, f32, wasm__f32_min(a, b));
                WASM__NEXT();
            WASM__CASE(0x97):
                WASM__BINARY(float, f32, f32, wasm__f32_max(a, b));
                WASM__NEXT();
            WASM__CASE(0x98):
                WASM__BINARY(uint32_t, f32_bits, f32_bits, (a & 0x7FFFFFFFu) | (b & 0x80000000u));
                WASM__NEXT();

            /* f64 arithmetic */
            WASM__CASE(0x99):
                WASM__UNARY(uint64_t, f64_bits, f64_bits, a & UINT64_C(0x7FFFFFFFFFFFFFFF));
                WASM__NEXT();
            WASM__CASE(0x9A):
                WASM__UNARY(uint64_t, f64_bits, f64_bits, a ^ UINT64_C(0x8000000000000000));
                WASM__NEXT();
            WASM__CASE(0x9B):
                WASM__UNARY(double, f64, f64, wasm__ceil_f64(a));
                WASM__NEXT();
            WASM__CASE(0x9C):
                WASM__UNARY(double, f64, f64, wasm__floor_f64(a));
                WASM__NEXT();
            WASM__CASE(0x9D):
                WASM__UNARY(double, f64, f64, wasm__trunc_f64(a));
                WASM__NEXT();
            WASM__CASE(0x9E):
                WASM__UNARY(double, f64, f64, wasm__nearest_f64(a));
                WASM__NEXT();
            WASM__CASE(0x9F):
                WASM__UNARY(double, f64, f64, wasm__sqrt_f64(a));
                WASM__NEXT();
            WASM__CASE(0xA0):
                WASM__F64_BINARY(a + b);
                WASM__NEXT();
            WASM__CASE(0xA1):
                WASM__F64_BINARY(a - b);
                WASM__NEXT();
            WASM__CASE(0xA2):
                WASM__F64_BINARY(a * b);
                WASM__NEXT();
            WASM__CASE(0xA3):
                WASM__F64_BINARY(a / b);
                WASM__NEXT();
            WASM__CASE(0xA4):
                WASM__BINARY(double, f64, f64, wasm__f64_min(a, b));
                WASM__NEXT();
            WASM__CASE(0xA5):
                WASM__BINARY(double, f64, f64, wasm__f64_max(a, b));
                WASM__NEXT();
            WASM__CASE(0xA6):
                WASM__BINARY(uint64_t, f64_bits, f64_bits, (a & UINT64_C(0x7FFFFFFFFFFFFFFF)) | (b & UINT64_C(0x8000000000000000)));
                WASM__NEXT();

            /* Conversions */
            WASM__CASE(0xA7):
                WASM__UNARY(uint64_t, u64, u32, (uint32_t)a);
                WASM__NEXT();
            WASM__CASE(0xA8): {
                int32_t value;

                err = wasm__trunc_i32_s((double)wasm__slot_f32(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_i32(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xA9): {
                uint32_t value;

                err = wasm__trunc_i32_u((double)wasm__slot_f32(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_u32(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xAA): {
                int32_t value;

                err = wasm__trunc_i32_s(wasm__slot_f64(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_i32(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xAB): {
                uint32_t value;

                err = wasm__trunc_i32_u(wasm__slot_f64(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_u32(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xAC):
                WASM__UNARY(int32_t, i32, i64, a);
                WASM__NEXT();
            WASM__CASE(0xAD):
                WASM__UNARY(uint32_t, u32, u64, a);
                WASM__NEXT();
            WASM__CASE(0xAE): {
                int64_t value;

                err = wasm__trunc_i64_s((double)wasm__slot_f32(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_i64(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xAF): {
                uint64_t value;

                err = wasm__trunc_i64_u((double)wasm__slot_f32(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_u64(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xB0): {
                int64_t value;

                err = wasm__trunc_i64_s(wasm__slot_f64(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_i64(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xB1): {
                uint64_t value;

                err = wasm__trunc_i64_u(wasm__slot_f64(sp - 1), &value);
                if (err != WASM_OK) goto cleanup;
                wasm__slot_set_u64(sp - 1, value);
                WASM__NEXT();
            }
            WASM__CASE(0xB2):
                WASM__UNARY(int32_t, i32, f32, (float)a);
                WASM__NEXT();
            WASM__CASE(0xB3):
                WASM__UNARY(uint32_t, u32, f32, (float)a);
                WASM__NEXT();
            WASM__CASE(0xB4):
                WASM__UNARY(int64_t, i64, f32, (float)a);
                WASM__NEXT();
            WASM__CASE(0xB5):
                WASM__UNARY(uint64_t, u64, f32, (float)a);
                WASM__NEXT();
            WASM__CASE(0xB6):
                WASM__UNARY(double, f64, f32, (float)a);
                WASM__NEXT();
            WASM__CASE(0xB7):
                WASM__UNARY(int32_t, i32, f64, (double)a);
                WASM__NEXT();
            WASM__CASE(0xB8):
                WASM__UNARY(uint32_t, u32, f64, (double)a);
                WASM__NEXT();
            WASM__CASE(0xB9):
                WASM__UNARY(int64_t, i64, f64, (double)a);
                WASM__NEXT();
            WASM__CASE(0xBA):
                WASM__UNARY(uint64_t, u64, f64, (double)a);
                WASM__NEXT();
            WASM__CASE(0xBB):
                WASM__UNARY(float, f32, f64, (double)a);
                WASM__NEXT();

            /* Sign extension */
            WASM__CASE(0xC0):
                WASM__UNARY(uint32_t, u32, u32, (a & 0x80u) ? (a | 0xFFFFFF00u) : (a & 0xFFu));
                WASM__NEXT();
            WASM__CASE(0xC1):
                WASM__UNARY(uint32_t, u32, u32, (a & 0x8000u) ? (a | 0xFFFF0000u) : (a & 0xFFFFu));
                WASM__NEXT();
            WASM__CASE(0xC2):
                WASM__UNARY(uint64_t, u64, u64, (a & 0x80u) ? (a | UINT64_C(0xFFFFFFFFFFFFFF00)) : (a & 0xFFu));
                WASM__NEXT();
            WASM__CASE(0xC3):
                WASM__UNARY(uint64_t, u64, u64, (a & 0x8000u) ? (a | UINT64_C(0xFFFFFFFFFFFF0000)) : (a & 0xFFFFu));
                WASM__NEXT();
            WASM__CASE(0xC4):
                WASM__UNARY(uint64_t, u64, u64, (a & 0x80000000u) ? (a | UINT64_C(0xFFFFFFFF00000000)) : (a & 0xFFFFFFFFu));
                WASM__NEXT();

            WASM__DEFAULT:
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown opcode 0x%02X", op);
                WASM__TRAP(WASM_ERR_MALFORMED);
#if WASM__THREADED_DISPATCH
        } while (0);
#else
        }
#endif
        continue;

    do_branch: {
        uint32_t arity = desc[1];
        uint32_t pops = desc[3] >> WASM__BRANCH_POPS_SHIFT;
        wasm__slot_t* dst = stack + sp_base + desc[2];

        if (desc[3] & WASM__BRANCH_RETURN) goto return_frame;
        if (arity > 0 && sp - arity != dst) memmove(dst, sp - arity, arity * sizeof(wasm__slot_t));
        sp = dst + arity;
        if (pops > 0) {
            wasm__drop_labels(labels, lsp, pops);
            lsp -= pops;
        }
        pc = ops + desc[0];
        if ((desc[3] & WASM__BRANCH_LOOP) && wasm__interrupt_pending(rt)) goto interrupted;
        WASM__NEXT();
    }

    do_call:
        sp -= call_slots;
        if (call_mod->funcs[call_idx].is_import) {
            WASM__SAVE_STATE();
            err = wasm__call_host_from_stack(call_mod, call_type, &call_mod->funcs[call_idx], sp);
            sp = stack + rt->sp;
            mem0_size = 0;
            WASM__LOAD_FUEL();
            if (err == WASM_OK) WASM__NEXT();
            if (err == WASM_YIELD) goto yielded;
            if (err != WASM__ERR_EXCEPTION) goto cleanup;
            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 0);
            if (err == WASM_ERR_INTERRUPTED) goto interrupted_in_handler;
            if (err != WASM__ERR_RESUME_FRAME) return err;
            goto resume_frame;
        }
        WASM__SAVE_STATE();
        err = wasm__push_call_frame(call_mod, call_idx, sp, NULL, 0);
        if (err != WASM_OK) goto cleanup;
        WASM__ENTER_FRAME();
        if (wasm__interrupt_pending(rt)) goto interrupted;
        WASM__NEXT();

    do_tail_call: {
        wasm_value_t* results_dst = cf->results_dst;
        uint32_t num_results = cf->num_results;
        if (call_mod->funcs[call_idx].is_import) {
            sp -= call_slots;
            WASM__SAVE_STATE();
            err = wasm__call_host_from_stack(call_mod, call_type, &call_mod->funcs[call_idx], sp);
            sp = stack + rt->sp;
            mem0_size = 0;
            WASM__LOAD_FUEL();
            if (err == WASM_OK) goto return_frame;
            if (err == WASM_YIELD) {
                pc = NULL;
                goto yielded;
            }
            if (err != WASM__ERR_EXCEPTION) goto cleanup;
            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 1);
            if (err == WASM_ERR_INTERRUPTED) goto interrupted_in_handler;
            if (err != WASM__ERR_RESUME_FRAME) return err;
            goto resume_frame;
        }
        if (call_slots > 0 && sp - call_slots != stack + sp_base)
            memmove(stack + sp_base, sp - call_slots, call_slots * sizeof(wasm__slot_t));
        sp = stack + sp_base;
        wasm__drop_labels(labels, lsp, lsp);
        lsp = 0;
        WASM__SAVE_STATE();
        wasm__arena_reset(rt, cf->arena_saved);
        rt->call_frame_sp--;
        err = wasm__push_call_frame(call_mod, call_idx, sp, results_dst, num_results);
        if (err != WASM_OK) return err;
        WASM__ENTER_FRAME();
        if (wasm__interrupt_pending(rt)) goto interrupted;
        WASM__NEXT();
    }

    throw_pending:
        WASM__SAVE_STATE();
        err = wasm__handle_exception_frames(mod, base_frame_sp);
        if (err == WASM_ERR_INTERRUPTED) goto interrupted_in_handler;
        if (err != WASM_OK) return err;
    resume_frame:
        WASM__ENTER_FRAME();
        if (!pc) goto return_frame;
        WASM__NEXT();

    return_frame:
        if (rt->call_frame_sp == base_frame_sp + 1u) {
            err = WASM__ERR_RETURN_FRAME;
            goto cleanup;
        }
        {
            wasm__slot_t* dst = stack + sp_base;
            wasm__slot_t* src = sp - func->result_slots;

            for (i = 0; i < func->result_slots; i++) dst[i] = src[i];
            sp = dst + func->result_slots;
            wasm__drop_labels(labels, lsp, lsp);
            cf->lsp = 0;
            wasm__arena_reset(rt, cf->arena_saved);
            rt->call_frame_sp--;
            rt->sp = (uint32_t)(sp - stack);
            WASM__ENTER_FRAME();
            WASM__NEXT();
        }
    }

    interrupted_in_handler:
        WASM__ENTER_FRAME();
    interrupted:
        err = WASM_ERR_INTERRUPTED;
        goto suspend;

    yielded:
        if (!resumable) {
            WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "host function yielded outside a resumable call");
            err = WASM_ERR_TRAP;
            goto cleanup;
        }
        rt->yield_module = call_mod;
        rt->yield_type = call_type;
        WASM__SET_ERR(rt, WASM_YIELD, "%s", "host function yielded");

    suspend:
        if (!resumable) goto cleanup;
        WASM__SAVE_STATE();
        rt->suspended = err;
        return err;

cleanup:
    WASM__SAVE_STATE();
    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }

    return err;
}

#undef WASM__BINARY
#undef WASM__F64_BINARY
#undef WASM__UNARY
#undef WASM__CALL_OUT
#undef WASM__MEMORY_PTR
#undef WASM__ENTER_FRAME
#undef WASM__DEFAULT
#undef WASM__CASE
#undef WASM__NEXT
#undef WASM__LOAD_FUEL
#if WASM__THREADED_DISPATCH
#pragma GCC diagnostic pop
#endif
#undef WASM__THREADED_DISPATCH
#undef WASM__LOAD_STATE
#undef WASM__SAVE_STATE

static void wasm__drop_call_frames(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];

        wasm__clear_call_frame(cf);
        wasm__arena_reset(rt, cf->arena_saved);
        rt->sp = cf->sp_base;
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }
}

static wasm_error_t wasm__interp_run(wasm_runtime_t* rt, uint32_t base_frame_sp, int resumable) {
    wasm_error_t err = wasm__interp_loop(rt, base_frame_sp, resumable);

    if (rt->suspended != WASM_OK) return err;
    if (err == WASM__ERR_RETURN_FRAME) err = wasm__finish_call_frame(rt, base_frame_sp);
    if (err != WASM_OK && rt->call_frame_sp > base_frame_sp)
        wasm__capture_backtrace(rt, base_frame_sp);
    wasm__drop_call_frames(rt, base_frame_sp);
    return err;
}

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    uint32_t base_frame_sp = rt->call_frame_sp;
    wasm_error_t err;

    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import) {
        err = wasm__invoke_host_func(mod, wasm__module_functype(mod, func->type_idx), func->host_func,
                                     args, results, func->host_userdata);
        if (err == WASM_YIELD) {
            WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "host function yielded outside a resumable call");
            err = WASM_ERR_TRAP;
        }
        return err;
    }

    if (wasm__runtime_take_interrupt(rt)) return WASM_ERR_INTERRUPTED;
    err = wasm__push_root_call_frame(mod, func_idx, args, num_args, results, num_results);
    if (err != WASM_OK) return err;
    return wasm__interp_run(rt, base_frame_sp, base_frame_sp == 0 && rt->nonresumable_depth == 0);
}

WASM_PRIVATE wasm_error_t wasm__interp_resume(wasm_runtime_t* rt,
                                        const wasm_value_t* host_results, uint32_t num_host_results,
                                        wasm_value_t* results, uint32_t num_results) {
    const wasm_functype_t* root_type;
    uint32_t expected = 0;
    uint32_t slots;
    uint32_t i;

    if (rt->suspended == WASM_OK || rt->call_frame_sp == 0) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "no call is suspended");
        return WASM_ERR_MALFORMED;
    }
    root_type = rt->call_frames[0].ft;
    if (num_results < root_type->num_results || (root_type->num_results > 0 && !results)) {
        WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "resume expects %u results, got %u",
                      (unsigned)root_type->num_results, (unsigned)num_results);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (rt->suspended == WASM_YIELD) expected = rt->yield_type->num_results;
    if (num_host_results != expected || (expected > 0 && !host_results)) {
        WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "resume expects %u host results, got %u",
                      (unsigned)expected, (unsigned)num_host_results);
        return WASM_ERR_TYPE_MISMATCH;
    }
    for (i = 0; i < expected; i++) {
        const wasm_functype_t* ft = rt->yield_type;

        if (!wasm__runtime_value_matches_type(rt->yield_module, &host_results[i], ft->results[i],
                                              ft->result_reftypes ? &ft->result_reftypes[i] : NULL)) {
            WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH,
                          "host function result %u does not match its declared type", (unsigned)i);
            return WASM_ERR_TYPE_MISMATCH;
        }
    }
    if (expected > 0 && wasm__valtypes_slots(rt->yield_type->results, expected) > rt->max_stack - rt->sp) {
        wasm__interp_cancel(rt);
        WASM__SET_ERR(rt, WASM_ERR_STACK_OVERFLOW, "%s", "no stack space for the host function's results");
        return WASM_ERR_STACK_OVERFLOW;
    }
    for (i = 0, slots = 0; i < expected; i++) {
        wasm_value_t value = host_results[i];
        wasm_error_t err = wasm__anyref_from_host(rt, &value);

        if (err != WASM_OK) return err;
        slots += wasm__slot_from_value(&rt->stack[rt->sp + slots], &value);
    }
    rt->sp += slots;
    rt->call_frames[0].results_dst = results;
    rt->suspended = WASM_OK;
    rt->yield_module = NULL;
    rt->yield_type = NULL;
    wasm__atomic_store_size(&rt->interrupt_requested, 0u);
    if (!rt->call_frames[rt->call_frame_sp - 1u].pc) {
        wasm_error_t err;

        wasm__clear_call_frame(&rt->call_frames[rt->call_frame_sp - 1u]);
        err = wasm__finish_call_frame(rt, 0);
        if (err != WASM_OK) {
            wasm__drop_call_frames(rt, 0);
            return err;
        }
        if (rt->call_frame_sp == 0) return WASM_OK;
    }
    return wasm__interp_run(rt, 0, 1);
}

WASM_PRIVATE void wasm__interp_cancel(wasm_runtime_t* rt) {
    if (!rt || rt->suspended == WASM_OK) return;
    wasm__drop_call_frames(rt, 0);
    rt->suspended = WASM_OK;
    rt->yield_module = NULL;
    rt->yield_type = NULL;
}

WASM_PRIVATE void wasm__interp_cancel_using(wasm_runtime_t* rt, const wasm_module_t* mod) {
    uint32_t i;

    if (!rt || rt->suspended == WASM_OK) return;
    if (rt->yield_module == mod) {
        wasm__interp_cancel(rt);
        return;
    }
    for (i = 0; i < rt->call_frame_sp; i++) {
        if (rt->call_frames[i].module == mod) {
            wasm__interp_cancel(rt);
            return;
        }
    }
}
