#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <time.h>

#define BIT_US      5000
#define ECVT_OFF    0x138ac0
#define MAP_SIZE    0x200000
#define THRESHOLD   100
#define PERIOD_US   6000
#define SAMPLE_OFFSET_US 1500

static uint64_t now_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

static int raw_sample(void *addr) {
    _mm_mfence();
    unsigned aux;
    uint64_t t0 = __rdtscp(&aux);
    *(volatile char *)addr;
    uint64_t t1 = __rdtscp(&aux);
    _mm_mfence();
    return (t1 - t0 < THRESHOLD) ? 1 : 0;
}

static int sample_at(void *addr, uint64_t target_us) {
    uint64_t now = now_us();
    if (target_us > now) usleep(target_us - now);
    return raw_sample(addr);
}

int main() {
    printf("[Receiver] Starting... threshold=%d PERIOD=%d OFFSET=%d\n",
           THRESHOLD, PERIOD_US, SAMPLE_OFFSET_US);

    int fd = open("/usr/lib64/libc-2.28.so", O_RDONLY);
    if (fd < 0) fd = open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    void *map = mmap(NULL, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }
    void *addr = (char*)map + ECVT_OFF;

    printf("[Receiver] addr = %p\n", addr);
    printf("[Receiver] Waiting for sync...\n");
    fflush(stdout);

    while (1) {
        int hit_count = 0;
        uint64_t last_hit_time = 0;
        while (hit_count < 8) {
            usleep(PERIOD_US / 2);
            if (raw_sample(addr) == 1) {
                hit_count++;
                last_hit_time = now_us();
            } else {
                hit_count = 0;
            }
        }

        uint64_t bit_start = last_hit_time - SAMPLE_OFFSET_US + PERIOD_US;

        int found_zero = 0;
        for (int i = 0; i < 20; i++) {
            int bit = sample_at(addr, bit_start + SAMPLE_OFFSET_US);
            bit_start += PERIOD_US;
            if (bit == 0) { found_zero = 1; break; }
        }
        if (!found_zero) continue;

        printf("[Receiver] Synced! Reading message: ");
        fflush(stdout);

        while (1) {
            uint8_t c = 0;
            for (int i = 7; i >= 0; i--) {
                int bit = sample_at(addr, bit_start + SAMPLE_OFFSET_US);
                bit_start += PERIOD_US;
                c |= (bit << i);
            }
            if (c == 0x00) { printf("\n[Receiver] Done.\n"); fflush(stdout); break; }
            if (c >= 0x20 && c <= 0x7E) printf("%c", c);
            else printf("[0x%02X]", c);
            fflush(stdout);
        }
    }
    return 0;
}
