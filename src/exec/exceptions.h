#ifndef WASM_SRC_EXEC_EXCEPTIONS_H
#define WASM_SRC_EXEC_EXCEPTIONS_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt);

/* Takes branch descriptor desc from cf, the top frame of its runtime's current stack; a branch that
 * returns leaves cf's pc NULL. */
WASM_PRIVATE wasm_error_t wasm__branch_frame(wasm__call_frame_t* cf, const uint32_t* desc);

/* Returns WASM_OK with cf at the handler, or clears its labels and returns WASM__ERR_EXCEPTION. */
WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm__call_frame_t* cf);

/* Makes an exception of tag tag_index of mod, whose payload is the slots just below top, rt's pending exception.
 * Returns in *out_slots how many slots the payload spans. */
WASM_PRIVATE wasm_error_t wasm__exn_throw_slots(wasm_runtime_t* rt, wasm_module_t* mod, uint32_t tag_index,
                                                const wasm__slot_t* top, uint32_t* out_slots);

/* Makes the exception ref names rt's pending exception; returns 0 if ref is null or names no exception of rt. */
WASM_PRIVATE int wasm__exn_throw_ref(wasm_runtime_t* rt, uintptr_t ref);

/* Makes exception, which a legacy catch clause holds, rt's pending exception. */
WASM_PRIVATE wasm_error_t wasm__exn_rethrow(wasm_runtime_t* rt, wasm__exception_object_t* exception);

WASM_PRIVATE void wasm__exn_clear_pending(wasm_runtime_t* rt);

/* exception is one a legacy catch clause held. */
WASM_PRIVATE void wasm__exn_release(wasm_runtime_t* rt, wasm__exception_object_t* exception);

/* Returns the exception ref names if it is one of rt's, else NULL. */
WASM_PRIVATE const wasm__exception_object_t* wasm__exn_live(const wasm_runtime_t* rt, uintptr_t ref);

#ifndef NDEBUG
/* Returns nonzero if ref has the form of a reference rt hands out to its exceptions. */
WASM_PRIVATE int wasm__exn_issued(const wasm_runtime_t* rt, uintptr_t ref);
#endif

/* Marks the exception ref names and returns it, or returns NULL if it names none or is already marked. */
WASM_PRIVATE wasm__exception_object_t* wasm__exn_mark(wasm_runtime_t* rt, uintptr_t ref);

WASM_PRIVATE void wasm__exn_clear_marks(wasm_runtime_t* rt);

/* Every live reference must be marked. */
WASM_PRIVATE void wasm__exn_sweep(wasm_runtime_t* rt);

/* Returns nonzero when the exceptions references were made to since the last collection call for one. */
WASM_PRIVATE int wasm__exn_collection_due(const wasm_runtime_t* rt);

WASM_PRIVATE void wasm__exn_free_all(wasm_runtime_t* rt);

#endif /* WASM_SRC_EXEC_EXCEPTIONS_H */
