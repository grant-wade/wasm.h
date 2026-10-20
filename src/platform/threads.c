#include "threads.h"
#include "host.h"
#include "../atomics.h"
#include <stdlib.h>
#include <string.h>

#if WASM_ENABLE_PLATFORM && WASM__PLATFORM_POSIX
#include <errno.h>
#include <pthread.h>
#include <time.h>
#define WASM__THREADS_POSIX 1
#elif WASM_ENABLE_PLATFORM && WASM__PLATFORM_WINDOWS
#define WASM__THREADS_WINDOWS 1
#endif

#if defined(WASM__THREADS_POSIX) || defined(WASM__THREADS_WINDOWS)
/* One node per blocked thread, linked in arrival order so notify wakes the oldest waiters first. */
typedef struct wasm__waiter_t {
    const uint8_t* address;
    const size_t* interrupt;
    int notified;
#if defined(WASM__THREADS_POSIX)
    pthread_cond_t cond;
#else
    CONDITION_VARIABLE cond;
#endif
    struct wasm__waiter_t* next;
} wasm__waiter_t;

static wasm__waiter_t* wasm__waiters_head;
static wasm__waiter_t* wasm__waiters_tail;

#if defined(WASM__THREADS_POSIX)
static pthread_mutex_t wasm__waiters_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wasm__grow_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wasm__cont_tags_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
static SRWLOCK wasm__waiters_lock = SRWLOCK_INIT;
static SRWLOCK wasm__grow_lock = SRWLOCK_INIT;
static SRWLOCK wasm__cont_tags_mutex = SRWLOCK_INIT;
#endif

static void wasm__waiters_remove(wasm__waiter_t* waiter) {
    wasm__waiter_t* prev = NULL;
    wasm__waiter_t* node = wasm__waiters_head;

    while (node && node != waiter) {
        prev = node;
        node = node->next;
    }
    if (!node) return;
    if (prev)
        prev->next = node->next;
    else
        wasm__waiters_head = node->next;
    if (wasm__waiters_tail == node) wasm__waiters_tail = prev;
    node->next = NULL;
}

/* Timeouts beyond about 146 years are treated as infinite so deadline arithmetic cannot overflow. */
#define WASM__WAIT_FOREVER_NS (INT64_C(1) << 62)

WASM_PRIVATE int wasm__platform_atomic_wait(uint8_t* p, uint32_t size, uint64_t expected, int64_t timeout_ns,
                                            const size_t* interrupt) {
    wasm__waiter_t waiter;
    int forever = timeout_ns < 0 || timeout_ns >= WASM__WAIT_FOREVER_NS;
    int result = 0;
#if defined(WASM__THREADS_POSIX)
    struct timespec deadline;

    if (!forever) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += (time_t)(timeout_ns / 1000000000);
        deadline.tv_nsec += (long)(timeout_ns % 1000000000);
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }
    pthread_mutex_lock(&wasm__waiters_lock);
#else
    ULONGLONG start = GetTickCount64();
    ULONGLONG timeout_ms = forever ? 0 : (ULONGLONG)((timeout_ns + 999999) / 1000000);

    AcquireSRWLockExclusive(&wasm__waiters_lock);
#endif

    if (wasm__atomic_load(p, size) != expected) {
        result = 1;
    } else if (interrupt && wasm__atomic_load_size(interrupt) != 0u) {
        result = 3;
    } else if (!forever && timeout_ns == 0) {
        result = 2;
    } else {
        memset(&waiter, 0, sizeof(waiter));
        waiter.address = p;
        waiter.interrupt = interrupt;
#if defined(WASM__THREADS_POSIX)
        pthread_cond_init(&waiter.cond, NULL);
#else
        InitializeConditionVariable(&waiter.cond);
#endif
        if (wasm__waiters_tail)
            wasm__waiters_tail->next = &waiter;
        else
            wasm__waiters_head = &waiter;
        wasm__waiters_tail = &waiter;

        while (!waiter.notified) {
            if (interrupt && wasm__atomic_load_size(interrupt) != 0u) {
                result = 3;
                break;
            }
#if defined(WASM__THREADS_POSIX)
            if (forever) {
                pthread_cond_wait(&waiter.cond, &wasm__waiters_lock);
            } else if (pthread_cond_timedwait(&waiter.cond, &wasm__waiters_lock, &deadline) == ETIMEDOUT &&
                       !waiter.notified) {
                result = 2;
                break;
            }
#else
            DWORD wait_ms = INFINITE;
            if (!forever) {
                ULONGLONG elapsed = GetTickCount64() - start;
                if (elapsed >= timeout_ms) {
                    result = 2;
                    break;
                }
                wait_ms = (DWORD)((timeout_ms - elapsed) > 0xFFFFFFFEu ? 0xFFFFFFFEu : (timeout_ms - elapsed));
            }
            SleepConditionVariableSRW(&waiter.cond, &wasm__waiters_lock, wait_ms, 0);
#endif
        }
        if (!waiter.notified) wasm__waiters_remove(&waiter);
#if defined(WASM__THREADS_POSIX)
        pthread_cond_destroy(&waiter.cond);
#endif
    }

#if defined(WASM__THREADS_POSIX)
    pthread_mutex_unlock(&wasm__waiters_lock);
#else
    ReleaseSRWLockExclusive(&wasm__waiters_lock);
#endif
    return result;
}

WASM_PRIVATE uint32_t wasm__platform_atomic_notify(uint8_t* p, uint32_t count) {
    wasm__waiter_t* node;
    uint32_t woken = 0;

#if defined(WASM__THREADS_POSIX)
    pthread_mutex_lock(&wasm__waiters_lock);
#else
    AcquireSRWLockExclusive(&wasm__waiters_lock);
#endif
    node = wasm__waiters_head;
    while (node && woken < count) {
        wasm__waiter_t* next = node->next;
        if (node->address == p) {
            wasm__waiters_remove(node);
            node->notified = 1;
#if defined(WASM__THREADS_POSIX)
            pthread_cond_signal(&node->cond);
#else
            WakeConditionVariable(&node->cond);
#endif
            woken++;
        }
        node = next;
    }
#if defined(WASM__THREADS_POSIX)
    pthread_mutex_unlock(&wasm__waiters_lock);
#else
    ReleaseSRWLockExclusive(&wasm__waiters_lock);
#endif
    return woken;
}

WASM_PRIVATE void wasm__platform_atomic_wake_all(void) {
    wasm__waiter_t* node;

#if defined(WASM__THREADS_POSIX)
    pthread_mutex_lock(&wasm__waiters_lock);
#else
    AcquireSRWLockExclusive(&wasm__waiters_lock);
#endif
    for (node = wasm__waiters_head; node; node = node->next) {
#if defined(WASM__THREADS_POSIX)
        pthread_cond_signal(&node->cond);
#else
        WakeConditionVariable(&node->cond);
#endif
    }
#if defined(WASM__THREADS_POSIX)
    pthread_mutex_unlock(&wasm__waiters_lock);
#else
    ReleaseSRWLockExclusive(&wasm__waiters_lock);
#endif
}

WASM_PRIVATE void wasm__platform_shared_grow_lock(void) {
#if defined(WASM__THREADS_POSIX)
    pthread_mutex_lock(&wasm__grow_lock);
#else
    AcquireSRWLockExclusive(&wasm__grow_lock);
#endif
}

WASM_PRIVATE void wasm__platform_shared_grow_unlock(void) {
#if defined(WASM__THREADS_POSIX)
    pthread_mutex_unlock(&wasm__grow_lock);
#else
    ReleaseSRWLockExclusive(&wasm__grow_lock);
#endif
}

WASM_PRIVATE void wasm__platform_cont_tags_lock(void) {
#if defined(WASM__THREADS_POSIX)
    pthread_mutex_lock(&wasm__cont_tags_mutex);
#else
    AcquireSRWLockExclusive(&wasm__cont_tags_mutex);
#endif
}

WASM_PRIVATE void wasm__platform_cont_tags_unlock(void) {
#if defined(WASM__THREADS_POSIX)
    pthread_mutex_unlock(&wasm__cont_tags_mutex);
#else
    ReleaseSRWLockExclusive(&wasm__cont_tags_mutex);
#endif
}

#else

WASM_PRIVATE int wasm__platform_atomic_wait(uint8_t* p, uint32_t size, uint64_t expected, int64_t timeout_ns,
                                            const size_t* interrupt) {
    if (wasm__atomic_load(p, size) != expected) return 1;
    if (interrupt && wasm__atomic_load_size(interrupt) != 0u) return 3;
    return timeout_ns == 0 ? 2 : -1;
}

WASM_PRIVATE void wasm__platform_atomic_wake_all(void) {
}

WASM_PRIVATE uint32_t wasm__platform_atomic_notify(uint8_t* p, uint32_t count) {
    (void)p;
    (void)count;
    return 0;
}

WASM_PRIVATE void wasm__platform_shared_grow_lock(void) {
}

WASM_PRIVATE void wasm__platform_shared_grow_unlock(void) {
}

WASM_PRIVATE void wasm__platform_cont_tags_lock(void) {
}

WASM_PRIVATE void wasm__platform_cont_tags_unlock(void) {
}

#endif

/* Reservations halve from max_bytes until one succeeds, since growth may fail before the declared maximum.
   Large calloc requests are lazily zero-filled mappings on POSIX hosts; Windows reserves address space and
   commits it as memory grows. */
WASM_PRIVATE uint8_t* wasm__platform_shared_reserve(uint64_t min_bytes, uint64_t max_bytes, uint64_t* out_bytes) {
    uint64_t bytes = max_bytes;

    if (bytes > (uint64_t)SIZE_MAX) bytes = (uint64_t)SIZE_MAX;
    for (;;) {
        void* base = NULL;
        uint64_t request = bytes ? bytes : 1u;
#if defined(WASM__THREADS_WINDOWS)
        base = VirtualAlloc(NULL, (SIZE_T)request, MEM_RESERVE, PAGE_NOACCESS);
#else
        base = calloc(1u, (size_t)request);
#endif
        if (base) {
            *out_bytes = bytes;
            return (uint8_t*)base;
        }
        if (bytes / 2u < min_bytes || bytes == 0) return NULL;
        bytes /= 2u;
    }
}

WASM_PRIVATE int wasm__platform_shared_commit(uint8_t* base, uint64_t bytes) {
#if defined(WASM__THREADS_WINDOWS)
    if (bytes == 0) return 1;
    return VirtualAlloc(base, (SIZE_T)bytes, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
    (void)base;
    (void)bytes;
    return 1;
#endif
}

WASM_PRIVATE void wasm__platform_shared_release(uint8_t* base, uint64_t reserved_bytes) {
    if (!base) return;
#if defined(WASM__THREADS_WINDOWS)
    (void)reserved_bytes;
    VirtualFree(base, 0, MEM_RELEASE);
#else
    (void)reserved_bytes;
    free(base);
#endif
}
