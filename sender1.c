#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

// 发送比特的核心函数
void send_bit(void *addr, int bit) {
    if (bit == 1) {
        // 频繁访问目标地址，使其驻留在 Cache
        for (int i = 0; i < 300; i++) {
            *(volatile uint8_t *)addr;
        }
    } else {
        // 确保该地址不在 Cache
        _mm_clflush(addr);
    }
    _mm_mfence();
    // 这里的延时要和 receiver 匹配
    usleep(500); 
}

int main() {
    // 1. 自动检测 libc 路径
    const char *libc_path = "/lib64/libc.so.6";
    if (access(libc_path, F_OK) == -1) {
        libc_path = "/lib/x86_64-linux-gnu/libc.so.6";
    }

    int fd = open(libc_path, O_RDONLY);
    if (fd < 0) {
        perror("[Sender] 无法打开 libc");
        return 1;
    }

    // 2. 映射足够大的内存空间 (2MB)
    size_t map_len = 2 * 1024 * 1024;
    void *map_base = mmap(NULL, map_len, PROT_READ, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) {
        perror("[Sender] mmap 失败");
        close(fd);
        return 1;
    }

    // 3. 计算目标地址 (这个偏移量 0x164ac0 需要通过 nm 指令确认)
    void *addr = (uint8_t *)map_base + 0x164ac0; 

    // 定义同步信号变量 (解决你报错的 sync_val 未定义问题)
    uint32_t sync_val = 0x3F2; 

    printf("[Sender] 正在发送同步信号，地址: %p\n", addr);
    fflush(stdout);

    // 4. 循环发送
    while (1) {
        // 发送 10 位的同步信号
        for (int i = 9; i >= 0; i--) {
            int bit = (sync_val >> i) & 1;
            send_bit(addr, bit);
        }
        // 每轮同步信号之间的长间隔
        usleep(20000); 
    }

    // 正常不会执行到这里
    munmap(map_base, map_len);
    close(fd);
    return 0;
}