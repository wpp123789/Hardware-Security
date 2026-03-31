#include "common.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

typedef struct {
    uint32_t samples;
    const char *lib_path;
    const char *symbol;
} calib_cfg_t;

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "Options:\n"
            "  --samples <N>    number of samples per class (default: 50000)\n"
            "  --lib <path>     shared library (default: %s)\n"
            "  --symbol <name>  symbol name (default: %s)\n",
            prog, DEFAULT_LIB, DEFAULT_SYMBOL);
}

static int parse_args(int argc, char **argv, calib_cfg_t *cfg) {
    static struct option long_opts[] = {
        {"samples", required_argument, 0, 'n'},
        {"lib", required_argument, 0, 'l'},
        {"symbol", required_argument, 0, 's'},
        {0, 0, 0, 0},
    };
    int opt;

    cfg->samples = 50000;
    cfg->lib_path = DEFAULT_LIB;
    cfg->symbol = DEFAULT_SYMBOL;

    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'n':
                if (parse_u32(optarg, &cfg->samples) != 0 || cfg->samples == 0) {
                    return -1;
                }
                break;
            case 'l':
                cfg->lib_path = optarg;
                break;
            case 's':
                cfg->symbol = optarg;
                break;
            default:
                return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    calib_cfg_t cfg;
    target_t target = {0};
    uint64_t i;
    uint64_t hit_sum = 0;
    uint64_t miss_sum = 0;
    uint32_t hit_min = UINT32_MAX;
    uint32_t hit_max = 0;
    uint32_t miss_min = UINT32_MAX;
    uint32_t miss_max = 0;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    if (resolve_target(cfg.lib_path, cfg.symbol, &target) != 0) {
        return 1;
    }

    printf("calibration target: %s:%s @ %p\n", cfg.lib_path, cfg.symbol, (void *)target.addr);
    printf("samples: %u per class\n", cfg.samples);

    for (i = 0; i < cfg.samples; ++i) {
        uint32_t t;

        touch_addr(target.addr);
        t = reload_time_cycles(target.addr);
        hit_sum += t;
        if (t < hit_min) hit_min = t;
        if (t > hit_max) hit_max = t;

        flush_addr(target.addr);
        _mm_mfence();
        t = reload_time_cycles(target.addr);
        miss_sum += t;
        if (t < miss_min) miss_min = t;
        if (t > miss_max) miss_max = t;
    }

    close_target(&target);

    {
        double hit_avg = (double)hit_sum / (double)cfg.samples;
        double miss_avg = (double)miss_sum / (double)cfg.samples;
        uint32_t threshold = (uint32_t)((hit_avg + miss_avg) / 2.0);

        printf("hit   : avg=%.2f min=%u max=%u cycles\n", hit_avg, hit_min, hit_max);
        printf("miss  : avg=%.2f min=%u max=%u cycles\n", miss_avg, miss_min, miss_max);
        printf("recommended threshold: %u cycles\n", threshold);
    }

    return 0;
}
