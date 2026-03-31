#define _GNU_SOURCE
#include "channel_common.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int samples;
} threshold_cfg_t;

static int cmp_u64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static uint64_t percentile_u64(const uint64_t *arr, int n, double p) {
    int idx = (int)((p * (double)(n - 1)) + 0.5);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return arr[idx];
}

static int parse_i(const char *s, int min_value, int max_value, const char *name) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < min_value || v > max_value) {
        fprintf(stderr, "Invalid %s value: %s\n", name, s);
        return -1;
    }
    return (int)v;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Threshold options:\n");
    fprintf(stderr, "  --samples <n>          Samples per class (cached/missed), default 20000\n");
    fprintf(stderr, "  --help                 Show this help\n");
    ch_print_common_usage(stderr);
}

static int parse_args(int argc, char **argv, threshold_cfg_t *tcfg, ch_config_t *ccfg) {
    static const struct option long_opts[] = {
        {"samples", required_argument, NULL, 2001},
        {"help", no_argument, NULL, 2002},
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
            case 2001:
                tcfg->samples = parse_i(optarg, 100, 5000000, "samples");
                if (tcfg->samples < 0) return -1;
                break;
            case 2002:
                usage(argv[0]);
                return 1;
            case 1001:
                ccfg->library_name = optarg;
                break;
            case 1002:
                ccfg->symbol_name = optarg;
                break;
            case 1003:
                ccfg->threshold_cycles = parse_i(optarg, 1, 1000000, "threshold");
                if (ccfg->threshold_cycles < 0) return -1;
                break;
            case 1004:
                ccfg->bit_us = parse_i(optarg, 10, 2000000, "bit-us");
                if (ccfg->bit_us < 0) return -1;
                break;
            case 1005:
                ccfg->probes_per_bit = parse_i(optarg, 1, 1000, "probes");
                if (ccfg->probes_per_bit < 0) return -1;
                break;
            case 1006:
                ccfg->hit_ratio = strtod(optarg, NULL);
                break;
            default:
                usage(argv[0]);
                return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    ch_config_t ccfg;
    threshold_cfg_t tcfg;
    ch_target_t target;
    uint64_t *hits = NULL;
    uint64_t *misses = NULL;
    uint64_t p95_hit = 0, p05_miss = 0, p50_hit = 0, p50_miss = 0;
    int suggested = 0;

    ch_default_config(&ccfg);
    tcfg.samples = 20000;

    {
        int pr = parse_args(argc, argv, &tcfg, &ccfg);
        if (pr != 0) {
            return pr < 0 ? 1 : 0;
        }
    }

    if (ch_init_target(&target, &ccfg) != 0) {
        return 1;
    }

    hits = (uint64_t *)calloc((size_t)tcfg.samples, sizeof(uint64_t));
    misses = (uint64_t *)calloc((size_t)tcfg.samples, sizeof(uint64_t));
    if (!hits || !misses) {
        fprintf(stderr, "Allocation failure for %d samples\n", tcfg.samples);
        free(hits);
        free(misses);
        ch_destroy_target(&target);
        return 1;
    }

    printf("[threshold] target=%p lib=%s symbol=%s samples=%d\n",
           target.target_addr, ccfg.library_name, ccfg.symbol_name, tcfg.samples);

    for (int i = 0; i < tcfg.samples; i++) {
        ch_access_burst(target.target_addr, 64);
        hits[i] = ch_measure_access_cycles(target.target_addr);
    }
    for (int i = 0; i < tcfg.samples; i++) {
        ch_flush(target.target_addr);
        misses[i] = ch_measure_access_cycles(target.target_addr);
    }

    qsort(hits, (size_t)tcfg.samples, sizeof(uint64_t), cmp_u64);
    qsort(misses, (size_t)tcfg.samples, sizeof(uint64_t), cmp_u64);

    p50_hit = percentile_u64(hits, tcfg.samples, 0.50);
    p95_hit = percentile_u64(hits, tcfg.samples, 0.95);
    p50_miss = percentile_u64(misses, tcfg.samples, 0.50);
    p05_miss = percentile_u64(misses, tcfg.samples, 0.05);
    suggested = (int)((p95_hit + p05_miss) / 2u);

    printf("[threshold] cached median=%" PRIu64 " p95=%" PRIu64 "\n", p50_hit, p95_hit);
    printf("[threshold] flushed median=%" PRIu64 " p05=%" PRIu64 "\n", p50_miss, p05_miss);
    printf("[threshold] overlap_guard: cached_p95 <= threshold < flushed_p05\n");
    printf("[threshold] suggested threshold: %d cycles\n", suggested);
    printf("[threshold] use: ./receiver --threshold %d ... and ./sender --threshold %d ...\n",
           suggested, suggested);

    free(hits);
    free(misses);
    ch_destroy_target(&target);
    return 0;
}