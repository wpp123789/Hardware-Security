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
#define THRESHOLD   100

// 采样一个bit，不带任何等待（由调用方控制时序）
static int raw_sample(void *addr) {
    _mm_mfence();
    unsigned aux;
    uint64_t t0 = __rdtscp(&aux);
    *(volatile char *)addr;
    uint64_t t1 = __rdtscp(&aux);
    _mm_mfence();
    return (t1 - t0 < THRESHOLD) ? 1 : 0;
}

// 等待到bit窗口中央再采样
static int sample_bit(void *addr) {
    usleep(200 + BIT_US / 2);  // 等到窗口中央
    int bit = raw_sample(addr);
    usleep(BIT_US / 2);        // 等到窗口结束
    return bit;
}

static uint8_t recv_byte(void *addr) {
    uint8_t b = 0;
    for (int i = 7; i >= 0; i--)
        b |= (sample_bit(addr) << i);
    return b;
}

int main(int argc, char *argv[]) {
    printf("[Receiver] Starting... threshold=%d BIT_US=%d\n", THRESHOLD, BIT_US);

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }
    void *addr = (char*)map + ECVT_OFF;

    printf("[Receiver] addr = %p\n", addr);
    printf("[Receiver] Waiting for sync (16x HIT then MISS)...\n");
    fflush(stdout);

    while (1) {
        // 步骤1：等待连续8个HIT（同步头开始）
        int hit_count = 0;
        while (hit_count < 8) {
            // 每隔半个BIT_US快速扫描，寻找HIT流
            usleep(BIT_US / 2);
            int b = raw_sample(addr);
            if (b == 1) {
                hit_count++;
            } else {
                hit_count = 0;  // 重置，必须连续8个HIT
            }
        }

        // 步骤2：找到了连续HIT，现在对齐到同步结束的0
        // sender发完16个1之后发一个0
        // 继续采样直到看到0
        int bit;
        int extra_ones = 0;
        do {
            usleep(BIT_US / 2);
            bit = raw_sample(addr);
            usleep(BIT_US / 2);
            if (bit == 1) extra_ones++;
        } while (bit == 1 && extra_ones < 20);

        if (extra_ones >= 20) continue;  // 没找到结束0，重新同步

        printf("[Receiver] Synced! Reading message: ");
        fflush(stdout);

        // 步骤3：读取消息字节
        while (1) {
            uint8_t c = recv_byte(addr);
            if (c == 0x00) {
                printf("\n[Receiver] Done.\n");
                fflush(stdout);
                break;
            }
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
