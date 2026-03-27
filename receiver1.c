#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

// 根据你的 threshold.c 测量结果，如果直接测内存访问，
// 缓存命中通常 < 100 cycles，未命中 > 200 cycles。
#define THRESHOLD 130 
#define SYNC_HEADER 0x3F2 // 同步头：用于定位数据开始

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr; // [cite: 41, 68] 仅仅读取一个字节
    _mm_mfence();
    end = __rdtscp(&aux);
    _mm_clflush(addr); // [cite: 68] 读取后立即清空，等待 Sender 下一次填入
    return end - start;
}

int main(int argc, char **argv) {
    // 映射 libc 获取共享的物理地址 
    int fd = open("/lib64/libc.so.6", O_RDONLY);
    // 使用 nm 指令找到 ecvt_r 的偏移量，例如 0x164ac0
    void *addr = mmap(NULL, 64, PROT_READ, MAP_SHARED, fd, 0x164ac0);
    
    printf("[Receiver] 正在监听地址: %p\n", addr);

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0; // 快代表 Sender 访问过 (1) 

        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;
        if (shift_reg == SYNC_HEADER) {
            printf("\n[Receiver] 检测到同步头，开始接收载荷...\n");
        }
        
        // 建议增加采样间隔，与 Sender 匹配
        usleep(500); 
    }
    return 0;
}