#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h> // for _mm_clflush , __rdtscp
#include <math.h>
#include <time.h>

#define REPORT_INTERVAL 100000 // how often to report stats

// Read timestamp counter
static inline uint64_t rdtscp64 () {
    unsigned aux;
    return __rdtscp (&aux);
}

typedef struct {
    uint64_t count;
    double sum;
    long double sum_sq;
} timing_stats;

void update_stats(timing_stats *stats , uint64_t t) {
    stats ->count ++;
    stats ->sum += t;
    stats ->sum_sq += (long double) t * t;
}

void print_stats(const char *label , timing_stats *stats) {
    if (stats ->count == 0) return;
    double avg = stats ->sum / stats ->count;
    double variance = (stats ->sum_sq / stats ->count) - (avg * avg);
    double stddev = sqrt(variance);
    fprintf(stderr , "%s: count=%lu avg =%.2f cycles stddev =%.2f cycles\n",
            label , stats ->count , avg , stddev);
    fflush(stderr);
}

int main(void) {
    // 1. 获取地址逻辑保持不变
    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *libc_fn = dlsym(handle, "ecvt_r"); 
    dlclose(handle);

    // 2. 将所有变量定义移出循环，防止栈操作干扰
    uint64_t iterations = 0;
    timing_stats flushed = {0}, nonflushed = {0};
    uint64_t start, end, t;
    int do_flush;
    volatile uint8_t dummy; // 用于强制读取

    printf("[Receiver] 正在进行纯净内存测试，地址: %p\n", libc_fn);

    while (1) {
        do_flush = rand() % 2;

        if (do_flush) {
            // 只清空我们要测的那一个缓存行（64字节）
            _mm_clflush(libc_fn);
        }
        
        // 关键：给 CPU 一点时间处理 clflush
        _mm_mfence();

        // --- 核心测量区：除了计时和读取，什么都不放 ---
        _mm_lfence(); // 保证之前的指令全部结束
        start = rdtscp64();

        dummy = *(volatile uint8_t *)libc_fn; 

        _mm_lfence(); // 保证读取结束后再停表
        end = rdtscp64();
        // ------------------------------------------

        t = end - start;

        if (do_flush)
            update_stats(&flushed, t);
        else
            update_stats(&nonflushed, t);

        if (++iterations % REPORT_INTERVAL == 0) {
            fprintf(stderr, "\n--- Stats after %lu iterations ---\n", iterations);
            print_stats("Flushed (Miss)", &flushed);
            print_stats("Non-Flushed (Hit)", &nonflushed);
            
            // 建议的阈值计算
            double hit_avg = nonflushed.sum / nonflushed.count;
            double miss_avg = flushed.sum / flushed.count;
            fprintf(stderr, ">>> 建议 THRESHOLD: %.0f\n", (hit_avg + miss_avg) / 2);
        }
    }
    return 0;
}