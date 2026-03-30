#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

// 1. 修改为你新版 threshold.c 测出的建议值 (假设现在是 150)
#define THRESHOLD 150 
#define SYNC_HEADER 0x3F2
// 建议将位间隔提高到 1000us (1ms) 以增加作业的可靠性
#define BIT_INTERVAL 1000 

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    _mm_mfence();                      // 内存屏障，确保指令顺序
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr;         // 核心：直接访问内存地址
    _mm_lfence();                      // 确保读取完成后再停表
    end = __rdtscp(&aux);
    _mm_clflush(addr);                 // 测量后立即清除，准备下一次采样
    return end - start;
}

int main() {
    const char *path = "/lib64/libc.so.6";
    if (access(path, F_OK) == -1) path = "/lib/x86_64-linux-gnu/libc.so.6";
    int fd = open(path, O_RDONLY);
    // 映射长度 2MB，确保偏移量 0x164ac0 在范围内
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0;

    printf("[Receiver] 硬件校准完成，准备接收数据...\n");

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;

        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;

        if (shift_reg == SYNC_HEADER) {
            printf("\n[收到消息]: ");
            
            // 2. 关键修改：相位补偿
            // 检测到同步头结束时，我们处于“同步位”的末尾。
            // 多等 500us（半个位时间），让接下来的采样点落在数据位的“正中央”。
            usleep(BIT_INTERVAL / 2); 

            while(1) {
                uint8_t current_char = 0;
                for (int i = 0; i < 8; i++) {
                    // 等待一个完整的位间隔
                    usleep(BIT_INTERVAL); 
                    uint64_t rt = reload_t(addr);
                    int b = (rt < THRESHOLD) ? 1 : 0;
                    current_char = (current_char << 1) | b;
                }
                
                // 结束符判断：如果读到 \0 或非打印字符则退出
                if (current_char == 0 || current_char > 126) break; 
                printf("%c", current_char);
                fflush(stdout);
            }
            shift_reg = 0; // 重置，寻找下一个同步头
            printf("\n");
        }
        usleep(BIT_INTERVAL); 
    }
    return 0;
}