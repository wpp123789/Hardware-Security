#ifndef CHANNEL_COMMON_H
#define CHANNEL_COMMON_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH_DEFAULT_LIBRARY "libc.so.6"
#define CH_DEFAULT_SYMBOL "clock_gettime"
#define CH_SYNC_WORD 0xC33CC33Cu
#define CH_END_WORD 0xDDAAu
#define CH_MAX_PAYLOAD 1024u

typedef struct {
    const char *library_name;
    const char *symbol_name;
    int threshold_cycles;
    int bit_us;
    int probes_per_bit;
    double hit_ratio;
} ch_config_t;

typedef struct {
    void *dl_handle;
    void *target_addr;
    ch_config_t cfg;
} ch_target_t;

typedef struct {
    uint64_t bits_seen;
    uint64_t frames_ok;
    uint64_t frames_bad_crc;
    uint64_t frames_bad_format;
    uint64_t frames_bad_length;
    uint64_t sync_attempts;
} ch_receiver_stats_t;

void ch_default_config(ch_config_t *cfg);
int ch_parse_common_args(int argc, char **argv, ch_config_t *cfg);
void ch_print_common_usage(FILE *out);

int ch_init_target(ch_target_t *target, const ch_config_t *cfg);
void ch_destroy_target(ch_target_t *target);

uint64_t ch_rdtscp64(void);
uint64_t ch_measure_access_cycles(void *addr);
void ch_flush(void *addr);
void ch_access_burst(void *addr, int iters);

uint8_t ch_crc8(const uint8_t *data, size_t len);
unsigned ch_hamming32(uint32_t a, uint32_t b);

#ifdef __cplusplus
}
#endif

#endif
