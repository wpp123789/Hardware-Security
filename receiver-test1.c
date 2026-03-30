#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US      50000
#define ECVT_OFF    0x138ac0
#define MAP_SIZE    0x200000   // 2MB从偏移0开始
#define THRESHOLD   130

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

int sample_bit(void *addr, int threshold) {
    // 不flush，让sender控制cache状态
    // 等半个窗口让sender完成操作
    usleep(BIT_US / 2);

    _mm_mfence();
    uint64_t t0 = rdtscp64();
    *(volatile char *)addr;
    uint64_t t1 = rdtscp64();
    _mm_mfence();

    uint64_t t = t1 - t0;

    // 等剩余半个窗口
    usleep(BIT_US / 2);

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
    printf("[Receiver] threshold=%d\n", threshold);

    // 和sender完全相同的mmap：从文件偏移0开始
    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Receiver] map base = %p\n", map);
    printf("[Receiver] addr = %p (offset 0x%x)\n", addr, ECVT_OFF);
    fflush(stdout);

    // 先测试：sender发全1时，应该全是HIT
    printf("[Receiver] TEST: should see all HIT if sender is loading...\n");
    fflush(stdout);

    while (1) {
        sample_bit(addr, threshold);
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
