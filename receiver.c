#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>

#define THRESHOLD    506    // 用 ./threshold 测出的值
#define BIT_INTERVAL 10000  // 和sender一样，10ms

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

// Flush+Reload: 先flush，等sender操作，再reload计时
int read_bit(void *addr) {
    // Step 1: flush，清除cache
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();

    // Step 2: 等sender在这个时间槽内操作cache
    usleep(BIT_INTERVAL / 2);  // 等半个时间槽

    // Step 3: reload计时
    _mm_mfence();
    uint64_t start = rdtscp64();
    *(volatile char *)addr;
    uint64_t end = rdtscp64();
    _mm_mfence();

    uint64_t t = end - start;

    // Step 4: 等剩余时间，对齐到完整时间槽
    usleep(BIT_INTERVAL / 2);

    return (t < THRESHOLD) ? 1 : 0;
}

int main(int argc, char *argv[]) {
    int threshold = THRESHOLD;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i+1 < argc)
            threshold = atoi(argv[++i]);
    }

    printf("[Receiver] Starting...\n");
    printf("[Receiver] threshold=%d, BIT_INTERVAL=%d us\n",
           threshold, BIT_INTERVAL);

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) { fprintf(stderr, "dlopen failed\n"); return 1; }
    void *addr = dlsym(handle, "ecvt_r");
    if (!addr)   { fprintf(stderr, "dlsym failed\n");  return 1; }

    printf("[Receiver] addr = %p\n", addr);
    fflush(stdout);

    while (1) {
        uint8_t c = 0;

        for (int i = 7; i >= 0; i--) {
            int bit = read_bit(addr);
            c |= (bit << i);
        }

        if (c == 0) {
            printf(" [END]\n");
            fflush(stdout);
        } else if (c >= 32 && c < 127) {
            printf("%c", c);
            fflush(stdout);
        } else {
            printf("[0x%02x]", c);
            fflush(stdout);
        }
    }

    return 0;
}
