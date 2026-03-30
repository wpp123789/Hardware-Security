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
#define SYNC_WORD   0xAA       // 10101010，用于同步

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

// 发送一个bit：
// 协议：flush → 等200us(传播) → 如果是1则load → 等BIT_US(receiver在此采样)
void send_bit(void *addr, int bit) {
    flush_addr(addr);
    usleep(200);               // 等flush传播到其他核心
    if (bit == 1) {
        load_addr(addr);       // 加载进LLC，receiver会看到HIT
    }
    usleep(BIT_US);            // 保持状态，等receiver采样完毕
}

void send_byte(void *addr, uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (b >> i) & 1);
    }
}

int main() {
    printf("[Sender] Starting...\n");

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) {
        fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    }
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Sender] addr = %p\n", addr);
    fflush(stdout);

    const char *message = "Hello!HW3";

    while (1) {
        printf("[Sender] Sending: %s\n", message);
        fflush(stdout);

        // 1. 发送同步头：8个交替的1010_1010，让receiver对齐时钟
        for (int i = 0; i < 16; i++) {
            send_bit(addr, i % 2);  // 0,1,0,1,0,1...
        }

        // 2. 发送起始标志：固定的0xFF，receiver检测到此字节后开始接收
        send_byte(addr, 0xFF);

        // 3. 发送消息内容
        for (int i = 0; message[i] != '\0'; i++) {
            send_byte(addr, (uint8_t)message[i]);
        }

        // 4. 发送结束符
        send_byte(addr, 0x00);

        // 两轮之间停顿，让receiver重置
        usleep(200000);  // 200ms
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
