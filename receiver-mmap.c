#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US     50000   // 和 sender 一样
#define ECVT_OFF   0x138ac0
#define MAP_SIZE   (ECVT_OFF + 4096)
#define THRESHOLD  150     // mmap直接读内存，比调用函数快，阈值要低

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

int sample_bit(void *addr, int threshold) {
    // Step1: flush
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();

    // Step2: 等 sender 操作（等窗口的 3/4）
    usleep(BIT_US * 3 / 4);

    // Step3: reload 计时
    _mm_mfence();
    uint64_t t0 = rdtscp64();
    *(volatile char *)addr;
    uint64_t t1 = rdtscp64();
    _mm_mfence();

    uint64_t t = t1 - t0;

    // Step4: 等剩余 1/4 时间
    usleep(BIT_US / 4);

    printf("[dbg] t=%lu %s\n", t, t < threshold ? "HIT" : "MISS");
    fflush(stdout);

    return (t < threshold) ? 1 : 0;
}

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

    int fd = open("/lib64/libc.so.6", O_RDONLY);
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Receiver] libc file offset = 0x%x\n", ECVT_OFF);
    printf("[Receiver] mapped addr = %p\n", addr);
    printf("[Receiver] Waiting for preamble 0xAA 0xAA...\n");
    fflush(stdout);

    while (1) {
        uint8_t b1 = recv_byte(addr, threshold);
        printf("[Receiver] sync byte1 = 0x%02x\n", b1);
        fflush(stdout);
        if (b1 != 0xAA) continue;

        uint8_t b2 = recv_byte(addr, threshold);
        printf("[Receiver] sync byte2 = 0x%02x\n", b2);
        fflush(stdout);
        if (b2 != 0xAA) continue;

        printf("[Receiver] Synced! Receiving: ");
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

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
