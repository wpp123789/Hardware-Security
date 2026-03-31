# Reflection Report - Hardware Security HW3

## Team

- Team name: Team submission for COMS E6424 HW3
- Members: Names omitted in this local draft; fill with official group roster before final upload.

## Objective

Build a high-bandwidth, low-latency, reliable microarchitectural covert channel between sender and receiver processes on the same VM under different user accounts, with no overt communication channel.

## Technique Selection

We implemented a `Flush+Reload` style covert channel over a shared read-only library page:

- Sender and receiver both resolve the same library symbol (`clock_gettime` by default).
- Sender transmits by modulating cache residency of that target in fixed time slots.
- Receiver probes the same target and classifies each slot as hit/miss based on calibrated latency threshold.

Rationale:

- Shared library code pages are naturally shared across processes and users.
- Flush+Reload provides strong timing contrast when co-location and shared pages are available.
- Symbol-based target resolution avoids fragile hardcoded file offsets.

## Protocol Design

Frame format:

1. `sync_word` (32 bits)
2. `payload_length` (16 bits)
3. `payload` (`length` bytes)
4. `crc8` (8 bits, poly `0x07`)
5. `end_word` (16 bits)

Reliability elements:

- Hamming-distance tolerant sync acquisition in receiver.
- Multi-probe per-bit decoding with `hit-ratio` threshold.
- CRC-8 integrity check with resynchronization on failure.

## Techniques Tried

- Initial prototype with fixed libc offset (rejected due portability and fragility).
- Dynamic symbol lookup with shared helper library (kept).
- Fixed-threshold default with per-machine calibration tool (`threshold`) (kept).
- Basic framed byte stream without checksum (rejected for poor noise handling).

## Experimental Setup

- OS: Linux x86-64 target environment (assignment platform expectation).
- CPU/microarchitecture: Depends on grading VM; implementation intentionally avoids hardcoded offsets and keeps parameters configurable.
- Hypervisor/VM info: Sender and receiver are expected to run in the same VM under different user accounts.
- Compiler: `gcc` (`-O2 -Wall -Wextra -std=c11`)

Runtime parameters used (example):

- `--symbol clock_gettime`
- `--bit-us 3000`
- `--probes 7`
- `--hit-ratio 0.57`
- `--sync-tolerance 4`

## Calibration Results

Use `./threshold --samples 20000` on the target VM and copy exact values below:

- Cached median: measured by tool output.
- Cached p95: measured by tool output.
- Flushed median: measured by tool output.
- Flushed p05: measured by tool output.
- Chosen threshold: suggested by tool output.

## Bandwidth and Reliability

Raw channel bit rate:

- `1 / bit_us` bits per microsecond, e.g. `bit_us=3000` gives about `333 bps`.

Net throughput depends on framing overhead:

- 32 (sync) + 16 (len) + 8 (crc) + 16 (end) = 72 overhead bits per frame, plus payload bits.

For payload length `L` bytes:

- Total bits = `72 + 8L`
- Payload efficiency = `8L / (72 + 8L)`

Measured reliability summary:

- Valid frames: read from receiver runtime output.
- CRC failures: read from receiver runtime output.
- Format failures: read from receiver runtime output.
- Observed decoded text correctness: compare sender payload and decoded receiver payload.

## Expected vs Actual

- Expected: clean hit/miss separation and robust decoding after threshold calibration.
- Actual: strong separation is expected on lightly loaded systems; on noisy systems increase `bit-us` and `probes`.
- Main gap contributors: scheduling jitter, VM noise, chosen bit slot duration, symbol choice.

## Reproducibility Notes

1. Build with `make`.
2. Calibrate threshold with `./threshold`.
3. Run receiver first, then sender.
4. Keep sender/receiver parameters identical for `--threshold`, `--bit-us`, and symbol selection.
5. Repeat calibration after moving to a different VM or hardware target.

## Limitations and Future Improvements

- Performance and error rate depend on scheduler contention.
- Current implementation uses simple CRC + resync (not full ECC).
- Future work:
  - stronger FEC (e.g., Hamming/Reed-Solomon style redundancy),
  - adaptive bit timing,
  - multi-symbol channel hopping for better resilience.

## Oral Review Preparedness

All team members should be able to explain:

- why symbol-based Flush+Reload works across processes/users,
- threshold calibration and decoding logic,
- frame format and CRC checks,
- tuning choices (`bit-us`, `probes`, `hit-ratio`, sync tolerance),
- measured reliability and bottlenecks.
