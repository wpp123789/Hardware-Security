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
        // 发送1：频繁访问该地址
        for (int i = 0; i < 200; i++) {
            *(volatile uint8_t *)addr;
        }
    } else {
        // 发送0：强制清空
        _mm_clflush(addr);
    }
    _mm_mfence();
    usleep(500); 
}

int main() {
    // 自动检测 libc 路径
    const char *libc_path = "/lib64/libc.so.6";
    if (access(libc_path, F_OK) == -1) {
        libc_path = "/lib/x86_64-linux-gnu/libc.so.6";
    }

    int fd = open(libc_path, O_RDONLY);
    if (fd < 0) {
        perror("Open libc failed");
        return 1;
    }

    // 映射并寻找偏移量 (根据你的系统微调 0x164ac0)
    void *map_base = mmap(NULL, 1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    void *addr = (uint8_t *)map_base + 0x164ac0; 

    uint32_t sync_val = 0x3F2; 
    printf("[Sender] 正在发送同步信号，目标地址: %p\n", addr);

    while (1) {
        for (int i = 9; i >= 0; i--) {
            send_bit(addr, (sync_val >> i) & 1);
        }
        usleep(10000); // 组间间隔
    }

    munmap(map_base, 1024*1024);
    close(fd);
    return 0;
}