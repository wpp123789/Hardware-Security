#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define BIT_INTERVAL 1000 

void send_bit(void *addr, int bit) {
    if (bit == 1) {
        // 疯狂读取 1000 次，确保地址在所有 Core 的 Cache 中都变“热”
        for (int i = 0; i < 1000; i++) {
            *(volatile uint8_t *)addr;
            _mm_mfence();
        }
    } else {
        // 彻底清除
        _mm_clflush(addr);
        _mm_mfence();
    }
    usleep(BIT_INTERVAL); 
}

int main() {
    int fd = open("/lib64/libc.so.6", O_RDONLY);
    if (fd < 0) fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0;

    uint32_t sync_header = 0x3F2; 
    uint8_t test_pattern = 0xAA; 

    printf("[Test Sender] 发送中...\n");

    while (1) {
        for (int i = 9; i >= 0; i--) send_bit(addr, (sync_header >> i) & 1);
        for (int i = 7; i >= 0; i--) send_bit(addr, (test_pattern >> i) & 1);
        usleep(100000); 
    }
}