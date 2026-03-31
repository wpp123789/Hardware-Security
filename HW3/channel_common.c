#define _GNU_SOURCE
#include "channel_common.h"

#include <dlfcn.h>
#include <errno.h>
#include <getopt.h>
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_int_arg(const char *arg, const char *name, int min_value) {
    char *end = NULL;
    long v = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || v < min_value || v > 100000000L) {
        fprintf(stderr, "Invalid %s value: '%s'\n", name, arg);
        return -1;
    }
    return (int)v;
}

static int parse_double_arg(const char *arg, const char *name, double min_v, double max_v, double *out) {
    char *end = NULL;
    double v = strtod(arg, &end);
    if (end == arg || *end != '\0' || v < min_v || v > max_v) {
        fprintf(stderr, "Invalid %s value: '%s'\n", name, arg);
        return -1;
    }
    *out = v;
    return 0;
}

void ch_default_config(ch_config_t *cfg) {
    if (!cfg) {
        return;
    }
    cfg->library_name = CH_DEFAULT_LIBRARY;
    cfg->symbol_name = CH_DEFAULT_SYMBOL;
    cfg->threshold_cycles = 170;
    cfg->bit_us = 3000;
    cfg->probes_per_bit = 5;
    cfg->hit_ratio = 0.55;
}

void ch_print_common_usage(FILE *out) {
    fprintf(out, "Common options:\n");
    fprintf(out, "  --lib <name>           Shared library to resolve symbol from (default: %s)\n", CH_DEFAULT_LIBRARY);
    fprintf(out, "  --symbol <name>        Symbol used as shared probe target (default: %s)\n", CH_DEFAULT_SYMBOL);
    fprintf(out, "  --threshold <cycles>   Hit/miss split in cycles (default: 170)\n");
    fprintf(out, "  --bit-us <us>          Bit slot duration in microseconds (default: 3000)\n");
    fprintf(out, "  --probes <n>           Probes per bit for receiver (default: 5)\n");
    fprintf(out, "  --hit-ratio <0..1>     Fraction of probes required for bit=1 (default: 0.55)\n");
}

int ch_parse_common_args(int argc, char **argv, ch_config_t *cfg) {
    static const struct option long_opts[] = {
        {"lib", required_argument, NULL, 1001},
        {"symbol", required_argument, NULL, 1002},
        {"threshold", required_argument, NULL, 1003},
        {"bit-us", required_argument, NULL, 1004},
        {"probes", required_argument, NULL, 1005},
        {"hit-ratio", required_argument, NULL, 1006},
        {0, 0, 0, 0}
    };

    int idx = 0;
    optind = 1;
    while (1) {
        int c = getopt_long(argc, argv, "", long_opts, &idx);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 1001:
                cfg->library_name = optarg;
                break;
            case 1002:
                cfg->symbol_name = optarg;
                break;
            case 1003: {
                int v = parse_int_arg(optarg, "threshold", 1);
                if (v < 0) {
                    return -1;
                }
                cfg->threshold_cycles = v;
                break;
            }
            case 1004: {
                int v = parse_int_arg(optarg, "bit-us", 10);
                if (v < 0) {
                    return -1;
                }
                cfg->bit_us = v;
                break;
            }
            case 1005: {
                int v = parse_int_arg(optarg, "probes", 1);
                if (v < 0) {
                    return -1;
                }
                cfg->probes_per_bit = v;
                break;
            }
            case 1006:
                if (parse_double_arg(optarg, "hit-ratio", 0.01, 1.0, &cfg->hit_ratio) != 0) {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
    return 0;
}

int ch_init_target(ch_target_t *target, const ch_config_t *cfg) {
    if (!target || !cfg) {
        fprintf(stderr, "Internal error: null target/config\n");
        return -1;
    }
    memset(target, 0, sizeof(*target));
    target->cfg = *cfg;

    target->dl_handle = dlopen(cfg->library_name, RTLD_LAZY | RTLD_LOCAL);
    if (!target->dl_handle) {
        fprintf(stderr, "dlopen('%s') failed: %s\n", cfg->library_name, dlerror());
        return -1;
    }
    dlerror();
    target->target_addr = dlsym(target->dl_handle, cfg->symbol_name);
    const char *err = dlerror();
    if (err || !target->target_addr) {
        fprintf(stderr, "dlsym('%s') failed: %s\n", cfg->symbol_name, err ? err : "unknown");
        ch_destroy_target(target);
        return -1;
    }
    return 0;
}

void ch_destroy_target(ch_target_t *target) {
    if (!target) {
        return;
    }
    if (target->dl_handle) {
        dlclose(target->dl_handle);
        target->dl_handle = NULL;
    }
    target->target_addr = NULL;
}

uint64_t ch_rdtscp64(void) {
    unsigned lo = 0;
    unsigned hi = 0;
    unsigned aux = 0;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) ::);
    (void)aux;
    return ((uint64_t)hi << 32) | lo;
}

uint64_t ch_measure_access_cycles(void *addr) {
    volatile unsigned char *p = (volatile unsigned char *)addr;
    _mm_lfence();
    uint64_t t0 = ch_rdtscp64();
    (void)*p;
    _mm_lfence();
    uint64_t t1 = ch_rdtscp64();
    return t1 - t0;
}

void ch_flush(void *addr) {
    _mm_clflush((const void *)addr);
    _mm_mfence();
}

void ch_access_burst(void *addr, int iters) {
    volatile unsigned char *p = (volatile unsigned char *)addr;
    for (int i = 0; i < iters; i++) {
        (void)*p;
    }
    _mm_mfence();
}

uint8_t ch_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00u;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

unsigned ch_hamming32(uint32_t a, uint32_t b) {
    uint32_t x = a ^ b;
    unsigned count = 0;
    while (x) {
        x &= (x - 1u);
        count++;
    }
    return count;
}
