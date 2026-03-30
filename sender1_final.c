#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US      5000
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

void send_bit(void *addr, int bit) {
    flush_addr(addr);
    usleep(200);
    if (bit == 1) load_addr(addr);
    usleep(BIT_US);
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--)
        send_bit(addr, (b >> i) & 1);
}

int main() {
    printf("[Sender] Starting...\n");

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }
    void *addr = (char*)map + ECVT_OFF;

    printf("[Sender] addr = %p\n", addr);
    fflush(stdout);

    const char *message = "Hello!HW3";

    while (1) {
        printf("[Sender] Sending: %s\n", message);
        fflush(stdout);

        // 同步头：连续16个1，让receiver检测到稳定的HIT流
        // 然后发一个0作为同步结束标志
        for (int i = 0; i < 16; i++) send_bit(addr, 1);
        send_bit(addr, 0);  // 同步结束标志位

        // 发送消息
        for (int i = 0; message[i] != '\0'; i++)
            send_byte(addr, (uint8_t)message[i]);

        // 结束符
        send_byte(addr, 0x00);

        // 两轮间隔
        usleep(500000);
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
