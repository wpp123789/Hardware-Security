#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define THRESHOLD 130 
#define SYNC_HEADER 0x3F2

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr; 
    _mm_mfence();
    end = __rdtscp(&aux);
    _mm_clflush(addr); 
    return end - start;
}

int main() {
    // 自动适配路径
    const char *path = "/lib64/libc.so.6";
    if (access(path, F_OK) == -1) path = "/lib/x86_64-linux-gnu/libc.so.6";

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("Open failed"); return 1; }

    // 映射长度 2MB 确保覆盖偏移量
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) { perror("mmap failed"); return 1; }

    // 使用与 Sender 相同的偏移量
    void *addr = (uint8_t *)map_base + 0x164ac0;

    printf("[Receiver] 映射成功! 地址: %p\n", addr);

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;

        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;
        if (shift_reg == SYNC_HEADER) {
            printf("!"); // 抓到同步头打印感叹号
            fflush(stdout);
        }
        usleep(500); 
    }
    return 0;
}