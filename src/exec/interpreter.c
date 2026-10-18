#include "interpreter.h"
#include <stdio.h>
#include "bulk.h"
#include "calls.h"
#include "translate.h"
#include "exceptions.h"
#include "frames.h"
#include "gc.h"
#include "simd.h"
#include "atomic.h"
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
    err = host_func(mod, args, ft->num_params, results, ft->num_results, userdata);
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
                                         const wasm_value_t* base,
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
    if (!wasm__memory_effective_addr(memory, base, offset, &addr)) return NULL;
    if (!wasm__memory_access_in_bounds(addr, width, wasm__memory_byte_size(memory))) return NULL;
    return memory->data + addr;
}

static uint32_t wasm__memory_width(uint32_t op) {
    static const uint8_t widths[] = { 4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4, 4, 8, 4, 8, 1, 2, 1, 2, 4 };

    return widths[op - 0x28];
}

static uint32_t wasm__f32_to_bits(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t wasm__f64_to_bits(double value) {
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* op is a load or store opcode (0x28 to 0x3E) and p the address it accesses. */
static void wasm__memory_transfer(uint32_t op, uint8_t* p, wasm_value_t* sp) {
    switch (op) {
        case 0x28:
            sp[-1] = wasm_i32((int32_t)wasm__load_le32(p));
            break;
        case 0x29:
            sp[-1] = wasm_i64((int64_t)wasm__load_le64(p));
            break;
        case 0x2A:
            sp[-1] = wasm__f32_bits(wasm__load_le32(p));
            break;
        case 0x2B:
            sp[-1] = wasm__f64_bits(wasm__load_le64(p));
            break;
        case 0x2C:
            sp[-1] = wasm_i32((int32_t)(int8_t)*p);
            break;
        case 0x2D:
            sp[-1] = wasm_i32((int32_t)*p);
            break;
        case 0x2E:
            sp[-1] = wasm_i32((int32_t)(int16_t)wasm__load_le16(p));
            break;
        case 0x2F:
            sp[-1] = wasm_i32((int32_t)wasm__load_le16(p));
            break;
        case 0x30:
            sp[-1] = wasm_i64((int64_t)(int8_t)*p);
            break;
        case 0x31:
            sp[-1] = wasm_i64((int64_t)*p);
            break;
        case 0x32:
            sp[-1] = wasm_i64((int64_t)(int16_t)wasm__load_le16(p));
            break;
        case 0x33:
            sp[-1] = wasm_i64((int64_t)wasm__load_le16(p));
            break;
        case 0x34:
            sp[-1] = wasm_i64((int64_t)(int32_t)wasm__load_le32(p));
            break;
        case 0x35:
            sp[-1] = wasm_i64((int64_t)wasm__load_le32(p));
            break;
        case 0x36:
            wasm__store_le32(p, (uint32_t)sp[-1].of.i32);
            break;
        case 0x37:
            wasm__store_le64(p, (uint64_t)sp[-1].of.i64);
            break;
        case 0x38:
            wasm__store_le32(p, wasm__f32_to_bits(sp[-1].of.f32));
            break;
        case 0x39:
            wasm__store_le64(p, wasm__f64_to_bits(sp[-1].of.f64));
            break;
        case 0x3A:
            *p = (uint8_t)sp[-1].of.i32;
            break;
        case 0x3B:
            wasm__store_le16(p, (uint16_t)sp[-1].of.i32);
            break;
        case 0x3C:
            *p = (uint8_t)sp[-1].of.i64;
            break;
        case 0x3D:
            wasm__store_le16(p, (uint16_t)sp[-1].of.i64);
            break;
        case 0x3E:
            wasm__store_le32(p, (uint32_t)sp[-1].of.i64);
            break;
        default:
            break;
    }
}

/* args holds ft->num_params values just above rt->sp. */
static wasm_error_t wasm__call_host_from_stack(wasm_module_t* mod,
                                               const wasm_functype_t* ft,
                                               const wasm_func_t* func,
                                               const wasm_value_t* args) {
    wasm_runtime_t* rt = mod->rt;
    wasm_value_t args_buf[8];
    wasm_value_t results_buf[8];
    wasm_value_t* call_args = args_buf;
    wasm_value_t* call_results = results_buf;
    wasm_error_t err = WASM_ERR_OOM;

    if (ft->num_params > 8) call_args = (wasm_value_t*)wasm__malloc_array(&mod->rt->allocator, ft->num_params, sizeof(wasm_value_t));
    if (ft->num_results > 8) call_results = (wasm_value_t*)wasm__malloc_array(&mod->rt->allocator, ft->num_results, sizeof(wasm_value_t));
    if (call_args && call_results) {
        if (ft->num_params > 0) memcpy(call_args, args, ft->num_params * sizeof(wasm_value_t));
        err = wasm__invoke_host_func(mod, ft, func->host_func, call_args, call_results, func->host_userdata);
        if (err == WASM_OK && ft->num_results > rt->max_stack - rt->sp) err = WASM_ERR_STACK_OVERFLOW;
        if (err == WASM_OK && ft->num_results > 0) {
            memcpy(&rt->stack[rt->sp], call_results, ft->num_results * sizeof(wasm_value_t));
            rt->sp += ft->num_results;
        }
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
/* Prefixed instructions must set cf->r to their immediates first. */
#define WASM__CALL_OUT(expr)                      \
    do {                                          \
        WASM__SAVE_STATE();                       \
        err = (expr);                             \
        WASM__LOAD_STATE();                       \
        mem0_size = 0;                            \
        if (err != WASM_OK) goto cleanup;         \
    } while (0)
/* Only for 32-bit memory 0 with an offset below 2^32; a zero mem0_size forces a refresh. */
#define WASM__MEMORY_PTR(p, base, width)                                                         \
    do {                                                                                         \
        uint64_t addr_ = (uint64_t)(uint32_t)(base).of.i32 + *pc++;                              \
        if (addr_ + (width) <= mem0_size) {                                                      \
            (p) = mem0_data + addr_;                                                             \
        } else {                                                                                 \
            (p) = wasm__memory_access_slow(mod, 0, &(base), pc[-1], (width), &mem0_data, &mem0_size); \
            if (!(p)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);                                        \
        }                                                                                        \
    } while (0)
#define WASM__SP_PUSH(val)                 \
    do {                                   \
        if (sp == stack_end) {             \
            err = WASM_ERR_STACK_OVERFLOW; \
            goto cleanup;                  \
        }                                  \
        *sp++ = (val);                     \
    } while (0)
/* Only after the same instruction has popped a slot. */
#define WASM__SP_PUT(val) (*sp++ = (val))
#define WASM__SP_POP() (*--sp)
#define WASM__SP_TOP() (sp[-1])

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

/* Returns WASM__ERR_RETURN_FRAME with results on the stack once the frame above base_frame_sp returns. */
static wasm_error_t wasm__interp_loop(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm_value_t* const stack = rt->stack;
    wasm_value_t* const stack_end = rt->stack + rt->max_stack;
    wasm__call_frame_t* cf;
    wasm_module_t* mod;
    wasm_func_t* func;
    wasm__label_t* labels;
    wasm_value_t* locals;
    wasm_value_t* sp;
    const uint32_t* ops;
    const uint32_t* pc;
    const uint32_t* op_at;
    const uint32_t* desc;
    wasm_module_t* call_mod;
    wasm_functype_t* call_type;
    uint32_t call_idx;
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
        [0xBC] = &&wasm__op_0xBC, [0xBD] = &&wasm__op_0xBD, [0xBE] = &&wasm__op_0xBE, [0xBF] = &&wasm__op_0xBF,
        [0xC0] = &&wasm__op_0xC0, [0xC1] = &&wasm__op_0xC1, [0xC2] = &&wasm__op_0xC2, [0xC3] = &&wasm__op_0xC3,
        [0xC4] = &&wasm__op_0xC4, [0xC5] = &&wasm__op_default, [0xC6] = &&wasm__op_default,
        [0xC7] = &&wasm__op_default, [0xC8] = &&wasm__op_default, [0xC9] = &&wasm__op_default,
        [0xCA] = &&wasm__op_default, [0xCB] = &&wasm__op_default, [0xCC] = &&wasm__op_default,
        [0xCD] = &&wasm__op_default, [0xCE] = &&wasm__op_default, [0xCF] = &&wasm__op_default,
        [0xD0] = &&wasm__op_0xD0, [0xD1] = &&wasm__op_0xD1, [0xD2] = &&wasm__op_0xD2, [0xD3] = &&wasm__op_0xD3,
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
        [WASM__OP_LOCAL_GET_REF] = &&wasm__op_WASM__OP_LOCAL_GET_REF,
        [WASM__OP_LOCAL_SET_REF] = &&wasm__op_WASM__OP_LOCAL_SET_REF,
        [WASM__OP_LOCAL_TEE_REF] = &&wasm__op_WASM__OP_LOCAL_TEE_REF,
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
            op_at = fuel_prev;
            WASM__TRAP(WASM_ERR_OUT_OF_FUEL);
        }
        rt->fuel--;
        fuel_prev = op_at;
        goto* wasm__dispatch[op];
        do {
#else
        if (fuel_on) {
            if (rt->fuel == 0) WASM__TRAP(WASM_ERR_OUT_OF_FUEL);
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
                if (WASM__SP_POP().of.i32)
                    pc++;
                else
                    pc = ops + *pc;
                WASM__NEXT();
            WASM__CASE(0x08): { /* throw */
                uint32_t tag_index = *pc++;
                wasm_functype_t* tag_type;
                wasm_value_t payload_buf[8];
                wasm_value_t* payload = payload_buf;
                int j;

                if (tag_index >= mod->num_tags) WASM__TRAP(WASM_ERR_MALFORMED);
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (tag_type->num_params > 8) {
                    payload = (wasm_value_t*)wasm__malloc_array(&rt->allocator, tag_type->num_params, sizeof(wasm_value_t));
                    if (!payload) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)tag_type->num_params - 1; j >= 0; j--) payload[j] = WASM__SP_POP();
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
                wasm_value_t exnref_value = WASM__SP_POP();
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
                if (wasm__interrupt_pending(rt)) WASM__TRAP(WASM_ERR_INTERRUPTED);
                pc = ops + *pc;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_X):
                desc = pc;
                goto do_branch;
            WASM__CASE(WASM__OP_BR_IF):
                if (WASM__SP_POP().of.i32)
                    pc = ops + *pc;
                else
                    pc++;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_IF_LOOP):
                if (WASM__SP_POP().of.i32) {
                    if (wasm__interrupt_pending(rt)) WASM__TRAP(WASM_ERR_INTERRUPTED);
                    pc = ops + *pc;
                } else {
                    pc++;
                }
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_IF_X):
                if (WASM__SP_POP().of.i32) {
                    desc = pc;
                    goto do_branch;
                }
                pc += 4;
                WASM__NEXT();
            WASM__CASE(WASM__OP_BR_TABLE): {
                uint32_t selected = (uint32_t)WASM__SP_POP().of.i32;

                if (selected > pc[0]) selected = pc[0];
                desc = pc + 1u + selected * 4u;
                goto do_branch;
            }
            WASM__CASE(0x0F): /* return */
                goto return_frame;

            WASM__CASE(0x10): /* call */
            WASM__CASE(0x12): /* return_call */
                call_idx = *pc++;
                call_mod = mod;
                call_type = wasm__module_functype(mod, mod->funcs[call_idx].type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (op == 0x12) goto do_tail_call;
                goto do_call;
            WASM__CASE(0x11):   /* call_indirect */
            WASM__CASE(0x13): { /* return_call_indirect */
                uint32_t type_idx = *pc++;
                uint32_t table_idx = *pc++;
                wasm_value_t index_value = WASM__SP_POP();
                wasm_table_t* table;
                uint64_t elem_index;

                if (type_idx >= mod->num_types || table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                elem_index = wasm__table_index_value(table, &index_value);
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
                wasm_value_t funcref_value;

                if (type_idx >= mod->num_types) WASM__TRAP(WASM_ERR_MALFORMED);
                call_type = wasm__module_functype(mod, type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                funcref_value = WASM__SP_POP();
                if (wasm__is_null_ref(&funcref_value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null function reference");
                    WASM__TRAP(WASM_ERR_TRAP);
                }
                if (!wasm__resolve_func_ref(rt, funcref_value.of.funcref, &call_mod, &call_idx))
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

                err = wasm__exec_br_on_cast(mod, pc[0], func->code + pc[1], func->code + func->code_len, &sp[-1],
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
            WASM__CASE(0x1A): {
                wasm_value_t tmp = WASM__SP_POP();
                (void)tmp;
                WASM__NEXT();
            }
            WASM__CASE(0x1B): {
                wasm_value_t c = WASM__SP_POP(), v2 = WASM__SP_POP(), v1 = WASM__SP_POP();
                WASM__SP_PUT(c.of.i32 ? v1 : v2);
                WASM__NEXT();
            }

            /* Variables */
            WASM__CASE(0x20):
                WASM__SP_PUSH(locals[*pc++]);
                WASM__NEXT();
            WASM__CASE(0x21):
                locals[*pc++] = WASM__SP_POP();
                WASM__NEXT();
            WASM__CASE(0x22):
                locals[*pc++] = WASM__SP_TOP();
                WASM__NEXT();
            WASM__CASE(WASM__OP_LOCAL_GET_REF): {
                uint32_t x = *pc++;

                WASM__SP_PUSH(wasm__retag_value_for_declared_type(
                    func->locals[x], func->local_reftypes ? &func->local_reftypes[x] : NULL, locals[x]));
                WASM__NEXT();
            }
            WASM__CASE(WASM__OP_LOCAL_SET_REF):
            WASM__CASE(WASM__OP_LOCAL_TEE_REF): {
                uint32_t x = *pc++;

                locals[x] = wasm__retag_value_for_declared_type(
                    func->locals[x], func->local_reftypes ? &func->local_reftypes[x] : NULL, sp[-1]);
                if (op == WASM__OP_LOCAL_SET_REF) sp--;
                WASM__NEXT();
            }
            WASM__CASE(0x23): {
                uint32_t x = *pc++;
                WASM__SP_PUSH(wasm__global_get_value(&mod->globals[x]));
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
                value = WASM__SP_POP();
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
                wasm_value_t index;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                index = WASM__SP_POP();
                element_index = wasm__table_index_value(table, &index);
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__SP_PUSH(table->elems[element_index]);
                WASM__NEXT();
            }
            WASM__CASE(0x26): {
                uint32_t table_idx = *pc++;
                wasm_table_t* table;
                wasm_value_t value;
                wasm_value_t index;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                value = WASM__SP_POP();
                index = WASM__SP_POP();
                element_index = wasm__table_index_value(table, &index);
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

                WASM__MEMORY_PTR(p, sp[-1], 4u);
                sp[-1] = wasm_i32((int32_t)wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x29): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 8u);
                sp[-1] = wasm_i64((int64_t)wasm__load_le64(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2A): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 4u);
                sp[-1] = wasm__f32_bits(wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2B): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 8u);
                sp[-1] = wasm__f64_bits(wasm__load_le64(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2C): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 1u);
                sp[-1] = wasm_i32((int32_t)(int8_t)*p);
                WASM__NEXT();
            }
            WASM__CASE(0x2D): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 1u);
                sp[-1] = wasm_i32((int32_t)*p);
                WASM__NEXT();
            }
            WASM__CASE(0x2E): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 2u);
                sp[-1] = wasm_i32((int32_t)(int16_t)wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x2F): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 2u);
                sp[-1] = wasm_i32((int32_t)wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x30): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 1u);
                sp[-1] = wasm_i64((int64_t)(int8_t)*p);
                WASM__NEXT();
            }
            WASM__CASE(0x31): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 1u);
                sp[-1] = wasm_i64((int64_t)*p);
                WASM__NEXT();
            }
            WASM__CASE(0x32): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 2u);
                sp[-1] = wasm_i64((int64_t)(int16_t)wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x33): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 2u);
                sp[-1] = wasm_i64((int64_t)wasm__load_le16(p));
                WASM__NEXT();
            }
            WASM__CASE(0x34): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 4u);
                sp[-1] = wasm_i64((int64_t)(int32_t)wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x35): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-1], 4u);
                sp[-1] = wasm_i64((int64_t)wasm__load_le32(p));
                WASM__NEXT();
            }
            WASM__CASE(0x36): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 4u);
                wasm__store_le32(p, (uint32_t)sp[-1].of.i32);
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x37): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 8u);
                wasm__store_le64(p, (uint64_t)sp[-1].of.i64);
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x38): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 4u);
                {
                    uint32_t bits;

                    memcpy(&bits, &sp[-1].of.f32, 4);
                    wasm__store_le32(p, bits);
                }
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x39): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 8u);
                {
                    uint64_t bits;

                    memcpy(&bits, &sp[-1].of.f64, 8);
                    wasm__store_le64(p, bits);
                }
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3A): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 1u);
                *p = (uint8_t)sp[-1].of.i32;
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3B): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 2u);
                wasm__store_le16(p, (uint16_t)sp[-1].of.i32);
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3C): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 1u);
                *p = (uint8_t)sp[-1].of.i64;
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3D): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 2u);
                wasm__store_le16(p, (uint16_t)sp[-1].of.i64);
                sp -= 2;
                WASM__NEXT();
            }
            WASM__CASE(0x3E): {
                uint8_t* p;

                WASM__MEMORY_PTR(p, sp[-2], 4u);
                wasm__store_le32(p, (uint32_t)sp[-1].of.i64);
                sp -= 2;
                WASM__NEXT();
            }

            WASM__CASE(WASM__OP_MEMORY_X): {
                uint32_t memory_op = pc[0];
                wasm_value_t* base = memory_op >= 0x36 ? &sp[-2] : &sp[-1];
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
                WASM__SP_PUSH(memory->is_64 ? wasm_i64((int64_t)wasm__memory_pages(memory))
                                             : wasm_i32((int32_t)wasm__memory_pages(memory)));
                WASM__NEXT();
            }
            WASM__CASE(0x40): {
                wasm_value_t d;
                int64_t res;
                uint32_t mem_idx = *pc++;
                wasm_memory_t* memory;
                d = WASM__SP_POP();
                memory = wasm__memory_at(mod, mem_idx);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                res = wasm_memory_grow(mod,
                                          mem_idx,
                                          memory->is_64 ? (uint64_t)d.of.i64
                                                        : (uint64_t)(uint32_t)d.of.i32);
                if (res >= 0) {
                    uint64_t page_size = memory->page_size ? memory->page_size : WASM_PAGE_SIZE;
                    uint64_t pages = memory->is_64 ? (uint64_t)d.of.i64 : (uint64_t)(uint32_t)d.of.i32;

                    WASM__CHARGE_BYTES(rt, pages * page_size);
                }
                mem0_size = 0;
                WASM__SP_PUSH(memory->is_64 ? wasm_i64(res) : wasm_i32((int32_t)res));
                WASM__NEXT();
            }

            /* Reference instructions */
            WASM__CASE(0xD0): {
                wasm_valtype_t type;
                wasm_reftype_t reftype = { 0 };
                cf->r.ptr = func->code + *pc++;
                err = wasm__read_heaptype(mod, &cf->r, &reftype);
                if (err != WASM_OK) goto cleanup;
                type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;
                WASM__SP_PUSH(wasm__typed_ref_null(type, &reftype));
                WASM__NEXT();
            }
            WASM__CASE(0xD1): {
                wasm_value_t value = WASM__SP_POP();
                WASM__SP_PUSH(wasm_i32(wasm__is_null_ref(&value) ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0xD2): {
                uint32_t func_index = *pc++;
                wasm_value_t funcref;
                if (func_index >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__CALL_OUT(wasm__make_funcref(mod, func_index, &funcref));
                WASM__SP_PUSH(funcref);
                WASM__NEXT();
            }
            WASM__CASE(0xD3): {
                wasm_value_t rhs = WASM__SP_POP();
                wasm_value_t lhs = WASM__SP_POP();
                int equal;

                if (!wasm__is_ref_type(lhs.type) || !wasm__is_ref_type(rhs.type)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (wasm__uses_funcref_storage(lhs.type) || wasm__uses_funcref_storage(rhs.type))
                    equal = wasm__uses_funcref_storage(lhs.type) && wasm__uses_funcref_storage(rhs.type) &&
                            lhs.of.funcref == rhs.of.funcref;
                else if (wasm__uses_externref_storage(lhs.type) || wasm__uses_externref_storage(rhs.type))
                    equal = wasm__uses_externref_storage(lhs.type) && wasm__uses_externref_storage(rhs.type) &&
                            lhs.of.externref == rhs.of.externref;
                else
                    equal = lhs.of.gc_ref == rhs.of.gc_ref;
                WASM__SP_PUSH(wasm_i32(equal ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0xD4): {
                wasm_reftype_t actual_ref;
                wasm_value_t value = WASM__SP_POP();

                if (!wasm__runtime_value_reftype(mod, &value, &actual_ref)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (wasm__is_null_ref(&value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null reference");
                    err = WASM_ERR_TRAP;
                    goto cleanup;
                }
                actual_ref.nullable = 0;
                WASM__SP_PUSH(wasm__runtime_cast_success_value(&actual_ref, value));
                WASM__NEXT();
            }
            WASM__CASE(0xD5):
            WASM__CASE(0xD6): {
                wasm_reftype_t target_ref;
                wasm_value_t value = WASM__SP_POP();

                if (!wasm__runtime_value_reftype(mod, &value, &target_ref)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                target_ref.nullable = 0;
                desc = pc;
                pc += 4;
                if (wasm__is_null_ref(&value)) {
                    if (op == 0xD5) goto do_branch;
                    WASM__NEXT();
                }
                WASM__SP_PUT(wasm__runtime_cast_success_value(&target_ref, value));
                if (op == 0xD6) goto do_branch;
                WASM__NEXT();
            }

            /* Constants */
            WASM__CASE(0x41):
            WASM__CASE(0x43):
                WASM__SP_PUSH(op == 0x41 ? wasm_i32((int32_t)*pc) : wasm__f32_bits(*pc));
                pc++;
                WASM__NEXT();
            WASM__CASE(0x42):
            WASM__CASE(0x44): {
                uint64_t bits = (uint64_t)pc[0] | ((uint64_t)pc[1] << 32);

                WASM__SP_PUSH(op == 0x42 ? wasm_i64((int64_t)bits) : wasm__f64_bits(bits));
                pc += 2;
                WASM__NEXT();
            }

            WASM__CASE(0x45): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 == 0 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x46): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 == b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x47): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 != b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x48): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 < b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x49): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 < (uint32_t)b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x4A): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 > b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x4B): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 > (uint32_t)b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x4C): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 <= b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x4D): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 <= (uint32_t)b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x4E): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 >= b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x4F): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 >= (uint32_t)b.of.i32 ? 1 : 0));
                WASM__NEXT();
            }

            /* i64 comparison */
            WASM__CASE(0x50): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 == 0 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x51): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 == b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x52): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 != b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x53): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 < b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x54): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 < (uint64_t)b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x55): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 > b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x56): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 > (uint64_t)b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x57): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 <= b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x58): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 <= (uint64_t)b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x59): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 >= b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x5A): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 >= (uint64_t)b.of.i64 ? 1 : 0));
                WASM__NEXT();
            }

            /* f32/f64 comparison */
            WASM__CASE(0x5B): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 == b.of.f32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x5C): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 != b.of.f32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x5D): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 < b.of.f32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x5E): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 > b.of.f32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x5F): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 <= b.of.f32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x60): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 >= b.of.f32 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x61): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 == b.of.f64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x62): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 != b.of.f64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x63): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 < b.of.f64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x64): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 > b.of.f64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x65): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 <= b.of.f64 ? 1 : 0));
                WASM__NEXT();
            }
            WASM__CASE(0x66): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 >= b.of.f64 ? 1 : 0));
                WASM__NEXT();
            }

            /* i32 arithmetic */
            WASM__CASE(0x67): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)wasm__clz32((uint32_t)a.of.i32)));
                WASM__NEXT();
            }
            WASM__CASE(0x68): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)wasm__ctz32((uint32_t)a.of.i32)));
                WASM__NEXT();
            }
            WASM__CASE(0x69): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)wasm__popcnt32((uint32_t)a.of.i32)));
                WASM__NEXT();
            }
            WASM__CASE(0x6A): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 + (uint32_t)b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x6B): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 - (uint32_t)b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x6C): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 * (uint32_t)b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x6D): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                if (a.of.i32 == INT32_MIN && b.of.i32 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                WASM__SP_PUT(wasm_i32(a.of.i32 / b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x6E): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i32((int32_t)((uint32_t)a.of.i32 / (uint32_t)b.of.i32)));
                WASM__NEXT();
            }
            WASM__CASE(0x6F): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i32((a.of.i32 == INT32_MIN && b.of.i32 == -1) ? 0 : a.of.i32 % b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x70): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i32((int32_t)((uint32_t)a.of.i32 % (uint32_t)b.of.i32)));
                WASM__NEXT();
            }
            WASM__CASE(0x71): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 & b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x72): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 | b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x73): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 ^ b.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0x74): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 << ((uint32_t)b.of.i32 & 31u)));
                WASM__NEXT();
            }
            WASM__CASE(0x75): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits(wasm__shr_s32_bits((uint32_t)a.of.i32,
                                                                  (uint32_t)b.of.i32)));
                WASM__NEXT();
            }
            WASM__CASE(0x76): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)((uint32_t)a.of.i32 >> (b.of.i32 & 31))));
                WASM__NEXT();
            }
            WASM__CASE(0x77): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i32 & 31u;
                WASM__SP_PUT(
                           wasm__u32_bits(((uint32_t)a.of.i32 << k) |
                                          ((uint32_t)a.of.i32 >> ((32u - k) & 31u))));
                WASM__NEXT();
            }
            WASM__CASE(0x78): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i32 & 31u;
                WASM__SP_PUT(
                           wasm__u32_bits(((uint32_t)a.of.i32 >> k) |
                                          ((uint32_t)a.of.i32 << ((32u - k) & 31u))));
                WASM__NEXT();
            }

            /* i64 arithmetic */
            WASM__CASE(0x79): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)wasm__clz64((uint64_t)a.of.i64)));
                WASM__NEXT();
            }
            WASM__CASE(0x7A): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)wasm__ctz64((uint64_t)a.of.i64)));
                WASM__NEXT();
            }
            WASM__CASE(0x7B): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)wasm__popcnt64((uint64_t)a.of.i64)));
                WASM__NEXT();
            }
            WASM__CASE(0x7C): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 + (uint64_t)b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x7D): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 - (uint64_t)b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x7E): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 * (uint64_t)b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x7F): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                if (a.of.i64 == INT64_MIN && b.of.i64 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                WASM__SP_PUT(wasm_i64(a.of.i64 / b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x80): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i64((int64_t)((uint64_t)a.of.i64 / (uint64_t)b.of.i64)));
                WASM__NEXT();
            }
            WASM__CASE(0x81): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i64((a.of.i64 == INT64_MIN && b.of.i64 == -1) ? 0 : a.of.i64 % b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x82): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i64((int64_t)((uint64_t)a.of.i64 % (uint64_t)b.of.i64)));
                WASM__NEXT();
            }
            WASM__CASE(0x83): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64(a.of.i64 & b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x84): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64(a.of.i64 | b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x85): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64(a.of.i64 ^ b.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0x86): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 << ((uint32_t)b.of.i64 & 63u)));
                WASM__NEXT();
            }
            WASM__CASE(0x87): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits(wasm__shr_s64_bits((uint64_t)a.of.i64,
                                                                  (uint32_t)b.of.i64)));
                WASM__NEXT();
            }
            WASM__CASE(0x88): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)((uint64_t)a.of.i64 >> (b.of.i64 & 63))));
                WASM__NEXT();
            }
            WASM__CASE(0x89): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i64 & 63u;
                WASM__SP_PUT(
                           wasm__u64_bits(((uint64_t)a.of.i64 << k) |
                                          ((uint64_t)a.of.i64 >> ((64u - k) & 63u))));
                WASM__NEXT();
            }
            WASM__CASE(0x8A): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i64 & 63u;
                WASM__SP_PUT(
                           wasm__u64_bits(((uint64_t)a.of.i64 >> k) |
                                          ((uint64_t)a.of.i64 << ((64u - k) & 63u))));
                WASM__NEXT();
            }

            /* f32 arithmetic */
            WASM__CASE(0x8B): {
                wasm_value_t a = WASM__SP_POP();
                uint32_t bits;
                memcpy(&bits, &a.of.f32, sizeof(bits));
                WASM__SP_PUT(wasm__f32_bits(bits & 0x7FFFFFFFu));
                WASM__NEXT();
            }
            WASM__CASE(0x8C): {
                wasm_value_t a = WASM__SP_POP();
                uint32_t bits;
                memcpy(&bits, &a.of.f32, sizeof(bits));
                WASM__SP_PUT(wasm__f32_bits(bits ^ 0x80000000u));
                WASM__NEXT();
            }
            WASM__CASE(0x8D): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__ceil_f32(a.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x8E): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__floor_f32(a.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x8F): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__trunc_f32(a.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x90): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__nearest_f32(a.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x91): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__sqrt_f32(a.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x92): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 + b.of.f32));
                WASM__NEXT();
            }
            WASM__CASE(0x93): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 - b.of.f32));
                WASM__NEXT();
            }
            WASM__CASE(0x94): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 * b.of.f32));
                WASM__NEXT();
            }
            WASM__CASE(0x95): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 / b.of.f32));
                WASM__NEXT();
            }
            WASM__CASE(0x96): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__f32_min(a.of.f32, b.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x97): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__f32_max(a.of.f32, b.of.f32)));
                WASM__NEXT();
            }
            WASM__CASE(0x98): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t ab, bb, rb;
                memcpy(&ab, &a.of.f32, 4);
                memcpy(&bb, &b.of.f32, 4);
                rb = (ab & 0x7FFFFFFFu) | (bb & 0x80000000u);
                WASM__SP_PUT(wasm__f32_bits(rb));
                WASM__NEXT();
            }

            /* f64 arithmetic */
            WASM__CASE(0x99): {
                wasm_value_t a = WASM__SP_POP();
                uint64_t bits;
                memcpy(&bits, &a.of.f64, sizeof(bits));
                WASM__SP_PUT(wasm__f64_bits(bits & UINT64_C(0x7FFFFFFFFFFFFFFF)));
                WASM__NEXT();
            }
            WASM__CASE(0x9A): {
                wasm_value_t a = WASM__SP_POP();
                uint64_t bits;
                memcpy(&bits, &a.of.f64, sizeof(bits));
                WASM__SP_PUT(wasm__f64_bits(bits ^ UINT64_C(0x8000000000000000)));
                WASM__NEXT();
            }
            WASM__CASE(0x9B): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__ceil_f64(a.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0x9C): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__floor_f64(a.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0x9D): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__trunc_f64(a.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0x9E): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__nearest_f64(a.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0x9F): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__sqrt_f64(a.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0xA0): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 + b.of.f64));
                WASM__NEXT();
            }
            WASM__CASE(0xA1): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 - b.of.f64));
                WASM__NEXT();
            }
            WASM__CASE(0xA2): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 * b.of.f64));
                WASM__NEXT();
            }
            WASM__CASE(0xA3): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 / b.of.f64));
                WASM__NEXT();
            }
            WASM__CASE(0xA4): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__f64_min(a.of.f64, b.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0xA5): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__f64_max(a.of.f64, b.of.f64)));
                WASM__NEXT();
            }
            WASM__CASE(0xA6): {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint64_t ab, bb, rb;
                memcpy(&ab, &a.of.f64, 8);
                memcpy(&bb, &b.of.f64, 8);
                rb = (ab & (uint64_t)0x7FFFFFFFFFFFFFFFull) | (bb & (uint64_t)0x8000000000000000ull);
                WASM__SP_PUT(wasm__f64_bits(rb));
                WASM__NEXT();
            }

            /* Conversions */
            WASM__CASE(0xA7): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)a.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0xA8): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_s((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xA9): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_u((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xAA): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_s(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xAB): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_u(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xAC): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)a.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0xAD): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)(uint32_t)a.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0xAE): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_s((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xAF): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_u((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xB0): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_s(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xB1): {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_u(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                WASM__NEXT();
            }
            WASM__CASE(0xB2): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)a.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0xB3): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)(uint32_t)a.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0xB4): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)a.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0xB5): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)(uint64_t)a.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0xB6): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)a.of.f64));
                WASM__NEXT();
            }
            WASM__CASE(0xB7): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)a.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0xB8): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)(uint32_t)a.of.i32));
                WASM__NEXT();
            }
            WASM__CASE(0xB9): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)a.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0xBA): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)(uint64_t)a.of.i64));
                WASM__NEXT();
            }
            WASM__CASE(0xBB): {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)a.of.f32));
                WASM__NEXT();
            }

            /* Reinterpretations */
            WASM__CASE(0xBC): {
                wasm_value_t a = WASM__SP_POP();
                int32_t v;
                memcpy(&v, &a.of.f32, 4);
                WASM__SP_PUT(wasm_i32(v));
                WASM__NEXT();
            }
            WASM__CASE(0xBD): {
                wasm_value_t a = WASM__SP_POP();
                int64_t v;
                memcpy(&v, &a.of.f64, 8);
                WASM__SP_PUT(wasm_i64(v));
                WASM__NEXT();
            }
            WASM__CASE(0xBE): {
                wasm_value_t a = WASM__SP_POP();
                a.type = WASM_TYPE_F32;
                WASM__SP_PUT(a);
                WASM__NEXT();
            }
            WASM__CASE(0xBF): {
                wasm_value_t a = WASM__SP_POP();
                a.type = WASM_TYPE_F64;
                WASM__SP_PUT(a);
                WASM__NEXT();
            }

            /* Sign extension */
            WASM__CASE(0xC0): {
                wasm_value_t a = WASM__SP_POP();
                uint32_t value = (uint32_t)a.of.i32 & 0xFFu;
                if (value & 0x80u) value |= 0xFFFFFF00u;
                WASM__SP_PUT(wasm_i32((int32_t)value));
                WASM__NEXT();
            }
            WASM__CASE(0xC1): {
                wasm_value_t a = WASM__SP_POP();
                uint32_t value = (uint32_t)a.of.i32 & 0xFFFFu;
                if (value & 0x8000u) value |= 0xFFFF0000u;
                WASM__SP_PUT(wasm_i32((int32_t)value));
                WASM__NEXT();
            }
            WASM__CASE(0xC2): {
                wasm_value_t a = WASM__SP_POP();
                uint64_t value = (uint64_t)a.of.i64 & 0xFFu;
                if (value & 0x80u) value |= 0xFFFFFFFFFFFFFF00ull;
                WASM__SP_PUT(wasm_i64((int64_t)value));
                WASM__NEXT();
            }
            WASM__CASE(0xC3): {
                wasm_value_t a = WASM__SP_POP();
                uint64_t value = (uint64_t)a.of.i64 & 0xFFFFu;
                if (value & 0x8000u) value |= 0xFFFFFFFFFFFF0000ull;
                WASM__SP_PUT(wasm_i64((int64_t)value));
                WASM__NEXT();
            }
            WASM__CASE(0xC4): {
                wasm_value_t a = WASM__SP_POP();
                uint64_t value = (uint64_t)a.of.i64 & 0xFFFFFFFFull;
                if (value & 0x80000000ull) value |= 0xFFFFFFFF00000000ull;
                WASM__SP_PUT(wasm_i64((int64_t)value));
                WASM__NEXT();
            }

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
        wasm_value_t* dst = stack + sp_base + desc[2];

        if (desc[3] & WASM__BRANCH_RETURN) goto return_frame;
        if ((desc[3] & WASM__BRANCH_LOOP) && wasm__interrupt_pending(rt)) WASM__TRAP(WASM_ERR_INTERRUPTED);
        if (arity > 0 && sp - arity != dst) memmove(dst, sp - arity, arity * sizeof(wasm_value_t));
        sp = dst + arity;
        if (pops > 0) {
            wasm__drop_labels(labels, lsp, pops);
            lsp -= pops;
        }
        pc = ops + desc[0];
        WASM__NEXT();
    }

    do_call:
        if (call_mod->funcs[call_idx].is_import) {
            sp -= call_type->num_params;
            WASM__SAVE_STATE();
            err = wasm__call_host_from_stack(call_mod, call_type, &call_mod->funcs[call_idx], sp);
            sp = stack + rt->sp;
            mem0_size = 0;
            WASM__LOAD_FUEL();
            if (err == WASM_OK) WASM__NEXT();
            if (err != WASM__ERR_EXCEPTION) goto cleanup;
            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 0);
            if (err != WASM__ERR_RESUME_FRAME) return err;
            goto resume_frame;
        }
        sp -= call_type->num_params;
        WASM__SAVE_STATE();
        err = wasm__push_call_frame(call_mod, call_idx, sp, call_type->num_params, NULL, 0);
        if (err != WASM_OK) goto cleanup;
        WASM__ENTER_FRAME();
        WASM__NEXT();

    do_tail_call: {
        wasm_value_t* results_dst = cf->results_dst;
        uint32_t num_results = cf->num_results;
        uint32_t num_params = call_type->num_params;

        if (call_mod->funcs[call_idx].is_import) {
            sp -= num_params;
            WASM__SAVE_STATE();
            err = wasm__call_host_from_stack(call_mod, call_type, &call_mod->funcs[call_idx], sp);
            sp = stack + rt->sp;
            mem0_size = 0;
            WASM__LOAD_FUEL();
            if (err == WASM_OK) goto return_frame;
            if (err != WASM__ERR_EXCEPTION) goto cleanup;
            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 1);
            if (err != WASM__ERR_RESUME_FRAME) return err;
            goto resume_frame;
        }
        if (num_params > 0 && sp - num_params != stack + sp_base)
            memmove(stack + sp_base, sp - num_params, num_params * sizeof(wasm_value_t));
        sp = stack + sp_base;
        wasm__drop_labels(labels, lsp, lsp);
        lsp = 0;
        WASM__SAVE_STATE();
        wasm__arena_reset(rt, cf->arena_saved);
        rt->call_frame_sp--;
        err = wasm__push_call_frame(call_mod, call_idx, sp, num_params, results_dst, num_results);
        if (err != WASM_OK) return err;
        WASM__ENTER_FRAME();
        WASM__NEXT();
    }

    throw_pending:
        WASM__SAVE_STATE();
        err = wasm__handle_exception_frames(mod, base_frame_sp);
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
            const wasm_functype_t* ft = cf->ft;
            wasm_value_t* dst = stack + sp_base;
            wasm_value_t* src = sp - ft->num_results;

            for (i = 0; i < ft->num_results; i++) {
                dst[i] = wasm__is_numeric_valtype(ft->results[i])
                             ? src[i]
                             : wasm__retag_value_for_declared_type(
                                   ft->results[i], ft->result_reftypes ? &ft->result_reftypes[i] : NULL, src[i]);
            }
            sp = dst + ft->num_results;
            wasm__drop_labels(labels, lsp, lsp);
            cf->lsp = 0;
            wasm__arena_reset(rt, cf->arena_saved);
            rt->call_frame_sp--;
            rt->sp = (uint32_t)(sp - stack);
            WASM__ENTER_FRAME();
            WASM__NEXT();
        }
    }

cleanup:
    WASM__SAVE_STATE();
    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }

    return err;
}

#undef WASM__SP_TOP
#undef WASM__SP_POP
#undef WASM__SP_PUT
#undef WASM__SP_PUSH
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

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    uint32_t base_frame_sp = rt->call_frame_sp;
    wasm_error_t err;

    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import)
        return wasm__invoke_host_func(mod, wasm__module_functype(mod, func->type_idx), func->host_func,
                                      args, results, func->host_userdata);

    err = wasm__push_call_frame(mod, func_idx, args, num_args, results, num_results);
    if (err != WASM_OK) return err;
    err = wasm__interp_loop(rt, base_frame_sp);
    if (err == WASM__ERR_RETURN_FRAME) err = wasm__finish_call_frame(rt, base_frame_sp);
    if (err != WASM_OK && rt->call_frame_sp > base_frame_sp)
        wasm__capture_backtrace(rt, base_frame_sp);
    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm__clear_call_frame(cf);
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }
    return err;
}
