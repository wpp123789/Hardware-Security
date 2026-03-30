#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>

#define BIT_INTERVAL 5000
#define THRESHOLD 1500   // ⚠️ 用 threshold.c 测出来替换

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

uint64_t reload_time(void *addr) {
    _mm_mfence();

    uint64_t start = rdtscp64();
    *(volatile char *)addr;
    uint64_t end = rdtscp64();

    return end - start;
}

int main() {
    printf("[Receiver] Starting...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *addr = dlsym(handle, "ecvt_r");

    printf("addr = %p\n", addr);

    while (1) {
        uint8_t c = 0;

        for (int i = 0; i < 8; i++) {
            uint64_t t = reload_time(addr);
            int bit = (t < THRESHOLD) ? 1 : 0;

            c = (c << 1) | bit;

            usleep(BIT_INTERVAL);
        }

        if (c == 0) {
            printf("\n");
        } else {
            printf("%c", c);
            fflush(stdout);
        }
    }

    return 0;
}