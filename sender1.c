#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>

void send_bit(void *addr, int bit) {
    if (bit == 1) {
        //  发送1：通过多次访问确保该地址进入缓存
        for (int i = 0; i < 200; i++) {
            *(volatile uint8_t *)addr;
        }
    } else {
        // 发送0：不做任何操作，让其保持被清空状态
        _mm_clflush(addr);
    }
    _mm_mfence();
    usleep(500); // 必须与 Receiver 的采样频率严格对齐
}

int main() {
    int fd = open("/lib64/libc.so.6", O_RDONLY);
    void *addr = mmap(NULL, 64, PROT_READ, MAP_SHARED, fd, 0x164ac0);
    
    uint32_t sync = 0x3F2; 
    printf("[Sender] 正在发送同步信号...\n");

    while (1) {
        // 循环发送同步头
        for (int i = 9; i >= 0; i--) {
            send_bit(addr, (sync >> i) & 1);
        }
        usleep(10000); // 间隔
    }
    return 0;
}