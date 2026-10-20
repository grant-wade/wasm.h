#ifndef WASM_SRC_EXEC_CONT_H
#define WASM_SRC_EXEC_CONT_H

#include "../internal/config.h"
#include "../internal/slot.h"
#include "stack.h"

/* Gives rt the tag its continuation and exception references hold if it has none; returns 0 if none is left. */
WASM_PRIVATE int wasm__cont_claim_tag(wasm_runtime_t* rt);

/* Returns the reference that names k while it is in use. */
WASM_PRIVATE uintptr_t wasm__cont_ref(const wasm_runtime_t* rt, const wasm__cont_object_t* k);

/* Returns the continuation ref names, or NULL with rt's error set if ref is null, not a continuation
 * reference of rt, already consumed, or stale. */
WASM_PRIVATE wasm__cont_object_t* wasm__cont_check(wasm_runtime_t* rt, uintptr_t ref, wasm_error_t* out_err);

/* Returns the continuation ref names if it is in use, else NULL. */
WASM_PRIVATE const wasm__cont_object_t* wasm__cont_live(const wasm_runtime_t* rt, uintptr_t ref);

/* Returns nonzero if ref names a consumed continuation of rt that the host may still pass back. */
WASM_PRIVATE int wasm__cont_consumed(const wasm_runtime_t* rt, uintptr_t ref);

#ifndef NDEBUG
/* Returns nonzero if rt handed out ref. */
WASM_PRIVATE int wasm__cont_issued(const wasm_runtime_t* rt, uintptr_t ref);
#endif

/* Marks the continuation ref names and returns it, or returns NULL if it is consumed or already marked. */
WASM_PRIVATE wasm__cont_object_t* wasm__cont_mark(wasm_runtime_t* rt, uintptr_t ref);

WASM_PRIVATE void wasm__cont_clear_marks(wasm_runtime_t* rt);

/* Every live reference must be marked. */
WASM_PRIVATE void wasm__cont_sweep(wasm_runtime_t* rt);

/* Returns nonzero when entries retired since the last collection call for one, or, if creating is nonzero,
 * when the table is full. */
WASM_PRIVATE int wasm__cont_collection_due(const wasm_runtime_t* rt, int creating);

/* Returns in *out a continuation of type type_index of mod calling function func_idx of func_mod.
 * Every live reference must be reachable from rt's roots. */
WASM_PRIVATE wasm_error_t wasm__cont_new_func(wasm_runtime_t* rt, wasm_module_t* mod, uint32_t type_index,
                                              wasm_module_t* func_mod, uint32_t func_idx,
                                              wasm__cont_object_t** out);

/* Returns the type of the function k calls when it first runs, or NULL once it has stacks. */
WASM_PRIVATE const wasm_functype_t* wasm__cont_start_type(const wasm__cont_object_t* k);

/* Gives k, which has not run, a stack holding its bound arguments. Every live reference must be reachable
 * from rt's roots. */
WASM_PRIVATE wasm_error_t wasm__cont_start(wasm_runtime_t* rt, wasm__cont_object_t* k);

/* Consumes k, binds the slots of args after its earlier arguments, and returns in *out a continuation of
 * type type_index of mod over its stacks or function. */
WASM_PRIVATE wasm_error_t wasm__cont_bind(wasm_runtime_t* rt, wasm__cont_object_t* k, wasm_module_t* mod,
                                          uint32_t type_index, const wasm__slot_t* args, uint32_t slots,
                                          wasm__cont_object_t** out);

/* Starts or continues k, which has stacks, with the slots of args on top of rt->exec, whose top frame's state
 * must be saved; handlers is the resume's handler table. */
WASM_PRIVATE wasm_error_t wasm__cont_resume(wasm_runtime_t* rt, wasm__cont_object_t* k, const wasm__slot_t* args,
                                            uint32_t slots, const uint32_t* handlers);

/* Returns the outermost stack a suspension (kind 0) or switch (kind 1) of tag_identity captures, with
 * *out_handler at the resume's handler entry, or NULL with rt's error set if no resume above barrier
 * handles it. */
WASM_PRIVATE wasm__exec_stack_t* wasm__cont_find_handler(wasm_runtime_t* rt, const wasm__exec_stack_t* barrier,
                                                         uint64_t tag_identity, uint32_t kind,
                                                         const uint32_t** out_handler);

/* Captures the stacks from rt->exec through outer and passes the top payload_slots slots and the
 * continuation to handler on the resumer's stack. */
WASM_PRIVATE wasm_error_t wasm__cont_suspend(wasm_runtime_t* rt, wasm__exec_stack_t* outer, const uint32_t* handler,
                                             uint32_t payload_slots);

/* Runs k under outer's resumer in place of the stacks from rt->exec through outer, passing it the slots slots
 * below k's ref atop rt->exec and those stacks as a continuation of type type_index of mod. k must have stacks
 * unless it calls a host function. Returns WASM__ERR_CALL_FRAME when host function *out_func_idx of *out_mod
 * must run on the resumer's top *out_slots slots. */
WASM_PRIVATE wasm_error_t wasm__cont_switch(wasm_runtime_t* rt, wasm__cont_object_t* k, wasm__exec_stack_t* outer,
                                            wasm_module_t* mod, uint32_t type_index, uint32_t slots,
                                            wasm_module_t** out_mod, uint32_t* out_func_idx, uint32_t* out_slots);

WASM_PRIVATE void wasm__cont_stack_release(wasm_runtime_t* rt, wasm__exec_stack_t* s);

/* Releases the running stacks above base and makes base current. */
WASM_PRIVATE void wasm__cont_unwind(wasm_runtime_t* rt, wasm__exec_stack_t* base);

/* Consumes k, releasing its stacks or bound arguments and its entry. */
WASM_PRIVATE void wasm__cont_drop_stacks(wasm_runtime_t* rt, wasm__cont_object_t* k);

/* Drops the stacks of continuations that run code of mod. */
WASM_PRIVATE void wasm__cont_invalidate_using(wasm_runtime_t* rt, const wasm_module_t* mod);

WASM_PRIVATE void wasm__cont_free_all(wasm_runtime_t* rt);

#endif /* WASM_SRC_EXEC_CONT_H */
