#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#define DEFAULT_LIB "libc.so.6"
#define DEFAULT_SYMBOL "puts"

#define SYNC_WORD 0xD391C5A7u
#define MAX_PAYLOAD_LEN 2048u

typedef struct {
    void *handle;
    volatile uint8_t *addr;
} target_t;

int resolve_target(const char *lib_path, const char *symbol, target_t *out);
void close_target(target_t *target);

uint32_t reload_time_cycles(volatile uint8_t *addr);
void flush_addr(volatile void *addr);
void touch_addr(volatile uint8_t *addr);

uint64_t now_us(void);
void sleep_until_us(uint64_t deadline_us);

uint8_t xor_checksum(const uint8_t *data, size_t len);
int parse_u32(const char *s, uint32_t *out);
int parse_u64(const char *s, uint64_t *out);
int hamming_u32(uint32_t a, uint32_t b);

#endif
