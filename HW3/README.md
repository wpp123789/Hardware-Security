# COMS E6424 Hardware Security Homework 3

This project implements a microarchitectural covert channel using a `Flush+Reload` style design between:

- `sender` (transmitter)
- `receiver` (decoder)
- `threshold` (calibration utility)

The code is written in C and uses no external repository code.

## Design Summary

- Shared target is a dynamically resolved symbol in a shared library (default: `clock_gettime` from `libc.so.6`).
- Sender encodes bits by **cache activity** in fixed time slots:
  - bit `1`: repeatedly access target address
  - bit `0`: stay mostly idle after flush
- Receiver probes access latency multiple times per slot and makes a majority/ratio decision.
- Protocol framing includes:
  - 32-bit sync word (`CH_SYNC_WORD`)
  - 16-bit payload length
  - payload bytes
  - CRC-8 (poly `0x07`)
  - 16-bit end marker (`CH_END_WORD`)
- Receiver performs continuous sync search with Hamming-distance tolerance for noisy conditions.

## Files

- `channel_common.h`, `channel_common.c`: shared target resolution, timing primitives, CRC, and utilities.
- `sender.c`: frame transmitter.
- `receiver.c`: framed decoder + noise-tolerant synchronization.
- `threshold.c`: cached vs flushed timing calibration and threshold recommendation.
- `Makefile`: build/clean/demo targets.
- `report.md`: reflection/report draft.
- `worklog.txt`: team work log template.

## Build

On Linux x86-64:

```bash
make clean
make
```

## Calibration

First compute a threshold on the target machine:

```bash
./threshold --samples 20000 --symbol clock_gettime
```

Use the suggested threshold for both sender and receiver.

## Demo Workflow (same machine, two terminals)

Terminal A (receiver):

```bash
./receiver --threshold 170 --bit-us 3000 --probes 7 --hit-ratio 0.57 --sync-tolerance 4 --max-frames 3
```

Terminal B (sender):

```bash
./sender --threshold 170 --bit-us 3000 --repeat 3 --gap-ms 120 --message "HELLO FROM HW3"
```

## Multi-user Covert-Channel Setup

Run sender and receiver under different user accounts on the same VM (assignment requirement). Example:

- user1 starts `receiver`
- user2 starts `sender`

No sockets, files, signals, shared memory IPC, or other overt channels are used for communication.

## Useful Parameters

- `--symbol`: choose alternate probe symbol if needed (for portability).
- `--lib`: shared library name (default `libc.so.6`).
- `--bit-us`: larger values increase robustness under noisy scheduling.
- `--probes` and `--hit-ratio`: tune decoding confidence.
- `--sync-tolerance`: increase on noisy systems for easier lock acquisition.

## Notes

- This implementation targets Linux x86-64 and requires TSC timing support (`rdtscp`) and cache flush (`clflush`) instructions.
- Use `threshold` each time you move to a new machine/microarchitecture.
