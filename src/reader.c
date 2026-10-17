#include "reader.h"
#include "values.h"
#include "portable.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>

WASM_PRIVATE int wasm__has(wasm__reader_t* r, size_t n) {
    return !r->malformed && (size_t)(r->end - r->ptr) >= n;
}

WASM_PRIVATE uint8_t wasm__read_u8(wasm__reader_t* r) {
    if (!wasm__has(r, 1)) {
        r->ptr = r->end;
        r->malformed = 1;
        return 0;
    }
    return *r->ptr++;
}

WASM_PRIVATE uint32_t wasm__read_leb128_u32(wasm__reader_t* r) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 5; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 4 && (byte & 0xF0u) != 0) {
            r->ptr = r->end;
            r->malformed = 1;
            return 0;
        }

        result |= (uint32_t)(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return result;
        shift += 7;
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

WASM_PRIVATE uint32_t wasm__read_count(wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);

    if (!r->malformed && count > (size_t)(r->end - r->ptr)) {
        r->ptr = r->end;
        r->malformed = 1;
        return 0;
    }
    return count;
}

WASM_PRIVATE int32_t wasm__read_leb128_i32(wasm__reader_t* r) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 5; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 4) {
            uint8_t payload = (uint8_t)(byte & 0x7Fu);
            uint8_t high_bits = (uint8_t)(payload & 0x70u);
            uint8_t sign_bit = (uint8_t)(payload & 0x08u);

            if ((sign_bit == 0u && high_bits != 0x00u) ||
                (sign_bit != 0u && high_bits != 0x70u)) {
                r->ptr = r->end;
                r->malformed = 1;
                return 0;
            }
        }

        result |= (uint32_t)(byte & 0x7Fu) << shift;
        shift += 7;
        if ((byte & 0x80u) == 0) {
            if (shift < 32 && (byte & 0x40u)) result |= ~0u << shift;
            return (int32_t)result;
        }
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

WASM_PRIVATE uint64_t wasm__read_leb128_u64(wasm__reader_t* r) {
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 10; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 9 && (byte & 0x7Eu) != 0) {
            r->ptr = r->end;
            r->malformed = 1;
            return 0;
        }

        result |= (uint64_t)(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return result;
        shift += 7;
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

WASM_PRIVATE int64_t wasm__read_leb128_i64(wasm__reader_t* r) {
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 10; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 9) {
            uint8_t payload = (uint8_t)(byte & 0x7Fu);

            if (payload != 0x00u && payload != 0x7Fu) {
                r->ptr = r->end;
                r->malformed = 1;
                return 0;
            }
        }

        result |= (uint64_t)(byte & 0x7Fu) << shift;
        shift += 7;
        if ((byte & 0x80u) == 0) {
            if (shift < 64 && (byte & 0x40u)) result |= ~(uint64_t)0 << shift;
            return (int64_t)result;
        }
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

/* Return the value as a struct: floating returns can quiet signaling NaNs
 * through the x87 ABI even when the caller only intends to copy bits. */
WASM_PRIVATE wasm_value_t wasm__read_f32(wasm__reader_t* r) {
    uint32_t bits;
    if (!wasm__has(r, 4)) {
        r->ptr = r->end;
        r->malformed = 1;
        return wasm__f32_bits(0u);
    }
    bits = wasm__load_le32(r->ptr);
    r->ptr += 4;
    return wasm__f32_bits(bits);
}

WASM_PRIVATE wasm_value_t wasm__read_f64(wasm__reader_t* r) {
    uint64_t bits;
    if (!wasm__has(r, 8)) {
        r->ptr = r->end;
        r->malformed = 1;
        return wasm__f64_bits(0u);
    }
    bits = wasm__load_le64(r->ptr);
    r->ptr += 8;
    return wasm__f64_bits(bits);
}

static int wasm__is_valid_utf8(const uint8_t* bytes, uint32_t len) {
    uint32_t i = 0;

    while (i < len) {
        uint8_t lead = bytes[i++];

        if (lead < 0x80u) continue;
        if (lead < 0xC2u) return 0;

        if (lead < 0xE0u) {
            if (i >= len) return 0;
            if ((bytes[i++] & 0xC0u) != 0x80u) return 0;
            continue;
        }

        if (lead < 0xF0u) {
            uint8_t b1;
            uint8_t b2;

            if (i + 1u >= len) return 0;
            b1 = bytes[i++];
            b2 = bytes[i++];
            if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) return 0;
            if (lead == 0xE0u && b1 < 0xA0u) return 0;
            if (lead == 0xEDu && b1 >= 0xA0u) return 0;
            continue;
        }

        if (lead < 0xF5u) {
            uint8_t b1;
            uint8_t b2;
            uint8_t b3;

            if (i + 2u >= len) return 0;
            b1 = bytes[i++];
            b2 = bytes[i++];
            b3 = bytes[i++];
            if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u)
                return 0;
            if (lead == 0xF0u && b1 < 0x90u) return 0;
            if (lead == 0xF4u && b1 >= 0x90u) return 0;
            continue;
        }

        return 0;
    }

    return 1;
}

static int wasm__bytes_equal(const void* lhs, size_t lhs_len, const void* rhs, size_t rhs_len) {
    if (lhs_len != rhs_len) return 0;
    if (lhs_len == 0u) return 1;
    return memcmp(lhs, rhs, lhs_len) == 0;
}

WASM_PRIVATE wasm_error_t wasm__read_name_owned_len(wasm__reader_t* r, char** out_name, uint32_t* out_len) {
    uint32_t len = wasm__read_leb128_u32(r);
    char* name;

    if (!out_name) return WASM_ERR_MALFORMED;
    *out_name = NULL;
    if (out_len) *out_len = 0u;

    if (r->malformed || !wasm__has(r, len)) {
        r->ptr = r->end;
        return WASM_ERR_MALFORMED;
    }

    if (!wasm__is_valid_utf8(r->ptr, len)) {
        r->ptr = r->end;
        return WASM_ERR_MALFORMED;
    }

    name = (char*)WASM_MALLOC((size_t)len + 1u);
    if (!name) return WASM_ERR_OOM;

    if (len > 0) memcpy(name, r->ptr, len);
    name[len] = '\0';
    r->ptr += len;
    *out_name = name;
    if (out_len) *out_len = len;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__read_name_owned(wasm__reader_t* r, char** out_name) {
    return wasm__read_name_owned_len(r, out_name, NULL);
}

WASM_PRIVATE int wasm__export_name_matches_bytes(const wasm_export_t* exp,
                                           const uint8_t* name,
                                           size_t name_len) {
    if (!exp || (!name && name_len != 0u)) return 0;
    return wasm__bytes_equal(exp->name, (size_t)exp->name_len, name, name_len);
}

WASM_PRIVATE int wasm__export_name_matches_cstr(const wasm_export_t* exp, const char* name) {
    if (!exp || !name) return 0;
    return wasm__export_name_matches_bytes(exp, (const uint8_t*)name, strlen(name));
}

WASM_PRIVATE const char* wasm__export_name_cstr(const wasm_export_t* exp) {
    if (!exp || !exp->name) return "";
    return exp->name;
}

WASM_PRIVATE void wasm__free_export(wasm_export_t* exp) {
    if (!exp) return;
    WASM_FREE(exp->name);
    exp->name = NULL;
    exp->name_len = 0u;
}

WASM_PRIVATE char* wasm__strdup(const char* text) {
    size_t len;
    char* copy;

    if (!text) return NULL;
    len = strlen(text);
    copy = (char*)WASM_MALLOC(len + 1u);
    if (!copy) return NULL;
    if (len > 0u) memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}
