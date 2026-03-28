#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <time.h>

#define THRESHOLD_CYCLES 506   // 用你测出来的值
#define MSG_BITS (26 * 8)      // 26个字母，每个8bit = 208 bits
#define BIT_SLOT_NS 1000000    // 和sender一样，1ms per bit

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

static void sleep_ns(long ns) {
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[]) {

    int threshold = THRESHOLD_CYCLES;

    // 支持 -t 参数覆盖阈值
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && (i + 1) < argc) {
            threshold = atoi(argv[++i]);
        }
    }

    printf("[Receiver] Starting up...\n");
    printf("[Receiver] Threshold = %d cycles, slot = %d ms\n",
           threshold, BIT_SLOT_NS / 1000000);
    fflush(stdout);

    // 获取和sender相同的libc函数地址
    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "[Receiver] dlopen failed\n");
        return 1;
    }
    void *libc_fn = dlsym(handle, "ecvt_r");
    if (!libc_fn) {
        fprintf(stderr, "[Receiver] dlsym failed\n");
        return 1;
    }
    printf("[Receiver] libc address = %p\n", libc_fn);
    fflush(stdout);
    dlclose(handle);

    double pi = 3.141592653589793;
    int decpt = 0, sign = 0;
    char buf[64];

    // 收集一轮完整消息的bits
    int bits[MSG_BITS];
    int count = 0;
    int round = 0;

    printf("[Receiver] Waiting for bits...\n");
    fflush(stdout);

    while (1) {
        // Step 1: 计时访问 ecvt_r
        uint64_t start = rdtscp64();
        int s = ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
        (void)s;
        uint64_t end = rdtscp64();
        uint64_t t = end - start;

        // Step 2: 判断是hit还是miss
        int bit = (t < threshold) ? 1 : 0;
        bits[count++] = bit;

        // Step 3: 等待下一个时间槽
        sleep_ns(BIT_SLOT_NS);

        // Step 4: 收够一轮后解码打印
        if (count == MSG_BITS) {
            round++;
            printf("\n[Receiver] Round %d decoded: ", round);

            for (int i = 0; i < 26; i++) {
                char c = 0;
                for (int b = 0; b < 8; b++) {
                    c |= bits[i * 8 + b] << b;
                }
                printf("%c", c);
            }
            printf("\n");

            // 打印原始比特流（前40bits）供调试
            printf("[Receiver] First 40 bits: ");
            for (int i = 0; i < 40; i++) {
                printf("%d", bits[i]);
            }
            printf("...\n");
            fflush(stdout);

            count = 0;  // 重置，接收下一轮
        }
    }

    return 0;
}
