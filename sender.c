#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>

int main(void) {
    printf("[Sender] Starting up...\n");
    fflush(stdout);

    // Load the real address from libc
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
    size_t num_bits = msg_len << 3;
    unsigned long bit_index = 0;

    double pi = 3.141592653589793;
    int decpt = 0, sign = 0;
    char buf[64];

    while (1) {
        size_t char_index = bit_index >> 3;
        int bit_pos = bit_index & 7;
        int bit_to_send = (msg[char_index] >> bit_pos) & 1;

        // Flush the cache line and neighborhood
        _mm_clflush((char*) libc_fn);
        _mm_clflush((char*) libc_fn + 64);
        _mm_clflush((char*) libc_fn + 128);
        _mm_clflush((char*) libc_fn + 192);
        _mm_clflush((char*) libc_fn + 256);
        _mm_clflush((char*) libc_fn + 320);
        _mm_clflush((char*) libc_fn + 384);
        _mm_clflush((char*) libc_fn + 448);

        _mm_mfence();

        if (bit_to_send == 1) {
            int s = ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
            (void)s;
        }

        bit_index = (bit_index + 1) % num_bits;
    }

    return 0;
}