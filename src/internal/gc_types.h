#ifndef WASM_INTERNAL_GC_TYPES_H
#define WASM_INTERNAL_GC_TYPES_H

#include "config.h"

/* Shared heap object layouts used by allocation, tracing, and execution. */
#define WASM__GC_I31_TAG ((uintptr_t)1u)

typedef enum wasm_gc_object_kind_t {
    WASM_GC_OBJ_STRUCT = 1,
    WASM_GC_OBJ_ARRAY = 2
} wasm_gc_object_kind_t;

struct wasm_gc_header_t {
    wasm_gc_header_t* next_alloc;
    size_t object_size;
    wasm_module_t* module;
    uint32_t type_index;
    uint8_t kind;
    uint8_t marked;
    uint8_t has_descriptor; /* a struct whose last pointer-sized bytes hold its descriptor */
    uint8_t reserved;
};

typedef struct wasm_gc_struct_t {
    wasm_gc_header_t header;
    uint8_t payload[1];
} wasm_gc_struct_t;

typedef struct wasm_gc_array_t {
    wasm_gc_header_t header;
    uint32_t length;
    uint8_t payload[1];
} wasm_gc_array_t;

#define WASM__GC_STRUCT_HEADER_SIZE offsetof(wasm_gc_struct_t, payload)
#define WASM__GC_ARRAY_HEADER_SIZE offsetof(wasm_gc_array_t, payload)

/* An entry of an exception table. In use, it is a caught exception: the tag and payload slots of the exception
 * an exnref or a legacy catch clause holds. */
struct wasm__exception_object_t {
    uint64_t tag_identity;
    union wasm__slot_t* slots; /* payload, with room for cap_slots slots */
    uint32_t* kinds;           /* WASM__SLOT_KIND_BITS-bit kind of each payload slot, after slots */
    uint32_t tag_index;
    uint32_t num_slots;
    uint32_t cap_slots;
    uint32_t index;     /* its index in its table */
    uint32_t gen;       /* generation of its reference in use or next */
    uint32_t next_free; /* 1 + the next free entry while this one is free, or 0 */
    uint8_t in_use;
    uint8_t held;       /* in the table of exceptions legacy catch clauses hold, which no reference names */
    uint8_t marked;
};

#define WASM__EXN_BLOCK_ENTRIES 256u

/* A runtime's exception objects, in blocks of WASM__EXN_BLOCK_ENTRIES entries that never move. */
typedef struct wasm__exn_table_t {
    struct wasm__exception_object_t** blocks;
    uint32_t num_blocks;
    uint32_t cap_blocks;
    uint32_t count;      /* entries ever used, the first count of the blocks */
    uint32_t free_head;  /* 1 + the first free entry, or 0 */
    uint32_t live;       /* entries in use */
    uint32_t swept_live; /* live when the last collection finished */
    size_t new_bytes;    /* bytes of the entries references were made to since the last collection */
} wasm__exn_table_t;

WASM_INLINE struct wasm__exception_object_t* wasm__exn_at(const wasm__exn_table_t* table, uint32_t index) {
    return &table->blocks[index / WASM__EXN_BLOCK_ENTRIES][index % WASM__EXN_BLOCK_ENTRIES];
}

/* A continuation reference has bit 0 set, the index of its entry in the runtime's continuation table in the next
 * WASM__CONT_INDEX_BITS bits, the entry's generation in the next WASM__CONT_GEN_BITS, and the runtime's tag in
 * the rest. Runtimes alive at once have distinct tags, except that runtimes with tag WASM__CONT_TAG_MAX each own
 * a distinct slice of its indices. An exception reference has the same layout with bit 0 clear, naming an entry
 * of the runtime's exception table. */
#if UINTPTR_MAX > 0xFFFFFFFFu
#define WASM__CONT_INDEX_BITS 27u
#define WASM__CONT_GEN_BITS 24u
#define WASM__CONT_SLICE_BITS 17u
#else
#define WASM__CONT_INDEX_BITS 18u
#define WASM__CONT_GEN_BITS 8u
#define WASM__CONT_SLICE_BITS 10u
#endif
#define WASM__CONT_GEN_SHIFT (1u + WASM__CONT_INDEX_BITS)
#define WASM__CONT_TAG_SHIFT (WASM__CONT_GEN_SHIFT + WASM__CONT_GEN_BITS)
#define WASM__CONT_MAX_ENTRIES ((uint32_t)1u << WASM__CONT_INDEX_BITS)
#define WASM__CONT_GEN_MAX (((uint32_t)1u << WASM__CONT_GEN_BITS) - 1u)
#define WASM__CONT_TAG_MAX ((uint32_t)(UINTPTR_MAX >> WASM__CONT_TAG_SHIFT))
#define WASM__CONT_SLICE_ENTRIES ((uint32_t)1u << WASM__CONT_SLICE_BITS)
#define WASM__CONT_SLICES ((uint32_t)1u << (WASM__CONT_INDEX_BITS - WASM__CONT_SLICE_BITS))
#define WASM__CONT_BLOCK_ENTRIES 256u

/* An entry of the continuation table. In use, it is a continuation: the stacks from top through their parent
 * links to bottom, or a function and its bound arguments until it first runs. */
struct wasm__cont_object_t {
    struct wasm__exec_stack_t* top;    /* innermost stack, where execution resumes */
    struct wasm__exec_stack_t* bottom; /* outermost stack, linked under the resumer */
    wasm_module_t* start_module;       /* owner of start_func until the continuation first runs */
    union wasm__slot_t* args;          /* slots of the arguments bound before it first runs */
    wasm_module_t* module;             /* owner of type_index, or NULL if the type is unknown */
    uint32_t start_func;
    uint32_t num_args;
    uint32_t type_index;
    uint32_t index;        /* the index its references hold */
    uint32_t gen;          /* generation of its reference in use or next; above WASM__CONT_GEN_MAX once retired */
    uint32_t swept_gen;    /* gen when the last collection finished */
    uint32_t stale_low;    /* lowest generation of a consumed reference the last collection marked */
    uint32_t stale_high;   /* highest such generation, or below stale_low if it marked none */
    uint32_t marking_low;  /* stale_low of the collection marking now */
    uint32_t marking_high; /* stale_high of the collection marking now */
    uint32_t next_free;    /* 1 + the next free entry while this one is free, or 0 */
    uint8_t in_use;
    uint8_t stale; /* nonzero if its stacks were dropped when a module they run was freed */
    uint8_t marked;
};

/* A runtime's continuations, in blocks of WASM__CONT_BLOCK_ENTRIES entries that never move. */
typedef struct wasm__cont_table_t {
    struct wasm__cont_object_t** blocks;
    uint32_t num_blocks;
    uint32_t cap_blocks;
    uint32_t count;         /* entries ever used, the first count of the blocks */
    uint32_t limit;         /* most entries it may use, or 0 until it has a tag */
    uint32_t base;          /* index the references of its first entry hold */
    uint32_t free_head;     /* 1 + the first free entry, or 0 */
    uint32_t live;          /* entries in use */
    uint32_t retired;       /* entries out of generations */
    uint32_t retired_swept; /* retired when the last collection finished */
    uintptr_t tag;          /* nonzero once the first entry is used */
} wasm__cont_table_t;

WASM_INLINE struct wasm__cont_object_t* wasm__cont_at(const wasm__cont_table_t* table, uint32_t index) {
    return &table->blocks[index / WASM__CONT_BLOCK_ENTRIES][index % WASM__CONT_BLOCK_ENTRIES];
}

/* An externref wrapping an anyref, or an anyref wrapping a host externref; value is what it wraps. */
struct wasm__externref_box_t {
    wasm_value_t value;
    int marked;
};

#endif /* WASM_INTERNAL_GC_TYPES_H */
