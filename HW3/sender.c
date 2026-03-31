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
    const char *message;
    int repeat_count;
    int gap_ms;
    int one_burst;
    int verbose;
} sender_cfg_t;

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
    fprintf(stderr, "Sender options:\n");
    fprintf(stderr, "  --message <text>       Message to send (default: \"HELLO FROM HW3\")\n");
    fprintf(stderr, "  --repeat <n>           Number of frame repeats, 0 = infinite (default: 0)\n");
    fprintf(stderr, "  --gap-ms <ms>          Delay between frames (default: 120)\n");
    fprintf(stderr, "  --one-burst <iters>    Loads per inner burst for bit=1 (default: 64)\n");
    fprintf(stderr, "  --verbose              Print per-frame summary\n");
    fprintf(stderr, "  --help                 Show this help\n");
    ch_print_common_usage(stderr);
}

static int parse_sender_args(int argc, char **argv, sender_cfg_t *sender_cfg, ch_config_t *common_cfg) {
    static const struct option long_opts[] = {
        {"message", required_argument, NULL, 2001},
        {"repeat", required_argument, NULL, 2002},
        {"gap-ms", required_argument, NULL, 2003},
        {"one-burst", required_argument, NULL, 2004},
        {"verbose", no_argument, NULL, 2005},
        {"help", no_argument, NULL, 2006},
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
                sender_cfg->message = optarg;
                break;
            case 2002:
                sender_cfg->repeat_count = parse_i(optarg, 0, 10000000, "repeat");
                if (sender_cfg->repeat_count < 0) return -1;
                break;
            case 2003:
                sender_cfg->gap_ms = parse_i(optarg, 0, 3600000, "gap-ms");
                if (sender_cfg->gap_ms < 0) return -1;
                break;
            case 2004:
                sender_cfg->one_burst = parse_i(optarg, 1, 100000, "one-burst");
                if (sender_cfg->one_burst < 0) return -1;
                break;
            case 2005:
                sender_cfg->verbose = 1;
                break;
            case 2006:
                usage(argv[0]);
                return 1;
            case 1001:
                common_cfg->library_name = optarg;
                break;
            case 1002:
                common_cfg->symbol_name = optarg;
                break;
            case 1003:
                common_cfg->threshold_cycles = parse_i(optarg, 1, 1000000, "threshold");
                if (common_cfg->threshold_cycles < 0) return -1;
                break;
            case 1004:
                common_cfg->bit_us = parse_i(optarg, 10, 2000000, "bit-us");
                if (common_cfg->bit_us < 0) return -1;
                break;
            case 1005:
                common_cfg->probes_per_bit = parse_i(optarg, 1, 1000, "probes");
                if (common_cfg->probes_per_bit < 0) return -1;
                break;
            case 1006:
                common_cfg->hit_ratio = parse_d(optarg, 0.01, 1.0, "hit-ratio");
                if (common_cfg->hit_ratio < 0.0) return -1;
                break;
            default:
                usage(argv[0]);
                return -1;
        }
    }

    if (!sender_cfg->message) {
        sender_cfg->message = "HELLO FROM HW3";
    }
    if (sender_cfg->repeat_count < 0 || sender_cfg->gap_ms < 0 || sender_cfg->one_burst <= 0) {
        fprintf(stderr, "Invalid sender options\n");
        return -1;
    }
    if (strlen(sender_cfg->message) > CH_MAX_PAYLOAD) {
        fprintf(stderr, "Message too long (max %u bytes)\n", CH_MAX_PAYLOAD);
        return -1;
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

static void sleep_ns(uint64_t ns) {
    struct timespec req;
    req.tv_sec = (time_t)(ns / 1000000000ULL);
    req.tv_nsec = (long)(ns % 1000000000ULL);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
}

static void send_bit(ch_target_t *target, int bit, int one_burst) {
    uint64_t slot_ns = (uint64_t)target->cfg.bit_us * 1000ULL;
    uint64_t end_ns = monotonic_ns() + slot_ns;

    ch_flush(target->target_addr);
    if (!bit) {
        sleep_ns(slot_ns);
        return;
    }
    while (monotonic_ns() < end_ns) {
        ch_access_burst(target->target_addr, one_burst);
    }
}

static void send_u16(ch_target_t *target, uint16_t v, int one_burst) {
    for (int i = 0; i < 16; i++) {
        send_bit(target, (v >> i) & 1u, one_burst);
    }
}

static void send_u32(ch_target_t *target, uint32_t v, int one_burst) {
    for (int i = 0; i < 32; i++) {
        send_bit(target, (v >> i) & 1u, one_burst);
    }
}

static void send_byte(ch_target_t *target, uint8_t b, int one_burst) {
    for (int i = 0; i < 8; i++) {
        send_bit(target, (b >> i) & 1u, one_burst);
    }
}

static void send_frame(ch_target_t *target, const uint8_t *payload, size_t len, int one_burst) {
    uint8_t crc = ch_crc8(payload, len);
    send_u32(target, CH_SYNC_WORD, one_burst);
    send_u16(target, (uint16_t)len, one_burst);
    for (size_t i = 0; i < len; i++) {
        send_byte(target, payload[i], one_burst);
    }
    send_byte(target, crc, one_burst);
    send_u16(target, CH_END_WORD, one_burst);
}

int main(int argc, char **argv) {
    ch_config_t common_cfg;
    sender_cfg_t sender_cfg;
    ch_target_t target;
    size_t msg_len = 0;
    uint64_t frame_idx = 0;
    int parse_rc = 0;

    ch_default_config(&common_cfg);
    sender_cfg.message = NULL;
    sender_cfg.repeat_count = 0;
    sender_cfg.gap_ms = 120;
    sender_cfg.one_burst = 64;
    sender_cfg.verbose = 0;

    parse_rc = parse_sender_args(argc, argv, &sender_cfg, &common_cfg);
    if (parse_rc != 0) {
        return parse_rc < 0 ? 1 : 0;
    }

    if (ch_init_target(&target, &common_cfg) != 0) {
        return 1;
    }

    msg_len = strlen(sender_cfg.message);
    printf("[sender] target=%p lib=%s symbol=%s bit_us=%d threshold=%d message_len=%zu\n",
           target.target_addr, common_cfg.library_name, common_cfg.symbol_name,
           common_cfg.bit_us, common_cfg.threshold_cycles, msg_len);
    fflush(stdout);

    while (sender_cfg.repeat_count == 0 || (int)frame_idx < sender_cfg.repeat_count) {
        send_frame(&target, (const uint8_t *)sender_cfg.message, msg_len, sender_cfg.one_burst);
        frame_idx++;
        if (sender_cfg.verbose) {
            printf("[sender] frame=%" PRIu64 " crc=0x%02x text=\"%s\"\n",
                   frame_idx, ch_crc8((const uint8_t *)sender_cfg.message, msg_len), sender_cfg.message);
            fflush(stdout);
        }
        if (sender_cfg.gap_ms > 0) {
            usleep((useconds_t)sender_cfg.gap_ms * 1000u);
        }
    }

    printf("[sender] done, sent %" PRIu64 " frame(s)\n", frame_idx);
    ch_destroy_target(&target);
    return 0;
}
