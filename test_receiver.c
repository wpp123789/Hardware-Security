#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define THRESHOLD 96      // 使用你测得的最优值
#define BIT_INTERVAL 1000 
#define SYNC_HEADER 0x3F2

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    _mm_lfence();
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr; // 纯内存读取
    _mm_lfence();
    end = __rdtscp(&aux);
    _mm_clflush(addr);         // 必须立即清除，准备下次采样
    return end - start;
}

int main() {
    const char *path = "/lib64/libc.so.6";
    int fd = open(path, O_RDONLY);
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0;

    printf("[Test Receiver] 阈值: %d, 监听中...\n", THRESHOLD);

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;
        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;

        if (shift_reg == SYNC_HEADER) {
            printf("\n[SYNC OK] 收到原始位: ");
            
            // 相位补偿：跳到第一个数据位的中心
            usleep(BIT_INTERVAL + (BIT_INTERVAL / 2)); 

            for (int i = 0; i < 8; i++) {
                // 1. 立即读（此时处于位中心）
                uint64_t rt = reload_t(addr); 
                printf("%d", (rt < THRESHOLD) ? 1 : 0);
                fflush(stdout);
                
                // 2. 读完再睡，等待进入下一位的中心
                usleep(BIT_INTERVAL); 
            }
            printf("\n");
            shift_reg = 0; // 重置，寻找下一个同步头
        }
        usleep(BIT_INTERVAL);
    }
    return 0;
}