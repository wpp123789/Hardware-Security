#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <string.h>
#include <time.h>

#define BIT_SLOT_NS 1000000  // 1 ms

static void sleep_ns(long ns) {
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

int main(void) {
    printf("[Sender] Starting up...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *libc_fn = dlsym(handle, "ecvt_r");

    if (!libc_fn) {
        printf("dlsym failed\n");
        return 1;
    }

    printf("[Sender] libc address = %p\n", libc_fn);

    // ===== Preamble =====
    const uint8_t preamble[] = {
        1,1,1,1,1,1,1,1,
        0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,
        0,0,0,0,0,0,0,0
    };
    size_t preamble_len = sizeof(preamble);

    // ===== Message =====
    const char *msg = "HELLO_WORLD_1234567890";
    size_t msg_len = strlen(msg);
    size_t num_bits = msg_len * 8;

    size_t total_bits = preamble_len + num_bits;
    unsigned long bit_index = 0;

    double pi = 3.141592653589793;
    int decpt, sign;
    char buf[64];

    printf("[Sender] Sending with sync...\n");

    while (1) {

        int bit;

        if (bit_index < preamble_len) {
            bit = preamble[bit_index];
        } else {
            size_t data_index = (bit_index - preamble_len) % num_bits;
            size_t char_index = data_index / 8;
            int bit_pos = data_index % 8;
            bit = (msg[char_index] >> bit_pos) & 1;
        }

        // Flush cache lines
        for (int i = 0; i < 8; i++) {
            _mm_clflush((char*)libc_fn + i * 64);
        }

        _mm_mfence();

        // Send 1 → make it strong (multiple accesses)
        if (bit == 1) {
            for (int i = 0; i < 5; i++) {
                ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
            }
        }

        sleep_ns(BIT_SLOT_NS);

        bit_index = (bit_index + 1) % total_bits;
    }

    return 0;
}