#!/usr/bin/env bash
set -euo pipefail

THRESHOLD="${1:-170}"
BIT_US="${2:-3000}"
MESSAGE="${3:-HELLO FROM HW3 DEMO}"

echo "[demo] build"
make

echo "[demo] start receiver in background"
./receiver --threshold "${THRESHOLD}" --bit-us "${BIT_US}" --probes 7 --hit-ratio 0.57 --max-frames 1 &
RECV_PID=$!

sleep 1

echo "[demo] send one frame"
./sender --threshold "${THRESHOLD}" --bit-us "${BIT_US}" --repeat 1 --message "${MESSAGE}" --verbose

wait "${RECV_PID}"
echo "[demo] done"
