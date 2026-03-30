#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#define THRESHOLD 94
#define SYNC_HEADER 0x3F2
#define BIT_INTERVAL 1000 

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    _mm_lfence();
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr;
    _mm_lfence();
    end = __rdtscp(&aux);
    return end - start;
}
int main() {
    // ... mmap 逻辑保持不变 ...
    void *addr = (uint8_t *)map_base + 0x164ac0;

    printf("[Test Receiver] 使用阈值 %d 监听中...\n", THRESHOLD);

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;
        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;

        if (shift_reg == SYNC_HEADER) {
            printf("\n[SYNC OK] 收到位序列: ");
            
            // 相位对齐：跳到第一个数据位的中心
            usleep(BIT_INTERVAL + (BIT_INTERVAL / 2)); 

            for (int i = 0; i < 8; i++) {
                uint64_t rt = reload_t(addr); // 立即读
                printf("%d", (rt < THRESHOLD) ? 1 : 0);
                fflush(stdout);
                usleep(BIT_INTERVAL); // 读完再睡
            }
            printf("\n");
            shift_reg = 0;
        }
        usleep(BIT_INTERVAL);
    }
}