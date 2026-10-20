#include "../support.h"

#if defined(WASM_TEST_THREADS)
/* Imports env.mem as (memory 1 1 shared) and exports
   add(n): n times i32.atomic.rmw.add at address 0,
   wait(): memory.atomic.wait32 at address 8 expecting 0 with no timeout,
   notify(): memory.atomic.notify at address 8 for one waiter. */
static void wasm_test_emit_shared_counter_module(wasm_builder_t* mod) {
    static const uint8_t sections[] = {
        0x01, 0x09, 0x02, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F,
        0x02, 0x0D, 0x01, 0x03, 'e', 'n', 'v', 0x03, 'm', 'e', 'm', 0x02, 0x03, 0x01, 0x01,
        0x03, 0x04, 0x03, 0x00, 0x01, 0x01,
        0x07, 0x17, 0x03,
        0x03, 'a', 'd', 'd', 0x00, 0x00,
        0x04, 'w', 'a', 'i', 't', 0x00, 0x01,
        0x06, 'n', 'o', 't', 'i', 'f', 'y', 0x00, 0x02,
        0x0A, 0x39, 0x03,
        0x1F, 0x00, 0x02, 0x40, 0x03, 0x40, 0x20, 0x00, 0x45, 0x0D, 0x01, 0x41, 0x00, 0x41, 0x01,
        0xFE, 0x1E, 0x02, 0x00, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x0B, 0x0B,
        0x0C, 0x00, 0x41, 0x08, 0x41, 0x00, 0x42, 0x7F, 0xFE, 0x01, 0x02, 0x00, 0x0B,
        0x0A, 0x00, 0x41, 0x08, 0x41, 0x01, 0xFE, 0x00, 0x02, 0x00, 0x0B,
    };

    emit_header(mod);
    emit_bytes(mod, sections, sizeof(sections));
}

typedef struct wasm_test_shared_worker_t {
    wasm_runtime_t rt;
    int initialized;
    wasm_module_t* mod;
    const char* export_name;
    int32_t arg;
    wasm_error_t err;
    int32_t result;
} wasm_test_shared_worker_t;

static void wasm_test_shared_worker_run(void* arg) {
    wasm_test_shared_worker_t* worker = (wasm_test_shared_worker_t*)arg;
    wasm_value_t args[1];
    wasm_value_t results[1];

    args[0] = wasm_i32(worker->arg);
    results[0] = wasm_i32(0);
    if (strcmp(worker->export_name, "add") == 0) {
        worker->err = wasm_call(worker->mod, "add", args, 1, NULL, 0);
    } else {
        worker->err = wasm_call(worker->mod, worker->export_name, NULL, 0, results, 1);
        worker->result = results[0].of.i32;
    }
}

/* Several runtimes are live at once, so they skip the one-runtime leak checks; the case-level check remains. */
static wasm_module_t* wasm_test_load_shared_worker(test_ctx* t, wasm_test_shared_worker_t* worker,
                                                  wasm_memory_t* memory, const wasm_builder_t* mod) {
    wasm_memory_import_t imp;

    imp.module = "env";
    imp.name = "mem";
    imp.memory = memory;
    CHECK_MSG(t, wasm_test_init_raw(&worker->rt, NULL) == WASM_OK, "%s", worker->rt.error_msg);
    worker->initialized = 1;
    CHECK_MSG(t, wasm_register_memory_import(&worker->rt, &imp) == WASM_OK, "%s", worker->rt.error_msg);
    worker->mod = wasm_load(&worker->rt, mod->buf, mod->len);
    CHECK_MSG(t, worker->mod != NULL, "%s", worker->rt.error_msg);
    return worker->mod;
}

TEST(test_threads_shared_memory_across_runtimes) {
    enum { WORKERS = 4, ITERATIONS = 20000 };
    wasm_builder_t mod = { 0 };
    wasm_test_shared_worker_t workers[WORKERS + 1];
    wasm_test_thread_start_t starts[WORKERS + 1];
    wasm_test_thread_t threads[WORKERS + 1];
    wasm_memory_t* memory = wasm_shared_memory_new(&wasm_test_allocator, 1, 1, 0);
    wasm_test_shared_worker_t* waiter = &workers[WORKERS];
    wasm_value_t results[1];
    uint32_t counter;
    int woken = 0;
    int i;

    CHECK_MSG(t, memory != NULL, "%s", "expected shared memory allocation");
    if (!memory) return;
    memset(workers, 0, sizeof(workers));
    wasm_test_emit_shared_counter_module(&mod);
    for (i = 0; i <= WORKERS; i++) {
        if (!wasm_test_load_shared_worker(t, &workers[i], memory, &mod)) goto cleanup;
        workers[i].export_name = i < WORKERS ? "add" : "wait";
        workers[i].arg = ITERATIONS;
        starts[i].fn = wasm_test_shared_worker_run;
        starts[i].arg = &workers[i];
    }

    CHECK(t, wasm_test_thread_spawn(&threads[WORKERS], &starts[WORKERS]));
    for (i = 0; i < WORKERS; i++) CHECK(t, wasm_test_thread_spawn(&threads[i], &starts[i]));
    for (i = 0; i < WORKERS; i++) wasm_test_thread_join(threads[i]);

    /* The waiter may not have blocked yet, so keep notifying until it is woken. */
    while (!woken) {
        CHECK(t, wasm_call(workers[0].mod, "notify", NULL, 0, results, 1) == WASM_OK);
        woken = results[0].of.i32 == 1;
    }
    wasm_test_thread_join(threads[WORKERS]);

    memcpy(&counter, memory->data, sizeof(counter));
    WASM_CHECK_I32(t, (int32_t)counter, WORKERS * ITERATIONS);
    for (i = 0; i < WORKERS; i++) CHECK_MSG(t, workers[i].err == WASM_OK, "worker %d failed", i);
    CHECK_MSG(t, waiter->err == WASM_OK, "%s", waiter->rt.error_msg);
    WASM_CHECK_I32(t, waiter->result, 0);

cleanup:
    for (i = 0; i <= WORKERS; i++) {
        if (workers[i].mod) wasm_module_free(workers[i].mod);
        if (workers[i].initialized) wasm__runtime_destroy(&workers[i].rt);
    }
    wasm_memory_free(memory);
}
#endif

#if defined(WASM_TEST_THREADS)
static const test_case platform_threads_cases[] = {
    { "threads: runtimes on separate threads share atomics and wait/notify", test_threads_shared_memory_across_runtimes },
};
const test_suite platform_threads_suite = { "platform.threads", platform_threads_cases, COUNTOF(platform_threads_cases) };
#else
const test_suite platform_threads_suite = { "platform.threads", NULL, 0 };
#endif
