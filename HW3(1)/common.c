#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>

int resolve_target(const char *lib_path, const char *symbol, target_t *out) {
    void *handle;
    void *sym;

    if (!lib_path || !symbol || !out) {
        return -1;
    }

    handle = dlopen(lib_path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen failed for %s: %s\n", lib_path, dlerror());
        return -1;
    }

    dlerror();
    sym = dlsym(handle, symbol);
    if (!sym) {
        fprintf(stderr, "dlsym failed for %s: %s\n", symbol, dlerror());
        dlclose(handle);
        return -1;
    }

    out->handle = handle;
    out->addr = (volatile uint8_t *)sym;
    return 0;
}

void close_target(target_t *target) {
    if (!target) {
        return;
    }
    if (target->handle) {
        dlclose(target->handle);
        target->handle = NULL;
    }
    target->addr = NULL;
}

uint32_t reload_time_cycles(volatile uint8_t *addr) {
    unsigned aux = 0;
    uint64_t start;
    uint64_t end;

    _mm_mfence();
    start = __rdtscp(&aux);
    (void)*addr;
    _mm_lfence();
    end = __rdtscp(&aux);
    return (uint32_t)(end - start);
}

void flush_addr(volatile void *addr) {
    _mm_clflush((const void *)addr);
}

void touch_addr(volatile uint8_t *addr) {
    (void)*addr;
}

uint64_t now_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

void sleep_until_us(uint64_t deadline_us) {
    while (now_us() < deadline_us) {
        _mm_pause();
    }
}

uint8_t xor_checksum(const uint8_t *data, size_t len) {
    size_t i;
    uint8_t c = 0;
    for (i = 0; i < len; ++i) {
        c ^= data[i];
    }
    return c;
}

int parse_u32(const char *s, uint32_t *out) {
    unsigned long v;
    char *end = NULL;

    if (!s || !out) {
        return -1;
    }
    errno = 0;
    v = strtoul(s, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        return -1;
    }
    if (v > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

int parse_u64(const char *s, uint64_t *out) {
    unsigned long long v;
    char *end = NULL;

    if (!s || !out) {
        return -1;
    }
    errno = 0;
    v = strtoull(s, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

int hamming_u32(uint32_t a, uint32_t b) {
    uint32_t x = a ^ b;
    int d = 0;
    while (x) {
        d += (int)(x & 1u);
        x >>= 1u;
    }
    return d;
}
