#include "instructions.h"
#include "module.h"
#include "portable.h"
#include "reader.h"
#include "types/canonical.h"
#include "types/references.h"
#include <string.h>

static void wasm__skip_simd_immediates(wasm__reader_t* r);

WASM_PRIVATE void wasm__skip_blocktype(wasm__reader_t* r) {
    uint8_t lead = *r->ptr;

    if (lead == 0x40 || wasm__is_valtype_byte(lead)) {
        if (lead == 0x40)
            r->ptr++;
        else
            wasm__skip_valtype(r);
        return;
    }

    (void)wasm__read_leb128_i32(r);
}

WASM_PRIVATE wasm_error_t wasm__read_try_table_clause(wasm__reader_t* r,
                                                wasm__try_table_clause_t* out_clause) {
    if (!r || !out_clause) return WASM_ERR_MALFORMED;

    memset(out_clause, 0, sizeof(*out_clause));
    out_clause->kind = wasm__read_u8(r);
    switch (out_clause->kind) {
        case 0x00:
        case 0x01:
            out_clause->tag_index = wasm__read_leb128_u32(r);
            out_clause->label_depth = wasm__read_leb128_u32(r);
            break;
        case 0x02:
        case 0x03:
            out_clause->label_depth = wasm__read_leb128_u32(r);
            break;
        default:
            return WASM_ERR_MALFORMED;
    }

    if (r->malformed) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

static void wasm__skip_try_table_immediates(wasm__reader_t* r) {
    uint32_t count;

    wasm__skip_blocktype(r);
    count = wasm__read_leb128_u32(r);
    while (count-- > 0 && r->ptr < r->end) {
        wasm__try_table_clause_t clause;

        if (wasm__read_try_table_clause(r, &clause) != WASM_OK) {
            r->ptr = r->end;
            r->malformed = 1;
            return;
        }
    }
}

static void wasm__skip_prefixed_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x08:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x09:
            wasm__read_leb128_u32(r);
            break;
        case 0x0A:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0B:
            wasm__read_leb128_u32(r);
            break;
        case 0x0C:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0D:
            wasm__read_leb128_u32(r);
            break;
        case 0x0E:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0F:
        case 0x10:
        case 0x11:
            wasm__read_leb128_u32(r);
            break;
        default:
            break;
    }
}

static void wasm__skip_gc_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x06:
        case 0x07:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x10:
            wasm__read_leb128_u32(r);
            break;
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x09:
        case 0x0A:
        case 0x11:
        case 0x12:
        case 0x13:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x08:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            wasm__skip_heaptype(r);
            break;
        case 0x18:
        case 0x19:
            if (r->ptr < r->end) r->ptr++;
            wasm__read_leb128_u32(r);
            wasm__skip_heaptype(r);
            wasm__skip_heaptype(r);
            break;
        default:
            break;
    }
}

WASM_PRIVATE wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype) {
    uint8_t lead = *r->ptr;
    wasm_error_t err;

    blocktype->param_arity = 0;
    blocktype->result_arity = 0;
    blocktype->params = NULL;
    blocktype->results = NULL;
    blocktype->param_reftypes = NULL;
    blocktype->result_reftypes = NULL;
    blocktype->inline_result = WASM_TYPE_VOID;
    wasm__clear_reftype(&blocktype->inline_result_reftype);

    if (lead == 0x40) {
        r->ptr++;
        return WASM_OK;
    }

    if (wasm__is_valtype_byte(lead)) {
        err = wasm__read_valtype(mod, r, &blocktype->inline_result, &blocktype->inline_result_reftype);
        if (err != WASM_OK) return err;
        blocktype->result_arity = 1;
        blocktype->results = &blocktype->inline_result;
        blocktype->result_reftypes = &blocktype->inline_result_reftype;
        return WASM_OK;
    }

    {
        int32_t type_idx = wasm__read_leb128_i32(r);
        wasm_functype_t* ft;

        if (wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE) != WASM_OK) return WASM_ERR_MALFORMED;
        if (type_idx < 0 || (uint32_t)type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
        ft = wasm__module_functype(mod, (uint32_t)type_idx);
        if (!ft) return WASM_ERR_MALFORMED;
        blocktype->param_arity = ft->num_params;
        blocktype->result_arity = ft->num_results;
        blocktype->params = ft->params;
        blocktype->results = ft->results;
        blocktype->param_reftypes = ft->param_reftypes;
        blocktype->result_reftypes = ft->result_reftypes;
    }

    return WASM_OK;
}

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
        case 0xFE: {
            uint32_t subop = wasm__read_leb128_u32(r);
            wasm__memarg_t memarg;
            if (subop == 0x03)
                wasm__read_u8(r);
            else if (wasm__read_memarg(r, &memarg) != WASM_OK)
                r->malformed = 1;
            break;
        }
        case 0x19:
        default:
            break;
    }
}

WASM_PRIVATE uint32_t wasm__simd_memory_align_log2(uint32_t subop) {
    switch (subop) {
        case 0x00:
        case 0x0B:
            return 4;
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
            return 3;
        case 0x07:
        case 0x54:
        case 0x58:
            return 0;
        case 0x08:
        case 0x55:
        case 0x59:
            return 1;
        case 0x09:
        case 0x5C:
        case 0x56:
        case 0x5A:
            return 2;
        case 0x0A:
        case 0x5D:
        case 0x57:
        case 0x5B:
            return 3;
        default:
            return UINT32_MAX;
    }
}

WASM_PRIVATE uint32_t wasm__simd_lane_limit(uint32_t subop) {
    switch (subop) {
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x54:
        case 0x58:
            return 16;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x55:
        case 0x59:
            return 8;
        case 0x1B:
        case 0x1C:
        case 0x1F:
        case 0x20:
        case 0x56:
        case 0x5A:
            return 4;
        default:
            return 2;
    }
}

static void wasm__skip_simd_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D: {
            wasm__memarg_t memarg;
            if (wasm__read_memarg(r, &memarg) != WASM_OK) {
                r->ptr = r->end;
                return;
            }
            if (subop >= 0x54 && subop <= 0x5B) (void)wasm__read_u8(r);
            break;
        }
        case 0x0C:
            r->ptr += 16;
            if (r->ptr > r->end) {
                r->ptr = r->end;
                r->malformed = 1;
            }
            break;
        case 0x0D: {
            uint32_t lane;
            for (lane = 0; lane < 16 && r->ptr < r->end; lane++) (void)wasm__read_u8(r);
            break;
        }
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
            (void)wasm__read_u8(r);
            break;
        default:
            break;
    }
}

WASM_PRIVATE int wasm__atomic_access(uint32_t subop, uint32_t* out_size_log2, wasm_valtype_t* out_type) {
    static const uint8_t size_log2[7] = { 2, 3, 0, 1, 0, 1, 2 };

    if (subop == 0x00 || subop == 0x01 || subop == 0x02) {
        *out_size_log2 = (subop == 0x02) ? 3u : 2u;
        *out_type = (subop == 0x02) ? WASM_TYPE_I64 : WASM_TYPE_I32;
        return 1;
    }
    if (subop < 0x10 || subop > 0x4E) return 0;
    subop = (subop - 0x10u) % 7u;
    *out_size_log2 = size_log2[subop];
    *out_type = (subop == 0 || subop == 2 || subop == 3) ? WASM_TYPE_I32 : WASM_TYPE_I64;
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__read_memarg(wasm__reader_t* r, wasm__memarg_t* out_memarg) {
    uint32_t flags = wasm__read_leb128_u32(r);

    if ((flags & ~0x7Fu) != 0) return WASM_ERR_MALFORMED;

    out_memarg->align_log2 = flags & 0x3Fu;
    out_memarg->memory_index = 0;
    out_memarg->has_memory_index = (uint8_t)(((flags & 0x40u) != 0) ? 1u : 0u);
    if (out_memarg->has_memory_index) out_memarg->memory_index = wasm__read_leb128_u32(r);
    out_memarg->offset = wasm__read_leb128_u64(r);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__read_typed_select_immediate(wasm_module_t* mod,
                                                      wasm__reader_t* r,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype) {
    uint32_t count = wasm__read_leb128_u32(r);

    if (count != 1) return WASM_ERR_MALFORMED;
    return wasm__read_valtype(mod, r, out_type, out_reftype);
}
