#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <math.h>
#include <time.h>

#define THRESHOLD_CYCLES 2250

static inline uint64_t rdtscp64() {
    unsigned aux;
    return __rdtscp(&aux);
}

int main(int argc, char *argv[]) {

    int threshold = THRESHOLD_CYCLES;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && (i + 1) < argc) {
            threshold = atoi(argv[++i]);
        }
    }
    printf("[Receiver] Starting up...\n");
    fflush(stdout);

    // Load ecvt_r address from libc (same shared library as sender)
    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "[Receiver] dlopen failed\n");
        return 1;
    }
    void *libc_fn = dlsym(handle, "ecvt_r");
    if (!libc_fn) {
        fprintf(stderr, "[Receiver] dlsym failed\n");
        return 1;
    }
    printf("[Receiver] libc address = %p\n", libc_fn);
    fflush(stdout);
    dlclose(handle);

    double pi = 3.141592653589793;
    int decpt = 0, sign = 0;
    char buf[64];
    uint64_t start, end;

    while (1) {
        start = rdtscp64();
        int s = ecvt_r(pi, 20, &decpt, &sign, buf, sizeof(buf));
        (void)s;
        end = rdtscp64();
        uint64_t t = end - start;

        int bit = (t < threshold) ? 1 : 0;
        printf("%d", bit);
        fflush(stdout);
    }

    return 0;
}