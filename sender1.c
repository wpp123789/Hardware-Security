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
    // 1. 自动适配不同的系统路径
    const char *libc_path = "/lib64/libc.so.6";
    if (access(libc_path, F_OK) == -1) {
        libc_path = "/lib/x86_64-linux-gnu/libc.so.6";
    }

    int fd = open(libc_path, O_RDONLY);
    if (fd < 0) {
        perror("无法打开 libc 文件 (open failed)");
        return 1;
    }

    // 2. 映射足够大的空间 (映射 2MB，确保 0x164ac0 在范围内)
    size_t map_len = 2 * 1024 * 1024; 
    void *map_base = mmap(NULL, map_len, PROT_READ, MAP_SHARED, fd, 0);
    
    if (map_base == MAP_FAILED) {
        perror("内存映射失败 (mmap failed)");
        close(fd);
        return 1;
    }

    // 3. 计算目标地址
    void *addr = (uint8_t *)map_base + 0x164ac0;

    // 检查地址是否正常
    printf("[Receiver] 映射成功！基地址: %p, 监听目标地址: %p\n", map_base, addr);

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