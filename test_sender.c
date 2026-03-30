// ... 头文件和 send_bit 定义参照你之前的 sender1.c ...
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>


void send_bit(void *addr, int bit) {
    if (bit == 1) {
        // 强化信号：循环 100 次纯内存读取
        for (int i = 0; i < 100; i++) {
            *(volatile uint8_t *)addr;
            _mm_mfence();
        }
    } else {
        // 发送 0：彻底清除
        _mm_clflush(addr);
    }
    _mm_mfence();
    usleep(1000); 
}
// 发送一个字节（8位）
void send_byte(void *addr, uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (byte >> i) & 1);
    }
}
int main() {
    // ... mmap 逻辑保持不变 ...
    void *addr = (uint8_t *)map_base + 0x164ac0; 

    uint32_t sync_header = 0x3F2; 
    uint8_t test_pattern = 0xAA; // 二进制: 10101010

    printf("[Test Sender] 发送模式: 同步头 + 10101010\n");

    while (1) {
        // 1. 发送同步头 (10位)
        for (int i = 9; i >= 0; i--) {
            send_bit(addr, (sync_header >> i) & 1);
        }
        // 2. 发送测试字节
        for (int i = 7; i >= 0; i--) {
            send_bit(addr, (test_pattern >> i) & 1);
        }
        usleep(100000); // 间隔一段时间再发下一轮
    }
    return 0;
}