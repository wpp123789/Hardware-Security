#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define BIT_US      5000       // 必须和sender完全一致
#define ECVT_OFF    0x138ac0
#define MAP_SIZE    0x200000
#define THRESHOLD   100        // HIT~18-32, MISS~165+

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

// 采样一个bit
// sender节奏：flush→等200us→(load)→等BIT_US
// receiver采样点：在200us之后、BIT_US结束之前的中间位置
// 即等待：200 + BIT_US/2 = 2700us后采样
int sample_bit(void *addr, int threshold) {
    usleep(200 + BIT_US / 2);   // 等到bit窗口中央

    _mm_mfence();
    unsigned aux;
    uint64_t t0 = __rdtscp(&aux);
    *(volatile char *)addr;
    uint64_t t1 = __rdtscp(&aux);
    _mm_mfence();

    uint64_t t = t1 - t0;
    int bit = (t < threshold) ? 1 : 0;

    usleep(BIT_US / 2);         // 等剩余半个窗口，对齐到下一个bit

    return bit;
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

    printf("[Receiver] Starting... threshold=%d, BIT_US=%d\n", threshold, BIT_US);

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) {
        fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    }
    if (fd < 0) { perror("open failed"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (char*)map + ECVT_OFF;

    printf("[Receiver] addr = %p\n", addr);
    printf("[Receiver] Waiting for sync...\n");
    fflush(stdout);

    while (1) {
        // 1. 检测同步头：连续看到0,1交替则对齐
        //    用一个滑动窗口检测0xFF起始标志
        uint8_t b = recv_byte(addr, threshold);

        if (b != 0xFF) {
            // 还没对齐，打印收到的字节帮助调试
            printf("[sync] got 0x%02X, waiting for 0xFF...\n", b);
            fflush(stdout);
            continue;
        }

        // 2. 收到起始标志0xFF，开始接收消息
        printf("[Receiver] Got start marker! Reading message: ");
        fflush(stdout);

        while (1) {
            uint8_t c = recv_byte(addr, threshold);
            if (c == 0x00) {
                printf("\n[Receiver] End of message.\n");
                fflush(stdout);
                break;
            }
            // 只打印可打印字符
            if (c >= 0x20 && c <= 0x7E) {
                printf("%c", c);
            } else {
                printf("[0x%02X]", c);
            }
            fflush(stdout);
        }
    }

    munmap(map, MAP_SIZE);
    close(fd);
    return 0;
}
