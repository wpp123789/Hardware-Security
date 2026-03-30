#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

void send_bit(void *addr, int bit) {
    if (bit == 1) {
        for (int i = 0; i < 300; i++) {
            *(volatile uint8_t *)addr;
        }
    } else {
        _mm_clflush(addr);
    }
    _mm_mfence();
    usleep(500); 
}

int main() {
    const char *path = "/lib64/libc.so.6";
    if (access(path, F_OK) == -1) path = "/lib/x86_64-linux-gnu/libc.so.6";

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("Open failed"); return 1; }

    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) { perror("mmap failed"); return 1; }

    void *addr = (uint8_t *)map_base + 0x164ac0;
    uint32_t sync_val = 0x3F2; 

    printf("[Sender] 映射成功! 地址: %p\n", addr);

    while (1) {
        for (int i = 9; i >= 0; i--) {
            send_bit(addr, (sync_val >> i) & 1);
        }
        usleep(20000); 
    }
    return 0;
}