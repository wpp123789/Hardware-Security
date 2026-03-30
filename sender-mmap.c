#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US     50000   // 50ms per bit
#define ECVT_OFF   0x138ac0 // ecvt_r 在 libc 文件中的偏移
#define MAP_SIZE   (ECVT_OFF + 4096)

static inline void flush_addr(void *addr) {
    _mm_clflush(addr);
    _mm_clflush((char*)addr + 64);
    _mm_clflush((char*)addr + 128);
    _mm_clflush((char*)addr + 192);
    _mm_mfence();
}

static inline void load_addr(void *addr) {
    // 多次读取，强化 cache 信号
    for (int i = 0; i < 100; i++) {
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
    // 保持这个状态，等 receiver 来采样
    usleep(BIT_US);
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (b >> i) & 1);
    }
}

int main() {
    printf("[Sender] Starting...\n");

    int fd = open("/lib64/libc.so.6", O_RDONLY);
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Sender] libc file offset = 0x%x\n", ECVT_OFF);
    printf("[Sender] mapped addr = %p\n", addr);
    printf("[Sender] BIT_US = %d us\n", BIT_US);
    fflush(stdout);

    char *msg = "HELLO";

 /*  while (1) {
        printf("[Sender] Sending preamble + message: %s\n", msg);
        fflush(stdout);

        // 同步前导码：0xAA 0xAA = 10101010 10101010
        send_byte(addr, 0xAA);
        send_byte(addr, 0xAA);

        // 发送消息
        for (int i = 0; i < (int)strlen(msg); i++) {
            send_byte(addr, (uint8_t)msg[i]);
        }

        // 结束符
        send_byte(addr, 0x00);

        usleep(200000); // 200ms 间隔
   }
*/ 
// 临时测试：一直发1
    while(1) {
        load_addr(addr);
        usleep(BIT_US);
    }
    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
