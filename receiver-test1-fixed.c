#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US      5000       // 必须和sender一致
#define ECVT_OFF    0x138ac0
#define MAP_SIZE    0x200000
#define THRESHOLD   100        // HIT~18, MISS~165+，取中间值100

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

// 采样时机：在bit窗口的中间采样
// sender节奏：flush(200us) → [load或不load] → 等BIT_US
// receiver节奏：等(200us + BIT_US/2) → 采样  ← 落在窗口正中央
int sample_bit(void *addr, int threshold) {
    // 等到bit窗口中央：flush传播时间 + 半个位间隔
    usleep(200 + BIT_US / 2);

    _mm_mfence();
    uint64_t t0 = rdtscp64();
    *(volatile char *)addr;
    uint64_t t1 = rdtscp64();
    _mm_mfence();

    uint64_t t = t1 - t0;

    // 等剩余半个窗口，对齐到下一个bit起始
    usleep(BIT_US / 2);

    int bit = (t < threshold) ? 1 : 0;
    printf("[dbg] t=%lu %s\n", t, bit ? "HIT(1)" : "MISS(0)");
    fflush(stdout);

    return bit;
}

int main(int argc, char *argv[]) {
    int threshold = THRESHOLD;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i+1 < argc)
            threshold = atoi(argv[++i]);
    }

    printf("[Receiver] Starting... threshold=%d\n", threshold);

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Receiver] addr = %p (offset 0x%x)\n", addr, ECVT_OFF);
    printf("[Receiver] TEST: should see HIT MISS HIT MISS...\n");
    fflush(stdout);

    while (1) {
        sample_bit(addr, threshold);
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
