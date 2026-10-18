#include "control.h"
#include "../module.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../reader.h"
#include "../validate/function.h"

typedef struct wasm__control_frame_t {
    uint8_t opcode;
    uint32_t offset;
} wasm__control_frame_t;

static const uint8_t* wasm__find_try_clause_boundary(const uint8_t* start, const uint8_t* code_end) {
    wasm__reader_t scan;
    int nest = 1;

    scan.ptr = start;
    scan.end = code_end;
    scan.malformed = 0;
    while (scan.ptr < scan.end) {
        const uint8_t* at = scan.ptr;
        uint8_t b = wasm__read_u8(&scan);

        if (scan.malformed) return code_end;

        if (b == 0x02 || b == 0x03 || b == 0x04 || b == 0x06 || b == 0x1F) {
            nest++;
            wasm__skip_immediates(b, &scan);
        } else if ((b == 0x07 || b == 0x18 || b == 0x19) && nest == 1) {
            return at;
        } else if (b == 0x0B) {
            nest--;
            if (nest == 0) return at;
        } else {
            wasm__skip_immediates(b, &scan);
        }
    }

    return code_end;
}

WASM_PRIVATE wasm_error_t wasm__build_control_targets(wasm_func_t* func) {
    wasm__reader_t scan;
    wasm__control_target_t* targets;
    wasm__control_frame_t* stack;
    uint32_t stack_capacity;
    uint32_t depth = 0;
    uint32_t i;

    if (!func->code || func->code_len == 0) return WASM_OK;
    if (func->control_targets) return WASM_OK;

    targets = (wasm__control_target_t*)wasm__malloc_array(func->code_len, sizeof(wasm__control_target_t));
    if (!targets) return WASM_ERR_OOM;

    stack_capacity = func->max_label_depth ? func->max_label_depth : 1u;
    stack = (wasm__control_frame_t*)wasm__malloc_array((size_t)stack_capacity, sizeof(*stack));
    if (!stack) {
        WASM_FREE(targets);
        return WASM_ERR_OOM;
    }

    for (i = 0; i < func->code_len; i++) {
        targets[i].end_offset = UINT32_MAX;
        targets[i].aux_offset = UINT32_MAX;
    }

    scan.ptr = func->code;
    scan.end = func->code + func->code_len;
    scan.malformed = 0;
    while (scan.ptr < scan.end) {
        const uint8_t* at = scan.ptr;
        uint8_t op = wasm__read_u8(&scan);

        if (scan.malformed) {
            WASM_FREE(stack);
            WASM_FREE(targets);
            return WASM_ERR_MALFORMED;
        }

        switch (op) {
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x06:
            case 0x1F:
                if (depth >= stack_capacity) {
                    WASM_FREE(stack);
                    WASM_FREE(targets);
                    return WASM_ERR_MALFORMED;
                }
                stack[depth].opcode = op;
                stack[depth].offset = (uint32_t)(at - func->code);
                wasm__skip_immediates(op, &scan);
                depth++;
                break;
            case 0x05:
                if (depth > 0 && stack[depth - 1].opcode == 0x04)
                    targets[stack[depth - 1].offset].aux_offset = (uint32_t)(scan.ptr - func->code);
                break;
            case 0x07:
            case 0x18:
            case 0x19:
                if (depth > 0 && stack[depth - 1].opcode == 0x06 &&
                    targets[stack[depth - 1].offset].aux_offset == UINT32_MAX)
                    targets[stack[depth - 1].offset].aux_offset = (uint32_t)(at - func->code);
                wasm__skip_immediates(op, &scan);
                break;
            case 0x0B:
                if (depth == 0) {
                    if (scan.ptr != scan.end) {
                        WASM_FREE(stack);
                        WASM_FREE(targets);
                        return WASM_ERR_MALFORMED;
                    }
                    break;
                }
                depth--;
                targets[stack[depth].offset].end_offset = (uint32_t)(scan.ptr - func->code);
                break;
            default:
                wasm__skip_immediates(op, &scan);
                break;
        }

        if (scan.malformed) {
            WASM_FREE(stack);
            WASM_FREE(targets);
            return WASM_ERR_MALFORMED;
        }
    }

    if (depth != 0) {
        WASM_FREE(stack);
        WASM_FREE(targets);
        return WASM_ERR_MALFORMED;
    }

    WASM_FREE(stack);
    func->control_targets = targets;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__build_module_control_targets(wasm_module_t* mod) {
    uint32_t i;

    for (i = mod->num_func_imports; i < mod->num_funcs; i++) {
        wasm_error_t err = wasm__prepare_function(mod, i);
        if (err != WASM_OK) return err;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__read_try_clause(const uint8_t* clause_ptr,
                                          const uint8_t* code_end,
                                          wasm__try_clause_t* out_clause) {
    wasm__reader_t r;

    if (!clause_ptr || clause_ptr >= code_end) return WASM_ERR_MALFORMED;

    r.ptr = clause_ptr;
    r.end = code_end;
    r.malformed = 0;
    out_clause->opcode = wasm__read_u8(&r);
    out_clause->immediate = UINT32_MAX;

    switch (out_clause->opcode) {
        case 0x07:
        case 0x18:
            out_clause->immediate = wasm__read_leb128_u32(&r);
            break;
        case 0x19:
            break;
        default:
            return WASM_ERR_MALFORMED;
    }

    if (r.malformed) return WASM_ERR_MALFORMED;

    out_clause->body_start = r.ptr;
    out_clause->next_clause = wasm__find_try_clause_boundary(out_clause->body_start, code_end);
    return WASM_OK;
}
