#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_INTERVAL 10000  // 10ms per bit

void send_bit(void *addr, int bit) {
    double pi = 3.14;
    int decpt, sign;
    char buf[64];

    // 每次先flush，保证从干净状态开始
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();

    if (bit == 1) {
        // 发1：调用函数，把它放进cache
        for (int i = 0; i < 20; i++) {
            ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
        }
    }
    // 发0：保持flush状态，不做任何事

    _mm_mfence();
    usleep(BIT_INTERVAL);  // 等待receiver采样
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (b >> i) & 1);
    }
}

int main() {
    printf("[Sender] Starting...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) { fprintf(stderr, "dlopen failed\n"); return 1; }
    void *addr = dlsym(handle, "ecvt_r");
    if (!addr)   { fprintf(stderr, "dlsym failed\n");  return 1; }

    printf("[Sender] addr = %p\n", addr);
    printf("[Sender] BIT_INTERVAL = %d us\n", BIT_INTERVAL);
    fflush(stdout);

    char *msg = "HELLO";

    while (1) {
        printf("[Sender] Sending: %s\n", msg);
        fflush(stdout);

        for (int i = 0; i < (int)strlen(msg); i++) {
            send_byte(addr, msg[i]);
        }
        send_byte(addr, 0);  // 结束符
        usleep(50000);       // 两轮之间停50ms
    }

    return 0;
}
