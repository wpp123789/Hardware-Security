#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

// 统一位间隔为 1ms，确保稳定性
#define BIT_INTERVAL 1000 

void send_bit(void *addr, int bit) {
    if (bit == 1) {
        // 强化信号：持续访问地址
        for (int i = 0; i < 100; i++) {
            *(volatile uint8_t *)addr;
            _mm_mfence();
        }
    } else {
        // 发送 0：清除缓存
        _mm_clflush(addr);
    }
    _mm_mfence();
    usleep(BIT_INTERVAL); 
}

int main() {
    const char *path = "/lib64/libc.so.6";
    int fd = open(path, O_RDONLY);
    // 映射 libc 并定位到偏移量
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0;

    uint32_t sync_header = 0x3F2; // 同步头
    uint8_t test_pattern = 0xAA;  // 模式：10101010

    printf("[Test Sender] 正在发送测试模式: 同步头 + 10101010\n");

    while (1) {
        // 1. 发送 10 位同步头
        for (int i = 9; i >= 0; i--) {
            send_bit(addr, (sync_header >> i) & 1);
        }
        // 2. 发送 8 位测试数据
        for (int i = 7; i >= 0; i--) {
            send_bit(addr, (test_pattern >> i) & 1);
        }
        // 轮次间隔
        usleep(100000); 
    }
    return 0;
}