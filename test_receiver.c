#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define THRESHOLD 96      // 你测得的建议值
#define BIT_INTERVAL 1000 
#define SYNC_HEADER 0x3F2

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    
    _mm_mfence();                      // 确保之前的操作完成
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr;         // 内存读取
    _mm_lfence();                      // 确保读取完成后再停表
    end = __rdtscp(&aux);
    
    _mm_clflush(addr);                 // 核心：读完必须清除，否则下次永远是 Hit
    _mm_mfence();
    
    return end - start;
}

int main() {
    int fd = open("/lib64/libc.so.6", O_RDONLY);
    if (fd < 0) fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0; // 必须与 Sender 完全一致

    printf("[Test Receiver] 阈值: %d, 监听中...\n", THRESHOLD);

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;
        
        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;

        if (shift_reg == SYNC_HEADER) {
            printf("\n[SYNC OK] 收到模式: ");
            usleep(1200); // 跳到第一个数据位中心

            for (int i = 0; i < 8; i++) {
                uint64_t rt = reload_t(addr); // 立即读
                printf("%d", (rt < THRESHOLD) ? 1 : 0);
                fflush(stdout);
                usleep(BIT_INTERVAL); 
            }
            printf("\n");
            shift_reg = 0;
        }
        usleep(BIT_INTERVAL); // 寻找同步头时的采样间隔
    }
}