#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <time.h>

#define THRESHOLD_CYCLES 200
#define BIT_SLOT_NS 1000000

#define PREAMBLE_LEN 32
#define MSG_LEN 22   // HELLO_WORLD_1234567890 长度

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

static void sleep_ns(long ns) {
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

int main() {

    printf("[Receiver] Starting...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *libc_fn = dlsym(handle, "ecvt_r");

    if (!libc_fn) {
        printf("dlsym failed\n");
        return 1;
    }

    printf("[Receiver] libc address = %p\n", libc_fn);

    const uint8_t preamble[PREAMBLE_LEN] = {
        1,1,1,1,1,1,1,1,
        0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,
        0,0,0,0,0,0,0,0
    };

    int preamble_buf[PREAMBLE_LEN];
    int preamble_idx = 0;
    int sync = 0;

    int total_bits = MSG_LEN * 8;
    int bits[1024];
    int count = 0;

    double pi = 3.141592653589793;
    int decpt, sign;
    char buf[64];

    while (1) {

        // ===== 多次采样（降噪）=====
        uint64_t total = 0;
        for (int i = 0; i < 5; i++) {
            uint64_t start = rdtscp64();
            ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
            uint64_t end = rdtscp64();
            total += (end - start);
        }
        uint64_t t = total / 5;

        int bit = (t < THRESHOLD_CYCLES) ? 1 : 0;

        // ===== 同步阶段 =====
        if (!sync) {
            preamble_buf[preamble_idx++] = bit;

            if (preamble_idx == PREAMBLE_LEN) {
                int match = 1;
                for (int i = 0; i < PREAMBLE_LEN; i++) {
                    if (preamble_buf[i] != preamble[i]) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    printf("\n[Receiver] SYNC ACQUIRED!\n");
                    sync = 1;
                    count = 0;
                }

                preamble_idx = 0;
            }

            sleep_ns(BIT_SLOT_NS);
            continue;
        }

        // ===== 收数据 =====
        bits[count++] = bit;

        sleep_ns(BIT_SLOT_NS);

        // ===== 解码 =====
        if (count == total_bits) {

            printf("[Receiver] Decoded: ");

            for (int i = 0; i < MSG_LEN; i++) {
                char c = 0;
                for (int b = 0; b < 8; b++) {
                    c |= bits[i * 8 + b] << b;
                }
                printf("%c", c);
            }

            printf("\n");
            fflush(stdout);

            count = 0;
            sync = 0;  // 重新同步（更稳）
        }
    }

    return 0;
}