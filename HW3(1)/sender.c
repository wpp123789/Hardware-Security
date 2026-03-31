#include "common.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

typedef struct {
    const char *message;
    uint32_t repeat;
    uint32_t bit_us;
    uint32_t gap_us;
    const char *lib_path;
    const char *symbol;
    int verbose;
} sender_cfg_t;

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --message <text> [options]\n"
            "Options:\n"
            "  --repeat <N>       frame repetitions (default: 1)\n"
            "  --bit-us <us>      bit slot duration in us (default: 500)\n"
            "  --gap-us <us>      inter-frame gap in us (default: 20000)\n"
            "  --gap-ms <ms>      inter-frame gap in ms (alternative)\n"
            "  --lib <path>       shared library (default: %s)\n"
            "  --symbol <name>    symbol name (default: %s)\n"
            "  --verbose          print per-frame stats\n",
            prog, DEFAULT_LIB, DEFAULT_SYMBOL);
}

static int parse_args(int argc, char **argv, sender_cfg_t *cfg) {
    static struct option long_opts[] = {
        {"message", required_argument, 0, 'm'},
        {"repeat", required_argument, 0, 'r'},
        {"bit-us", required_argument, 0, 'b'},
        {"gap-us", required_argument, 0, 'g'},
        {"gap-ms", required_argument, 0, 'G'},
        {"lib", required_argument, 0, 'l'},
        {"symbol", required_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };
    int opt;
    uint32_t tmp_u32;

    cfg->message = NULL;
    cfg->repeat = 1;
    cfg->bit_us = 500;
    cfg->gap_us = 20000;
    cfg->lib_path = DEFAULT_LIB;
    cfg->symbol = DEFAULT_SYMBOL;
    cfg->verbose = 0;

    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'm':
                cfg->message = optarg;
                break;
            case 'r':
                if (parse_u32(optarg, &cfg->repeat) != 0 || cfg->repeat == 0) {
                    return -1;
                }
                break;
            case 'b':
                if (parse_u32(optarg, &cfg->bit_us) != 0 || cfg->bit_us == 0) {
                    return -1;
                }
                break;
            case 'g':
                if (parse_u32(optarg, &cfg->gap_us) != 0) {
                    return -1;
                }
                break;
            case 'G':
                if (parse_u32(optarg, &tmp_u32) != 0) {
                    return -1;
                }
                cfg->gap_us = tmp_u32 * 1000u;
                break;
            case 'l':
                cfg->lib_path = optarg;
                break;
            case 's':
                cfg->symbol = optarg;
                break;
            case 'v':
                cfg->verbose = 1;
                break;
            default:
                return -1;
        }
    }
    return cfg->message ? 0 : -1;
}

static void tx_bit(int bit, volatile uint8_t *addr, uint64_t slot_end_us) {
    if (bit) {
        while (now_us() < slot_end_us) {
            touch_addr(addr);
        }
    } else {
        sleep_until_us(slot_end_us);
    }
}

int main(int argc, char **argv) {
    sender_cfg_t cfg;
    target_t target = {0};
    const uint8_t *payload;
    uint16_t payload_len;
    uint8_t checksum;
    uint64_t total_start;
    uint64_t total_end;
    uint64_t total_bits = 0;
    uint32_t frame_idx;
    uint32_t frame_bits;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    payload = (const uint8_t *)cfg.message;
    if (strlen(cfg.message) > MAX_PAYLOAD_LEN) {
        fprintf(stderr, "message too long (max %u bytes)\n", MAX_PAYLOAD_LEN);
        return 1;
    }
    payload_len = (uint16_t)strlen(cfg.message);
    checksum = xor_checksum(payload, payload_len);
    frame_bits = 32u + 16u + (uint32_t)payload_len * 8u + 8u;

    if (resolve_target(cfg.lib_path, cfg.symbol, &target) != 0) {
        return 1;
    }

    printf("sender target: %s:%s @ %p\n", cfg.lib_path, cfg.symbol, (void *)target.addr);
    printf("sending %u frame(s), payload_len=%u bytes, bit_us=%u\n",
           cfg.repeat, (unsigned)payload_len, cfg.bit_us);

    total_start = now_us();
    for (frame_idx = 0; frame_idx < cfg.repeat; ++frame_idx) {
        uint64_t frame_start = now_us();
        uint64_t slot_end = frame_start;
        int bit;
        uint32_t i;

        if (frame_idx > 0 && cfg.gap_us > 0) {
            sleep_until_us(frame_start + cfg.gap_us);
            frame_start = now_us();
            slot_end = frame_start;
        }

        for (bit = 31; bit >= 0; --bit) {
            slot_end += cfg.bit_us;
            tx_bit((SYNC_WORD >> bit) & 1u, target.addr, slot_end);
        }

        for (bit = 15; bit >= 0; --bit) {
            slot_end += cfg.bit_us;
            tx_bit((payload_len >> bit) & 1u, target.addr, slot_end);
        }

        for (i = 0; i < payload_len; ++i) {
            int b;
            for (b = 7; b >= 0; --b) {
                slot_end += cfg.bit_us;
                tx_bit((payload[i] >> b) & 1u, target.addr, slot_end);
            }
        }

        for (bit = 7; bit >= 0; --bit) {
            slot_end += cfg.bit_us;
            tx_bit((checksum >> bit) & 1u, target.addr, slot_end);
        }

        total_bits += frame_bits;
        if (cfg.verbose) {
            printf("frame %u/%u done in %.3f ms\n",
                   frame_idx + 1,
                   cfg.repeat,
                   (double)(slot_end - frame_start) / 1000.0);
        }
    }
    total_end = now_us();

    close_target(&target);

    {
        double seconds = (double)(total_end - total_start) / 1000000.0;
        double bps = seconds > 0.0 ? (double)total_bits / seconds : 0.0;
        printf("finished: total_bits=%" PRIu64 ", total_time=%.3f s, throughput=%.2f bit/s\n",
               total_bits, seconds, bps);
    }

    return 0;
}
