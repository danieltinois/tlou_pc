/* macOS guest heap (src/runtime_memory.c runtime_guest_calloc): host objects whose address the
 * guest keeps must sit below 1 TiB, 64-byte aligned and zeroed, also when a freed block is
 * reused and under concurrent use. Linked with the whole runtime (gpu/CMakeLists.txt). */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

void runtime_restart(void) { abort(); }

static int fails;
#define CHECK(c, ...) do { if (!(c)) { __atomic_add_fetch(&fails, 1, __ATOMIC_RELAXED); \
    printf("FAIL: " __VA_ARGS__); puts(""); } } while (0)
static int low_aligned(const void *p) { return p && (uintptr_t)p < (UINT64_C(1) << 40) && !((uintptr_t)p & 63); }
static int zeroed(const unsigned char *p, size_t n) {
    for (size_t i = 0; i < n; ++i) if (p[i]) return 0;
    return 1;
}
static void *churn(void *argument) {
    (void)argument;
    void *keep[64];
    for (int round = 0; round < 2000; ++round) {
        for (int i = 0; i < 64; ++i) {
            const size_t n = (size_t)(i * 37 + round) % 3000 + 1;
            keep[i] = runtime_guest_calloc(1, n);
            CHECK(low_aligned(keep[i]) && zeroed(keep[i], n), "thread block of %zu bytes at %p", n, keep[i]);
            if (keep[i]) memset(keep[i], 0xab, n);
        }
        for (int i = 0; i < 64; ++i) runtime_guest_free(keep[i]);
    }
    return NULL;
}
int main(void) {
    static const size_t sizes[] = {1, 8, 48, 64, 100, 1000, 4032, 4033, 65536, 1 << 20};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i) {
        unsigned char *p = runtime_guest_calloc(1, sizes[i]);
        CHECK(low_aligned(p), "%zu bytes at %p", sizes[i], (void *)p);
        CHECK(p && zeroed(p, sizes[i]), "%zu bytes not zeroed", sizes[i]);
        if (p) memset(p, 0xcd, sizes[i]);
        runtime_guest_free(p);
        unsigned char *q = runtime_guest_calloc(1, sizes[i]);
        CHECK(q && zeroed(q, sizes[i]), "%zu bytes: reused block not zeroed", sizes[i]);
        if (sizes[i] <= 4032) CHECK(q == p, "%zu bytes: freed block not reused", sizes[i]);
        runtime_guest_free(q);
    }
    CHECK(!runtime_guest_calloc(SIZE_MAX / 2, 4), "overflowing count * size accepted");
    pthread_t threads[8];
    for (int i = 0; i < 8; ++i) pthread_create(&threads[i], NULL, churn, NULL);
    for (int i = 0; i < 8; ++i) pthread_join(threads[i], NULL);
    puts(fails ? "guest-heap-test: FAILED" : "guest-heap-test: ok");
    return fails != 0;
}
