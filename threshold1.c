#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <math.h>
#include <time.h>

#define REPORT_INTERVAL 100000

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

typedef struct {
    uint64_t count;
    double sum;
    long double sum_sq;
} timing_stats;

void update_stats(timing_stats *stats, uint64_t t) {
    stats->count++;
    stats->sum += t;
    stats->sum_sq += (long double)t * t;
}

void print_stats(const char *label, timing_stats *stats) {
    if (stats->count == 0) return;
    double avg = stats->sum / stats->count;
    double variance = (stats->sum_sq / stats->count) - (avg * avg);
    double stddev = sqrt(variance);
    printf("%s: avg=%.2f cycles std=%.2f\n", label, avg, stddev);
}

int main() {
    printf("[Threshold] Starting...\n");

    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    void *addr = dlsym(handle, "ecvt_r");

    printf("addr = %p\n", addr);

    timing_stats flushed = {0}, cached = {0};
    srand(time(NULL));

    while (1) {
        int do_flush = rand() % 2;

        if (do_flush) {
            _mm_clflush(addr);
        }

        _mm_mfence();

        double pi = 3.14;
        int decpt, sign;
        char buf[64];

        uint64_t start = rdtscp64();
        ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
        uint64_t end = rdtscp64();

        uint64_t t = end - start;

        if (do_flush)
            update_stats(&flushed, t);
        else
            update_stats(&cached, t);

        if ((flushed.count + cached.count) % REPORT_INTERVAL == 0) {
            printf("\n--- stats ---\n");
            print_stats("Flushed", &flushed);
            print_stats("Cached", &cached);
        }
    }
}