#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_INTERVAL 8000   // 更稳

void send_bit(void *addr, int bit) {
    double pi = 3.14;
    int decpt, sign;
    char buf[64];

    if (bit == 1) {
        // 🔥 强化信号
        for (int i = 0; i < 10; i++) {
            ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
        }
    } else {
        // 🔥 强化 flush
        for (int i = 0; i < 5; i++) {
            _mm_clflush(addr);
        }
    }

    _mm_mfence();
    usleep(BIT_INTERVAL);
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (b >> i) & 1);
    }
}

int main() {
    printf("[Sender] Starting...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *addr = dlsym(handle, "ecvt_r");

    printf("addr = %p\n", addr);

    char *msg = "Hello_HW3";

    while (1) {
        for (int i = 0; i < strlen(msg); i++) {
            send_byte(addr, msg[i]);
        }

        send_byte(addr, 0); // 结束符
        usleep(20000);
    }

    return 0;
}