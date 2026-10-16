#include "../internal.h"

WASM_PRIVATE void wasm__skip_immediates(uint8_t op, wasm__reader_t* r) {
    switch (op) {
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x06:
            wasm__skip_blocktype(r);
            break;
        case 0x1F:
            wasm__skip_try_table_immediates(r);
            break;
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0C:
        case 0x0D:
        case 0x10:
        case 0x12:
        case 0x14:
        case 0x15:
        case 0x18:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x3F:
        case 0x40:
            wasm__read_leb128_u32(r);
            break;
        case 0x41:
            wasm__read_leb128_i32(r);
            break;
        case 0x42:
            wasm__read_leb128_i64(r);
            break;
        case 0x43:
            r->ptr += 4;
            break;
        case 0x44:
            r->ptr += 8;
            break;
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E: {
            wasm__memarg_t memarg;
            if (wasm__read_memarg(r, &memarg) != WASM_OK) {
                r->ptr = r->end;
                return;
            }
        } break;
        case 0x0E: {
            uint32_t n = wasm__read_leb128_u32(r), j;
            for (j = 0; j <= n; j++) wasm__read_leb128_u32(r);
            break;
        }
        case 0x11:
        case 0x13:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x1C: {
            uint32_t count = wasm__read_leb128_u32(r);
            while (count-- > 0 && r->ptr < r->end) wasm__skip_valtype(r);
            break;
        }
        case 0xD0:
            wasm__skip_heaptype(r);
            break;
        case 0xD2:
            wasm__read_leb128_u32(r);
            break;
        case 0xD5:
        case 0xD6:
            wasm__read_leb128_u32(r);
            break;
        case 0xFC:
            wasm__skip_prefixed_immediates(r);
            break;
        case 0xFB:
            wasm__skip_gc_immediates(r);
            break;
        case 0xFD:
            wasm__skip_simd_immediates(r);
            break;
        case 0x19:
        default:
            break;
    }
}

WASM_PRIVATE const uint8_t* wasm__find_try_clause_boundary(const uint8_t* start, const uint8_t* code_end) {
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

    targets = (wasm__control_target_t*)WASM_MALLOC(func->code_len * sizeof(wasm__control_target_t));
    if (!targets) return WASM_ERR_OOM;

    stack_capacity = func->max_label_depth ? func->max_label_depth : 1u;
    stack = (wasm__control_frame_t*)WASM_MALLOC((size_t)stack_capacity * sizeof(*stack));
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

WASM_PRIVATE const wasm__control_target_t* wasm__control_target_at(const wasm_func_t* func, const uint8_t* pc) {
    uint32_t offset;

    if (!func || !func->control_targets || !func->code || pc < func->code) return NULL;
    offset = (uint32_t)(pc - func->code);
    if (offset >= func->code_len) return NULL;
    if (func->control_targets[offset].end_offset == UINT32_MAX) return NULL;
    return &func->control_targets[offset];
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
