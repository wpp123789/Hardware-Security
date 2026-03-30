#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US      5000       // 5ms per bit
#define ECVT_OFF    0x138ac0
#define MAP_SIZE    0x200000

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

// 协议：
// 发送1：先flush确保干净，然后立刻load进缓存，等待BIT_US让receiver采样
// 发送0：先flush，不load，等待BIT_US让receiver采样
void send_bit(void *addr, int bit) {
    flush_addr(addr);           // 先清空
    _mm_mfence();
    usleep(200);                // 给flush一点时间传播

    if (bit == 1) {
        load_addr(addr);        // 加载进缓存
    }
    // receiver在这段时间内采样
    usleep(BIT_US);
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (b >> i) & 1);
    }
}

int main() {
    printf("[Sender] Starting...\n");

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Sender] addr = %p (offset 0x%x)\n", addr, ECVT_OFF);
    printf("[Sender] BIT_US = %d us\n", BIT_US);
    fflush(stdout);

    // 测试：交替发1和0，receiver应该看到 HIT MISS HIT MISS...
    printf("[Sender] TEST MODE: sending alternating 1,0,1,0...\n");
    fflush(stdout);

    int bit = 1;
    while(1) {
        send_bit(addr, bit);
        printf("[Sender] sent %d\n", bit);
        fflush(stdout);
        bit = !bit;
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
