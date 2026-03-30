#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>

// 每个bit的时间窗口，单位微秒
// 调大一点更稳定
#define BIT_US 50000   // 50ms per bit

static inline void flush_addr(void *addr) {
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();
}

static inline void load_addr(void *addr) {
    double pi = 3.14;
    int decpt, sign;
    char buf[64];
    // 多次调用，强化cache信号
    for (int i = 0; i < 50; i++) {
        ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
    }
    _mm_mfence();
}

// 发送一个bit
// bit=1: 先flush，再load进cache
// bit=0: 只flush，不load
void send_bit(void *addr, int bit) {
    flush_addr(addr);
    if (bit == 1) {
        load_addr(addr);
    }
    usleep(BIT_US);
}

// 发送一个字节，高位先发
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
    dlclose(handle);

    printf("[Sender] addr = %p\n", addr);
    printf("[Sender] BIT_US = %d us\n", BIT_US);
    fflush(stdout);

    // 同步前导码：10101010 10101010（让receiver找到边界）
    // 然后发送消息
    char *msg = "HELLO";

    while (1) {
        printf("[Sender] Sending preamble + message: %s\n", msg);
        fflush(stdout);

        // 发送同步前导码：2字节 0xAA = 10101010
        send_byte(addr, 0xAA);
        send_byte(addr, 0xAA);

        // 发送消息
        for (int i = 0; i < (int)strlen(msg); i++) {
            send_byte(addr, (uint8_t)msg[i]);
        }

        // 发送结束符
        send_byte(addr, 0x00);

        // 两轮之间的间隔
        usleep(200000);  // 200ms
    }

    return 0;
}
