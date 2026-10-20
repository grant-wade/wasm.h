#include "support.h"
#include "internal/alloc.h"
#include <stdlib.h>
#include <string.h>

#define WASM__ALLOC_HEADER WASM__ALLOC_ALIGN

/* Alignment malloc provides on the hosts the runtime targets, for blocks of at least this size. */
#define WASM__LIBC_ALIGN (2u * sizeof(void*))

/* Whether malloc alone aligns a block of size bytes to align. */
static int wasm__libc_aligns(size_t size, size_t align) {
    return align <= sizeof(void*) || (align <= WASM__LIBC_ALIGN && size >= align);
}

static void* wasm__libc_alloc(void* ctx, size_t size, size_t align) {
    uint8_t* raw;
    uintptr_t aligned;

    (void)ctx;
    if (wasm__libc_aligns(size, align)) return malloc(size);
    if (size > SIZE_MAX - align - sizeof(void*)) return NULL;
    raw = (uint8_t*)malloc(size + align - 1u + sizeof(void*));
    if (!raw) return NULL;
    aligned = ((uintptr_t)raw + sizeof(void*) + align - 1u) & ~(uintptr_t)(align - 1u);
    memcpy((uint8_t*)aligned - sizeof(void*), &raw, sizeof(raw));
    return (void*)aligned;
}

static void wasm__libc_free(void* ctx, void* p, size_t size, size_t align) {
    void* raw;

    (void)ctx;
    if (wasm__libc_aligns(size, align)) {
        free(p);
        return;
    }
    memcpy(&raw, (uint8_t*)p - sizeof(void*), sizeof(raw));
    free(raw);
}

static void* wasm__libc_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align) {
    void* moved;

    if (wasm__libc_aligns(old_size, align) && wasm__libc_aligns(new_size, align)) return realloc(p, new_size);
    moved = wasm__libc_alloc(ctx, new_size, align);
    if (!moved) return NULL;
    memcpy(moved, p, old_size < new_size ? old_size : new_size);
    wasm__libc_free(ctx, p, old_size, align);
    return moved;
}

static const wasm_allocator_t wasm__libc_allocator = { NULL, wasm__libc_alloc, wasm__libc_resize, wasm__libc_free };

const wasm_allocator_t* wasm_libc_allocator(void) {
    return &wasm__libc_allocator;
}

WASM_PRIVATE int wasm__allocator_valid(const wasm_allocator_t* a) {
    return a && a->alloc && a->resize && a->free;
}

static size_t wasm__block_size(const void* p) {
    size_t size;

    memcpy(&size, (const uint8_t*)p - WASM__ALLOC_HEADER, sizeof(size));
    return size;
}

WASM_PRIVATE void* wasm__malloc(const wasm_allocator_t* a, size_t size) {
    uint8_t* block;

    if (size > SIZE_MAX - WASM__ALLOC_HEADER) return NULL;
    block = (uint8_t*)a->alloc(a->ctx, size + WASM__ALLOC_HEADER, WASM__ALLOC_ALIGN);
    if (!block) return NULL;
    memcpy(block, &size, sizeof(size));
    return block + WASM__ALLOC_HEADER;
}

WASM_PRIVATE void* wasm__calloc(const wasm_allocator_t* a, size_t count, size_t size) {
    void* p;

    if (size != 0 && count > SIZE_MAX / size) return NULL;
    p = wasm__malloc(a, count * size);
    if (p) memset(p, 0, count * size);
    return p;
}

WASM_PRIVATE void* wasm__realloc(const wasm_allocator_t* a, void* p, size_t size) {
    uint8_t* block;

    if (!p) return wasm__malloc(a, size);
    if (size > SIZE_MAX - WASM__ALLOC_HEADER) return NULL;
    block = (uint8_t*)a->resize(a->ctx, (uint8_t*)p - WASM__ALLOC_HEADER, wasm__block_size(p) + WASM__ALLOC_HEADER,
                                size + WASM__ALLOC_HEADER, WASM__ALLOC_ALIGN);
    if (!block) return NULL;
    memcpy(block, &size, sizeof(size));
    return block + WASM__ALLOC_HEADER;
}

WASM_PRIVATE void* wasm__malloc_array(const wasm_allocator_t* a, size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    return wasm__malloc(a, count * size);
}

WASM_PRIVATE void* wasm__realloc_array(const wasm_allocator_t* a, void* p, size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    return wasm__realloc(a, p, count * size);
}

WASM_PRIVATE void wasm__free(const wasm_allocator_t* a, void* p) {
    if (!p) return;
    a->free(a->ctx, (uint8_t*)p - WASM__ALLOC_HEADER, wasm__block_size(p) + WASM__ALLOC_HEADER, WASM__ALLOC_ALIGN);
}

WASM_PRIVATE void* wasm__alloc_sized(const wasm_allocator_t* a, size_t size, size_t align) {
    return a->alloc(a->ctx, size ? size : 1u, align);
}

WASM_PRIVATE void* wasm__alloc_zeroed_sized(const wasm_allocator_t* a, size_t size, size_t align) {
    void* p = wasm__alloc_sized(a, size, align);

    if (p) memset(p, 0, size);
    return p;
}

WASM_PRIVATE void* wasm__resize_sized(const wasm_allocator_t* a, void* p, size_t old_size, size_t new_size,
                                      size_t align) {
    if (!p) return wasm__alloc_sized(a, new_size, align);
    return a->resize(a->ctx, p, old_size ? old_size : 1u, new_size ? new_size : 1u, align);
}

WASM_PRIVATE void wasm__free_sized(const wasm_allocator_t* a, void* p, size_t size, size_t align) {
    if (p) a->free(a->ctx, p, size ? size : 1u, align);
}

WASM_PRIVATE_INLINE int wasm__uses_funcref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_NOFUNC;
}

WASM_PRIVATE_INLINE int wasm__uses_externref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXTERNREF || type == WASM_TYPE_NOEXTERN;
}

WASM_PRIVATE_INLINE int wasm__uses_exnref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXNREF || type == WASM_TYPE_NOEXN;
}

WASM_PRIVATE_INLINE int wasm__uses_contref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_CONTREF || type == WASM_TYPE_NOCONT;
}

WASM_PRIVATE_INLINE int wasm__uses_gc_ref_storage(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return 1;
        default:
            return 0;
    }
}
