# Minimal Flush+Reload Covert Channel

This project implements a simple instruction-page Flush+Reload covert channel between two user processes on the same VM.

## Files

- `sender.c` - sends one framed message (optionally repeated)
- `receiver.c` - receives framed messages with threshold timing and sync search
- `calibrate.c` - estimates hit/miss timing and suggests a threshold
- `common.c`, `common.h` - shared low-level helpers
- `Makefile` - build rules

## Protocol

Frame format (bits):

- `sync` (32-bit fixed word)
- `length` (16-bit payload length in bytes)
- `payload` (`length * 8` bits)
- `checksum` (8-bit XOR of payload bytes)

Bit encoding:

- bit `1`: sender repeatedly touches the target symbol address during the slot
- bit `0`: sender stays idle during the slot

Receiver:

- probes multiple times per slot
- each probe does flush -> wait -> timed reload
- classifies hit/miss by threshold and votes within slot
- searches for sync continuously, then decodes frame and validates checksum

## Build

```bash
make clean
make
```

## Typical Run

### 1) Calibrate threshold

```bash
./calibrate --samples 50000 --lib libc.so.6 --symbol puts
```

Take `recommended threshold` from output.

### 2) Start receiver (terminal A)

```bash
./receiver --threshold 180 --bit-us 500 --probes 5 --sync-tolerance 0 --max-frames 1 --lib libc.so.6 --symbol puts
```

### 3) Start sender (terminal B)

```bash
./sender --message "HELLO_HW3" --repeat 1 --bit-us 500 --gap-ms 20 --lib libc.so.6 --symbol puts --verbose
```

Receiver should print one valid decoded frame.

## Throughput estimate

Payload bits:

```text
payload_bits = 8 * payload_length_bytes
```

Total transmission time (single frame):

```text
total_time_sec ~= frame_bits * bit_us / 1e6
frame_bits = 32 + 16 + payload_bits + 8
```

Bandwidth:

```text
bandwidth_bps = payload_bits / total_time_sec
```

The sender and receiver also print measured runtime/throughput stats.

## Error-rate estimate (repeated runs)

1. Send the same message with `--repeat N` (for example `N=50`).
2. Run receiver with `--max-frames N`.
3. Compare number of valid frames vs corrupted/dropped frames.

Simple frame error estimate:

```text
FER = bad_frames / (valid_frames + bad_frames)
```

To approximate bit error rate, compare decoded payloads to expected payload across runs and count bit mismatches.
