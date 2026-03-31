#include "common.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

typedef enum {
    ST_SEARCH_SYNC = 0,
    ST_READ_LEN = 1,
    ST_READ_PAYLOAD = 2,
    ST_READ_CHECKSUM = 3
} rx_state_t;

typedef struct {
    uint32_t threshold;
    uint32_t bit_us;
    uint32_t probes;
    uint32_t sync_tolerance;
    uint32_t max_frames;
    const char *lib_path;
    const char *symbol;
    int quiet;
} receiver_cfg_t;

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --threshold <cycles> [options]\n"
            "Options:\n"
            "  --bit-us <us>           bit slot duration in us (default: 500)\n"
            "  --probes <N>            probes per slot (default: 5)\n"
            "  --sync-tolerance <N>    allowed sync hamming distance (default: 0)\n"
            "  --max-frames <N>        stop after N valid frames (0=infinite, default: 1)\n"
            "  --lib <path>            shared library (default: %s)\n"
            "  --symbol <name>         symbol name (default: %s)\n"
            "  --quiet                 print payload only on success\n",
            prog, DEFAULT_LIB, DEFAULT_SYMBOL);
}

static int parse_args(int argc, char **argv, receiver_cfg_t *cfg) {
    static struct option long_opts[] = {
        {"threshold", required_argument, 0, 't'},
        {"bit-us", required_argument, 0, 'b'},
        {"probes", required_argument, 0, 'p'},
        {"sync-tolerance", required_argument, 0, 'y'},
        {"max-frames", required_argument, 0, 'm'},
        {"lib", required_argument, 0, 'l'},
        {"symbol", required_argument, 0, 's'},
        {"quiet", no_argument, 0, 'q'},
        {0, 0, 0, 0},
    };
    int opt;
    int threshold_set = 0;

    cfg->threshold = 0;
    cfg->bit_us = 500;
    cfg->probes = 5;
    cfg->sync_tolerance = 0;
    cfg->max_frames = 1;
    cfg->lib_path = DEFAULT_LIB;
    cfg->symbol = DEFAULT_SYMBOL;
    cfg->quiet = 0;

    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            case 't':
                if (parse_u32(optarg, &cfg->threshold) != 0 || cfg->threshold == 0) {
                    return -1;
                }
                threshold_set = 1;
                break;
            case 'b':
                if (parse_u32(optarg, &cfg->bit_us) != 0 || cfg->bit_us == 0) {
                    return -1;
                }
                break;
            case 'p':
                if (parse_u32(optarg, &cfg->probes) != 0 || cfg->probes == 0) {
                    return -1;
                }
                break;
            case 'y':
                if (parse_u32(optarg, &cfg->sync_tolerance) != 0 || cfg->sync_tolerance > 32u) {
                    return -1;
                }
                break;
            case 'm':
                if (parse_u32(optarg, &cfg->max_frames) != 0) {
                    return -1;
                }
                break;
            case 'l':
                cfg->lib_path = optarg;
                break;
            case 's':
                cfg->symbol = optarg;
                break;
            case 'q':
                cfg->quiet = 1;
                break;
            default:
                return -1;
        }
    }
    return threshold_set ? 0 : -1;
}

static int rx_bit(volatile uint8_t *addr, uint32_t bit_us, uint32_t probes, uint32_t threshold) {
    uint64_t slot_start = now_us();
    uint64_t slot_end = slot_start + bit_us;
    uint32_t probe_period = bit_us / (probes + 1u);
    uint32_t votes = 0;
    uint32_t p;

    if (probe_period == 0) {
        probe_period = 1;
    }

    for (p = 0; p < probes; ++p) {
        uint64_t probe_time = slot_start + (uint64_t)(p + 1u) * probe_period;
        uint32_t t;
        flush_addr(addr);
        _mm_mfence();
        sleep_until_us(probe_time);
        t = reload_time_cycles(addr);
        if (t < threshold) {
            votes++;
        }
    }

    sleep_until_us(slot_end);
    return (votes > probes / 2u) ? 1 : 0;
}

int main(int argc, char **argv) {
    receiver_cfg_t cfg;
    target_t target = {0};
    rx_state_t state = ST_SEARCH_SYNC;
    uint32_t shift_reg = 0;
    uint16_t payload_len = 0;
    uint32_t len_bits = 0;
    uint8_t payload[MAX_PAYLOAD_LEN];
    uint32_t payload_bits = 0;
    uint8_t recv_checksum = 0;
    uint32_t checksum_bits = 0;
    uint64_t valid_frames = 0;
    uint64_t bad_frames = 0;
    uint64_t good_payload_bits = 0;
    uint64_t start_us;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    if (resolve_target(cfg.lib_path, cfg.symbol, &target) != 0) {
        return 1;
    }

    if (!cfg.quiet) {
        printf("receiver target: %s:%s @ %p\n", cfg.lib_path, cfg.symbol, (void *)target.addr);
        printf("threshold=%u cycles, bit_us=%u, probes=%u\n",
               cfg.threshold, cfg.bit_us, cfg.probes);
    }

    start_us = now_us();

    while (1) {
        int bit = rx_bit(target.addr, cfg.bit_us, cfg.probes, cfg.threshold);

        if (state == ST_SEARCH_SYNC) {
            shift_reg = (shift_reg << 1) | (uint32_t)bit;
            if (hamming_u32(shift_reg, SYNC_WORD) <= (int)cfg.sync_tolerance) {
                state = ST_READ_LEN;
                payload_len = 0;
                len_bits = 0;
                payload_bits = 0;
                recv_checksum = 0;
                checksum_bits = 0;
                if (!cfg.quiet) {
                    printf("sync found\n");
                }
            }
            continue;
        }

        if (state == ST_READ_LEN) {
            payload_len = (uint16_t)((payload_len << 1) | (uint16_t)bit);
            len_bits++;
            if (len_bits == 16) {
                if (payload_len > MAX_PAYLOAD_LEN) {
                    if (!cfg.quiet) {
                        printf("invalid length %u, resync\n", (unsigned)payload_len);
                    }
                    state = ST_SEARCH_SYNC;
                    shift_reg = 0;
                } else if (payload_len == 0) {
                    state = ST_READ_CHECKSUM;
                    recv_checksum = 0;
                    checksum_bits = 0;
                } else {
                    memset(payload, 0, payload_len);
                    state = ST_READ_PAYLOAD;
                }
            }
            continue;
        }

        if (state == ST_READ_PAYLOAD) {
            uint32_t byte_idx = payload_bits / 8u;
            payload[byte_idx] = (uint8_t)((payload[byte_idx] << 1) | (uint8_t)bit);
            payload_bits++;
            if (payload_bits == (uint32_t)payload_len * 8u) {
                state = ST_READ_CHECKSUM;
                recv_checksum = 0;
                checksum_bits = 0;
            }
            continue;
        }

        recv_checksum = (uint8_t)((recv_checksum << 1) | (uint8_t)bit);
        checksum_bits++;
        if (checksum_bits == 8) {
            uint8_t expected = xor_checksum(payload, payload_len);
            if (recv_checksum == expected) {
                valid_frames++;
                good_payload_bits += (uint64_t)payload_len * 8ull;
                if (cfg.quiet) {
                    fwrite(payload, 1, payload_len, stdout);
                    fputc('\n', stdout);
                } else {
                    printf("frame ok: len=%u checksum=0x%02x payload=\"",
                           (unsigned)payload_len, recv_checksum);
                    fwrite(payload, 1, payload_len, stdout);
                    printf("\"\n");
                }
                if (cfg.max_frames > 0 && valid_frames >= cfg.max_frames) {
                    break;
                }
            } else {
                bad_frames++;
                if (!cfg.quiet) {
                    printf("frame checksum fail: got=0x%02x expected=0x%02x\n",
                           recv_checksum, expected);
                }
            }
            state = ST_SEARCH_SYNC;
            shift_reg = 0;
        }
    }

    close_target(&target);

    if (!cfg.quiet) {
        uint64_t end_us = now_us();
        double sec = (double)(end_us - start_us) / 1000000.0;
        double payload_bps = sec > 0.0 ? (double)good_payload_bits / sec : 0.0;
        double fer = (valid_frames + bad_frames) > 0
                         ? (double)bad_frames / (double)(valid_frames + bad_frames)
                         : 0.0;
        printf("stats: valid=%" PRIu64 " bad=%" PRIu64 " frame_error_rate=%.4f goodput=%.2f bit/s\n",
               valid_frames, bad_frames, fer, payload_bps);
    }

    return 0;
}
