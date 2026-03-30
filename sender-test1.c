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
#define MAP_SIZE    0x200000   // 和receiver一样，2MB从偏移0开始

static inline void flush_addr(void *addr) {
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();
}

static inline void load_addr(void *addr) {
    for (int i = 0; i < 200; i++) {
        *(volatile char *)addr;
        *(volatile char *)((char*)addr + 64);
    }
    _mm_mfence();
}

void send_bit(void *addr, int bit) {
    flush_addr(addr);
    if (bit == 1) {
        load_addr(addr);
    }
    usleep(BIT_US);
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (b >> i) & 1);
    }
}

int main() {
    printf("[Sender] Starting...\n");

    // 和receiver完全相同的mmap方式：从偏移0开始map整个区域
    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) { perror("open failed"); return 1; }

    // 从文件偏移0开始map，大小2MB
    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Sender] map base = %p\n", map);
    printf("[Sender] addr = %p (offset 0x%x)\n", addr, ECVT_OFF);
    printf("[Sender] BIT_US = %d us\n", BIT_US);
    fflush(stdout);

    // 测试：先只发1，验证receiver能看到全HIT
    printf("[Sender] TEST MODE: sending all 1s...\n");
    fflush(stdout);
    while(1) {
        load_addr(addr);
        usleep(BIT_US);
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
