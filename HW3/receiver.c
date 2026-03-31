#define _GNU_SOURCE
#include "channel_common.h"

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int sync_tolerance;
    int max_frames;
    int quiet;
} receiver_cfg_t;

static int parse_i(const char *s, int min_value, int max_value, const char *name) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < min_value || v > max_value) {
        fprintf(stderr, "Invalid %s value: %s\n", name, s);
        return -1;
    }
    return (int)v;
}

static double parse_d(const char *s, double min_value, double max_value, const char *name) {
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || v < min_value || v > max_value) {
        fprintf(stderr, "Invalid %s value: %s\n", name, s);
        return -1.0;
    }
    return v;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Receiver options:\n");
    fprintf(stderr, "  --sync-tolerance <n>   Max sync-bit mismatches in 32-bit sync word (default: 4)\n");
    fprintf(stderr, "  --max-frames <n>       Exit after n valid frames, 0 = infinite (default: 0)\n");
    fprintf(stderr, "  --quiet                Print only decoded payloads\n");
    fprintf(stderr, "  --help                 Show this help\n");
    ch_print_common_usage(stderr);
}

static int parse_args(int argc, char **argv, receiver_cfg_t *rcfg, ch_config_t *ccfg) {
    static const struct option long_opts[] = {
        {"sync-tolerance", required_argument, NULL, 2001},
        {"max-frames", required_argument, NULL, 2002},
        {"quiet", no_argument, NULL, 2003},
        {"help", no_argument, NULL, 2004},
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
                rcfg->sync_tolerance = parse_i(optarg, 0, 31, "sync-tolerance");
                if (rcfg->sync_tolerance < 0) return -1;
                break;
            case 2002:
                rcfg->max_frames = parse_i(optarg, 0, 10000000, "max-frames");
                if (rcfg->max_frames < 0) return -1;
                break;
            case 2003:
                rcfg->quiet = 1;
                break;
            case 2004:
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
                ccfg->hit_ratio = parse_d(optarg, 0.01, 1.0, "hit-ratio");
                if (ccfg->hit_ratio < 0.0) return -1;
                break;
            default:
                usage(argv[0]);
                return -1;
        }
    }
    return 0;
}

static uint64_t monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int sample_bit(ch_target_t *target) {
    int hits = 0;
    uint64_t slot_ns = (uint64_t)target->cfg.bit_us * 1000ULL;
    uint64_t end_ns = monotonic_ns() + slot_ns;
    int probes = target->cfg.probes_per_bit;

    for (int i = 0; i < probes; i++) {
        uint64_t t = ch_measure_access_cycles(target->target_addr);
        if ((int)t <= target->cfg.threshold_cycles) {
            hits++;
        }
        ch_flush(target->target_addr);

        if (i + 1 < probes) {
            uint64_t now = monotonic_ns();
            uint64_t remain = (now < end_ns) ? (end_ns - now) : 0;
            unsigned us = (unsigned)((remain / (uint64_t)(probes - i)) / 1000ULL);
            if (us > 0) {
                usleep(us);
            }
        }
    }

    uint64_t now = monotonic_ns();
    if (now < end_ns) {
        usleep((useconds_t)((end_ns - now) / 1000ULL));
    }
    return ((double)hits / (double)probes) >= target->cfg.hit_ratio ? 1 : 0;
}

static int recv_byte(ch_target_t *target, uint8_t *out) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        b |= (uint8_t)(sample_bit(target) << i);
    }
    *out = b;
    return 0;
}

static int recv_u16(ch_target_t *target, uint16_t *out) {
    uint16_t v = 0;
    for (int i = 0; i < 16; i++) {
        v |= (uint16_t)(sample_bit(target) << i);
    }
    *out = v;
    return 0;
}

static int sync_to_frame(ch_target_t *target, int tolerance, ch_receiver_stats_t *stats) {
    uint32_t shift = 0;
    for (;;) {
        int bit = sample_bit(target);
        shift = (shift >> 1) | ((uint32_t)(bit & 1) << 31);
        stats->bits_seen++;
        stats->sync_attempts++;
        if (ch_hamming32(shift, CH_SYNC_WORD) <= (unsigned)tolerance) {
            return 0;
        }
    }
}

static void print_payload(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned c = buf[i];
        if (c >= 0x20 && c <= 0x7e) {
            putchar((int)c);
        } else {
            printf("\\x%02x", c);
        }
    }
}

int main(int argc, char **argv) {
    ch_config_t ccfg;
    receiver_cfg_t rcfg;
    ch_target_t target;
    ch_receiver_stats_t stats;

    ch_default_config(&ccfg);
    rcfg.sync_tolerance = 4;
    rcfg.max_frames = 0;
    rcfg.quiet = 0;
    memset(&stats, 0, sizeof(stats));

    {
        int pr = parse_args(argc, argv, &rcfg, &ccfg);
        if (pr != 0) {
            return pr < 0 ? 1 : 0;
        }
    }

    if (ch_init_target(&target, &ccfg) != 0) {
        return 1;
    }

    if (!rcfg.quiet) {
        printf("[receiver] target=%p lib=%s symbol=%s bit_us=%d threshold=%d probes=%d hit_ratio=%.2f\n",
               target.target_addr, ccfg.library_name, ccfg.symbol_name, ccfg.bit_us,
               ccfg.threshold_cycles, ccfg.probes_per_bit, ccfg.hit_ratio);
        fflush(stdout);
    }

    while (rcfg.max_frames == 0 || (int)stats.frames_ok < rcfg.max_frames) {
        uint16_t len = 0;
        uint16_t end = 0;
        uint8_t crc = 0;
        uint8_t payload[CH_MAX_PAYLOAD];

        sync_to_frame(&target, rcfg.sync_tolerance, &stats);
        recv_u16(&target, &len);
        if (len > CH_MAX_PAYLOAD) {
            stats.frames_bad_length++;
            continue;
        }

        for (uint16_t i = 0; i < len; i++) {
            recv_byte(&target, &payload[i]);
        }
        recv_byte(&target, &crc);
        recv_u16(&target, &end);

        if (end != CH_END_WORD) {
            stats.frames_bad_format++;
            continue;
        }
        if (ch_crc8(payload, len) != crc) {
            stats.frames_bad_crc++;
            continue;
        }

        stats.frames_ok++;
        if (rcfg.quiet) {
            print_payload(payload, len);
            putchar('\n');
        } else {
            printf("[receiver] frame=%" PRIu64 " len=%u text=\"", stats.frames_ok, len);
            print_payload(payload, len);
            printf("\" crc=ok bits_seen=%" PRIu64 "\n", stats.bits_seen);
            fflush(stdout);
        }
    }

    if (!rcfg.quiet) {
        printf("[receiver] done ok=%" PRIu64 " bad_crc=%" PRIu64 " bad_fmt=%" PRIu64 " bad_len=%" PRIu64 "\n",
               stats.frames_ok, stats.frames_bad_crc, stats.frames_bad_format, stats.frames_bad_length);
    }
    ch_destroy_target(&target);
    return 0;
}
