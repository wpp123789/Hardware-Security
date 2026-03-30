#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define THRESHOLD 96      // 对应你 threshold.c 测得的建议值
#define SYNC_HEADER 0x3F2
#define BIT_INTERVAL 1000 

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    _mm_mfence();
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr; // 触发内存访问
    _mm_lfence();
    end = __rdtscp(&aux);
    _mm_clflush(addr);         // 采样后必须立即清除，否则后续永远是 Hit
    _mm_mfence();
    return end - start;
}

int main() {
    // ... mmap 部分保持不变 ...
    void *addr = (uint8_t *)map_base + 0x164ac0;

    printf("[Receiver] 硬件校准完成，准备接收数据...\n");

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;
        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;

        if (shift_reg == SYNC_HEADER) {
            printf("\n[收到消息]: ");
            fflush(stdout);

            // 相位补偿：跳到第一个数据位的中心
            // 之前 00101010 说明 1500 可能稍晚，尝试用 1400 进入位中心
            usleep(BIT_INTERVAL + 400); 

            while(1) {
                uint8_t c = 0;
                for (int i = 0; i < 8; i++) {
                    // 1. 立即采样（此时位于位中心）
                    uint64_t rt = reload_t(addr);
                    c = (c << 1) | ((rt < THRESHOLD) ? 1 : 0);
                    
                    // 2. 采样完再睡，等待下一位
                    usleep(BIT_INTERVAL); 
                }
                
                if (c == 0 || c > 126) break; 
                printf("%c", c);
                fflush(stdout);
            }
            shift_reg = 0;
        }
        usleep(BIT_INTERVAL); 
    }
    return 0;
}