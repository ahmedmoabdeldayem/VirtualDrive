#!/usr/bin/env bash
# Measure PREEMPT_RT scheduling latency using cyclictest + stress-ng.
# Must be run ON the RPi5 (not the PC).
# Usage: sudo bash measure_latency.sh [duration_seconds]
set -euo pipefail

DURATION="${1:-60}"
PRIORITY=80
PERIOD_US=1000        # 1 ms
HISTOGRAM="/tmp/latency_histogram.txt"
STRESS_LOG="/tmp/stress.log"

if [[ "$(id -u)" -ne 0 ]]; then
    echo "ERROR: This script must be run as root (for SCHED_FIFO access)."
    exit 1
fi

command -v cyclictest  >/dev/null 2>&1 || { echo "ERROR: cyclictest not found. Install rt-tests."; exit 1; }
command -v stress-ng   >/dev/null 2>&1 || { echo "ERROR: stress-ng not found.";                   exit 1; }

echo "========================================================"
echo " PREEMPT_RT Latency Measurement"
echo " Duration:  ${DURATION}s"
echo " Period:    ${PERIOD_US} µs"
echo " Priority:  SCHED_FIFO ${PRIORITY}"
echo " Stressors: CPU + memory"
echo "========================================================"
echo ""

# Verify kernel has PREEMPT_RT
if grep -q "PREEMPT_RT" /proc/version 2>/dev/null; then
    echo "[OK] PREEMPT_RT kernel detected"
else
    echo "[WARN] PREEMPT_RT not detected in /proc/version — results may be high"
fi
echo ""

echo "==> Starting stress-ng in background"
stress-ng --cpu 3 --vm 1 --vm-bytes 128M \
          --timeout "${DURATION}s" \
          --log-file "${STRESS_LOG}" &
STRESS_PID=$!
echo "    stress-ng PID=${STRESS_PID}"

# Brief delay so stress-ng is fully running before measurement starts
sleep 1

echo "==> Running cyclictest for ${DURATION}s ..."
cyclictest \
    --mlockall \
    --smp \
    --priority="${PRIORITY}" \
    --interval="${PERIOD_US}" \
    --duration="${DURATION}" \
    --histogram=400 \
    --histfile="${HISTOGRAM}" \
    --quiet

echo "==> Stopping stress-ng"
kill "${STRESS_PID}" 2>/dev/null || true
wait "${STRESS_PID}" 2>/dev/null || true

echo ""
echo "==> Histogram saved to ${HISTOGRAM}"
echo ""

echo "==> Summary (min/avg/max per CPU):"
cyclictest \
    --mlockall \
    --smp \
    --priority="${PRIORITY}" \
    --interval="${PERIOD_US}" \
    --duration=5 \
    --quiet 2>&1 | tail -n 20 || true

echo ""
echo "==> Top latency percentiles (from histogram):"
if [[ -f "${HISTOGRAM}" ]]; then
    # Parse cyclictest histogram: columns = latency_us, cpu0_count, cpu1_count, ...
    # Find the latency at which cumulative count crosses 99th / 99.9th percentile
    python3 - "${HISTOGRAM}" <<'PYEOF'
import sys, collections

fname = sys.argv[1]
buckets = {}  # latency_us -> total_count
total = 0
with open(fname) as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split()
        try:
            lat = int(parts[0])
            cnt = sum(int(x) for x in parts[1:])
            buckets[lat] = cnt
            total += cnt
        except ValueError:
            continue

if total == 0:
    print("No data found in histogram.")
    sys.exit(0)

thresholds = {50: None, 99: None, 99.9: None, 99.99: None}
running = 0
for lat in sorted(buckets):
    running += buckets[lat]
    pct = running / total * 100.0
    for t in list(thresholds):
        if thresholds[t] is None and pct >= t:
            thresholds[t] = lat

print(f"  Total samples: {total:,}")
print(f"  Min:      {min(buckets):>6} µs")
for pct in [50, 99, 99.9, 99.99]:
    val = thresholds.get(pct)
    label = f"P{pct}" if pct < 99 else f"P{pct:.2f}".rstrip('0').rstrip('.')
    print(f"  {label:8s}: {val:>6} µs")
print(f"  Max:      {max(buckets):>6} µs")
print()
print("  Latency < 100 µs (RT target): " +
      ("PASS" if (thresholds.get(99.9) or 9999) < 100 else
       f"FAIL — P99.9 = {thresholds.get(99.9)} µs"))
PYEOF
fi
