#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define THRESHOLD 130 
#define SYNC_HEADER 0x3F2

static inline uint64_t reload_t(void *addr) {
    uint64_t start, end;
    unsigned aux;
    start = __rdtscp(&aux);
    *(volatile uint8_t *)addr; 
    _mm_mfence();
    end = __rdtscp(&aux);
    _mm_clflush(addr); 
    return end - start;
}

int main() {
    const char *path = "/lib64/libc.so.6";
    if (access(path, F_OK) == -1) path = "/lib/x86_64-linux-gnu/libc.so.6";
    int fd = open(path, O_RDONLY);
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0;

    printf("[Receiver] 准备接收数据...\n");

    uint32_t shift_reg = 0;
    while (1) {
        uint64_t t = reload_t(addr);
        int bit = (t < THRESHOLD) ? 1 : 0;

        shift_reg = ((shift_reg << 1) | bit) & 0x3FF;

        // 检查是否匹配同步头
        if (shift_reg == SYNC_HEADER) {
            printf("\n[收到消息]: ");
            
            // 同步后，开始抓取接下来的字符
            while(1) {
                uint8_t current_char = 0;
                for (int i = 0; i < 8; i++) {
                    usleep(500); // 与发送端间隔一致
                    uint64_t rt = reload_t(addr);
                    int b = (rt < THRESHOLD) ? 1 : 0;
                    current_char = (current_char << 1) | b;
                }
                
                if (current_char == 0 || current_char > 126) break; // 结束符或乱码退出
                printf("%c", current_char);
                fflush(stdout);
            }
            shift_reg = 0; // 重置寄存器，准备下一次同步
        }
        usleep(500); 
    }
    return 0;
}