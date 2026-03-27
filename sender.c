#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>
#include <time.h>

// 每个bit占用的时间槽：1ms
#define BIT_SLOT_NS 1000000  // 1,000,000 ns = 1 ms

static void sleep_ns(long ns) {
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

int main(void) {
    printf("[Sender] Starting up...\n");
    fflush(stdout);

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "[Sender] dlopen failed\n");
        return 1;
    }
    void *libc_fn = dlsym(handle, "ecvt_r");
    if (!libc_fn) {
        fprintf(stderr, "[Sender] dlsym failed\n");
        return 1;
    }
    printf("[Sender] libc address = %p\n", libc_fn);
    fflush(stdout);
    dlclose(handle);

    const char *msg = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t msg_len = strlen(msg);
    size_t num_bits = msg_len * 8;  // 208 bits
    unsigned long bit_index = 0;

    double pi = 3.141592653589793;
    int decpt = 0, sign = 0;
    char buf[64];

    printf("[Sender] Sending: %s (%zu bits, slot=%dms)\n",
           msg, num_bits, BIT_SLOT_NS / 1000000);
    fflush(stdout);

    while (1) {
        size_t char_index = bit_index / 8;
        int bit_pos = bit_index % 8;
        int bit_to_send = (msg[char_index] >> bit_pos) & 1;

        // Step 1: 先flush所有相关cache line
        _mm_clflush((char*) libc_fn);
        _mm_clflush((char*) libc_fn + 64);
        _mm_clflush((char*) libc_fn + 128);
        _mm_clflush((char*) libc_fn + 192);
        _mm_clflush((char*) libc_fn + 256);
        _mm_clflush((char*) libc_fn + 320);
        _mm_clflush((char*) libc_fn + 384);
        _mm_clflush((char*) libc_fn + 448);
        _mm_mfence();

        // Step 2: 发1则调用函数放回cache，发0则保持flush状态
        if (bit_to_send == 1) {
            int s = ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
            (void)s;
        }

        // Step 3: 等待一个时间槽
        sleep_ns(BIT_SLOT_NS);

        bit_index = (bit_index + 1) % num_bits;

        if (bit_index == 0) {
            printf("[Sender] One round done, repeating...\n");
            fflush(stdout);
        }
    }

    return 0;
}
