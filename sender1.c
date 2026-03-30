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
        // 强化信号：循环 100 次纯内存读取
        for (int i = 0; i < 100; i++) {
            *(volatile uint8_t *)addr;
            _mm_mfence();
        }
    } else {
        // 发送 0：彻底清除
        _mm_clflush(addr);
    }
    _mm_mfence();
    usleep(1000); // 必须是 1000，不要改小
}
// 发送一个字节（8位）
void send_byte(void *addr, uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        send_bit(addr, (byte >> i) & 1);
    }
}

int main() {
    const char *path = "/lib64/libc.so.6";
    if (access(path, F_OK) == -1) path = "/lib/x86_64-linux-gnu/libc.so.6";
    int fd = open(path, O_RDONLY);
    void *map_base = mmap(NULL, 2*1024*1024, PROT_READ, MAP_SHARED, fd, 0);
    void *addr = (uint8_t *)map_base + 0x164ac0;

    uint32_t sync_val = 0x3F2; // 10位同步头
    char *message = "Hello!HW3"; // 要发送的内容

    printf("[Sender] 开始传输字符串: %s\n", message);

    while (1) {
        // 1. 发送同步头
        for (int i = 9; i >= 0; i--) {
            send_bit(addr, (sync_val >> i) & 1);
        }

        // 2. 发送字符串内容
        for (int i = 0; i < strlen(message); i++) {
            send_byte(addr, (uint8_t)message[i]);
        }

        // 3. 发送一个结束符 \0 
        send_byte(addr, 0);

        usleep(50000); // 每一轮发送后的长休息
    }
    return 0;
}