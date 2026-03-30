#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>

#define BIT_INTERVAL 8000
#define THRESHOLD 520   // ⭐ 你的机器专用

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

// 🔥 多次采样投票（关键）
int read_bit(void *addr) {
    int ones = 0;

    for (int i = 0; i < 5; i++) {
        uint64_t t = reload_time(addr);
        if (t < THRESHOLD) ones++;
    }

    return (ones >= 3) ? 1 : 0;
}

int main() {
    printf("[Receiver] Starting...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *addr = dlsym(handle, "ecvt_r");

    printf("addr = %p\n", addr);

    while (1) {
        uint8_t c = 0;

        for (int i = 0; i < 8; i++) {
            int bit = read_bit(addr);
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