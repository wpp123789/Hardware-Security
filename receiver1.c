#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#define THRESHOLD 96
#define SYNC_HEADER 0x3F2
// 建议将位间隔提高到 1000us (1ms) 以增加作业的可靠性
#define BIT_INTERVAL 1000 


static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    _mm_lfence();
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr; // 纯内存读取
    _mm_lfence();
    end = __rdtscp(&aux);
    _mm_clflush(addr); // 采样后立刻清除，等待 Sender 下一次加载
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
            fflush(stdout);
            // 2. 关键修改：相位补偿
            // 检测到同步头结束时，我们处于“同步位”的末尾。
            // 多等 500us（半个位时间），让接下来的采样点落在数据位的“正中央”。
            usleep(BIT_INTERVAL+(BIT_INTERVAL/2)); 

            while(1) {
                uint8_t c = 0;
                for (int i = 0; i < 8; i++) {
                    // 等待一个完整的位间隔
                    uint64_t t = reload_t(addr);
                    c=(c<<1) | ((t < THRESHOLD) ? 1 : 0);
                    usleep(BIT_INTERVAL); // 等待下一个位间隔
                   
                }
                
                // 结束符判断：如果读到 \0 或非打印字符则退出
                if (c == 0 || c > 126) break; 
                printf("%c", c);
                fflush(stdout);
            }
            shift_reg = 0; // 重置，寻找下一个同步头
           
        }
        usleep(BIT_INTERVAL); 
    }
    return 0;
}