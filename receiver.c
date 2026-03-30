#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>

#define THRESHOLD 506   // ./threshold 测出的阈值
#define BIT_US    50000 // 和sender一样，50ms per bit

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

// 采样一个bit：
// 1. 先flush（清除cache）
// 2. 等sender在这个窗口内操作
// 3. reload计时
int sample_bit(void *addr, int threshold) {
    // Step1: flush，让sender决定放不放回来
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();

    // Step2: 等sender操作（等窗口的前3/4时间）
    usleep(BIT_US * 3 / 4);

    // Step3: reload计时
    _mm_mfence();
    uint64_t t0 = rdtscp64();
    *(volatile char *)addr;
    uint64_t t1 = rdtscp64();
    _mm_mfence();

    uint64_t t = t1 - t0;

    // Step4: 等剩余时间对齐到下一个bit边界
    usleep(BIT_US / 4);

    return (t < threshold) ? 1 : 0;
}

// 接收一个字节（高位先收）
uint8_t recv_byte(void *addr, int threshold) {
    uint8_t b = 0;
    for (int i = 7; i >= 0; i--) {
        int bit = sample_bit(addr, threshold);
        b |= (bit << i);
    }
    return b;
}

int main(int argc, char *argv[]) {
    int threshold = THRESHOLD;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i+1 < argc)
            threshold = atoi(argv[++i]);
    }

    printf("[Receiver] Starting...\n");
    printf("[Receiver] threshold=%d, BIT_US=%d us\n", threshold, BIT_US);

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) { fprintf(stderr, "dlopen failed\n"); return 1; }
    void *addr = dlsym(handle, "ecvt_r");
    if (!addr)   { fprintf(stderr, "dlsym failed\n");  return 1; }
    dlclose(handle);

    printf("[Receiver] addr = %p\n", addr);
    printf("[Receiver] Waiting for preamble 0xAA 0xAA...\n");
    fflush(stdout);

    while (1) {
        // Step1: 找同步前导码 0xAA 0xAA
        uint8_t b1 = recv_byte(addr, threshold);
        if (b1 != 0xAA) {
            printf("[Receiver] sync byte1 = 0x%02x (expected 0xAA)\n", b1);
            fflush(stdout);
            continue;
        }
        uint8_t b2 = recv_byte(addr, threshold);
        if (b2 != 0xAA) {
            printf("[Receiver] sync byte2 = 0x%02x (expected 0xAA)\n", b2);
            fflush(stdout);
            continue;
        }

        // Step2: 收到同步头，开始接收消息
        printf("[Receiver] Synced! Receiving message: ");
        fflush(stdout);

        while (1) {
            uint8_t c = recv_byte(addr, threshold);
            if (c == 0x00) {
                printf(" [END]\n");
                fflush(stdout);
                break;
            } else if (c >= 32 && c < 127) {
                printf("%c", c);
                fflush(stdout);
            } else {
                printf("[0x%02x]", c);
                fflush(stdout);
            }
        }
    }

    return 0;
}
