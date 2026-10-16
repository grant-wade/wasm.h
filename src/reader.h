#ifndef WASM_READER_H
#define WASM_READER_H

#include "internal/config.h"

typedef struct wasm__reader_t {
    const uint8_t* ptr;
    const uint8_t* end;
    int malformed;
} wasm__reader_t;

WASM_PRIVATE int wasm__has(wasm__reader_t* r, size_t n);

WASM_PRIVATE uint8_t wasm__read_u8(wasm__reader_t* r);

WASM_PRIVATE uint32_t wasm__read_leb128_u32(wasm__reader_t* r);

WASM_PRIVATE int32_t wasm__read_leb128_i32(wasm__reader_t* r);

WASM_PRIVATE uint64_t wasm__read_leb128_u64(wasm__reader_t* r);

WASM_PRIVATE int64_t wasm__read_leb128_i64(wasm__reader_t* r);

WASM_PRIVATE wasm_value_t wasm__read_f32(wasm__reader_t* r);

WASM_PRIVATE wasm_value_t wasm__read_f64(wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__read_name_owned_len(wasm__reader_t* r, char** out_name, uint32_t* out_len);

WASM_PRIVATE wasm_error_t wasm__read_name_owned(wasm__reader_t* r, char** out_name);

WASM_PRIVATE int wasm__export_name_matches_bytes(const wasm_export_t* exp,
                                           const uint8_t* name,
                                           size_t name_len);

WASM_PRIVATE int wasm__export_name_matches_cstr(const wasm_export_t* exp, const char* name);

WASM_PRIVATE const char* wasm__export_name_cstr(const wasm_export_t* exp);

WASM_PRIVATE void wasm__free_export(wasm_export_t* exp);

WASM_PRIVATE char* wasm__strdup(const char* text);

#endif /* WASM_READER_H */
