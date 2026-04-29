#!/usr/bin/env bash
# SPDX-License-Identifier: Unlicense
#
# Compare bench results against a committed baseline.
#
# Usage:
#   bench-compare.sh [--include-regex REGEX] [--warn-only] \
#     [--threshold-override REGEX:MEDIAN:P95] \
#     BASELINE_JSON CURRENT_JSON [MEDIAN_REGRESSION_PCT] [P95_REGRESSION_PCT]
#
# Both JSON files are line-delimited records produced by the bench harness,
# each containing at least: "name", "ns_per_op_median", "ns_per_op_p95".
#
# Benchmark flakiness mitigations:
# - Tail-latency checks skip baseline p95 values below 1 µs, where scheduler
#   jitter dominates these batched microbenchmarks.
# - cli_cold_start_* p95 checks are skipped entirely due to process spawn
#   variability (kernel scheduler, COW fork overhead).
# - The Makefile default thresholds (40% median, 70% p95) are intentionally
#   coarse so the gate catches sustained slowdowns without failing on ordinary
#   local or shared-runner jitter.
#
# Exits 0 if all current cases are within the configured regression thresholds,
# exits 1 if any case regresses or the case set drifts, exits 2 on usage error.

set -euo pipefail

include_regex=""
warn_only=0
threshold_overrides=""

while [ "$#" -gt 0 ]; do
    case "$1" in
        --include-regex)
            if [ "$#" -lt 2 ]; then
                echo "bench-compare: --include-regex requires REGEX" >&2
                exit 2
            fi
            include_regex=$2
            shift 2
            ;;
        --warn-only)
            warn_only=1
            shift
            ;;
        --threshold-override)
            if [ "$#" -lt 2 ]; then
                echo "bench-compare: --threshold-override requires REGEX:MEDIAN:P95" >&2
                exit 2
            fi
            if [ -n "$threshold_overrides" ]; then
                threshold_overrides="${threshold_overrides};$2"
            else
                threshold_overrides=$2
            fi
            shift 2
            ;;
        --)
            shift
            break
            ;;
        -*)
            echo "bench-compare: unknown option: $1" >&2
            exit 2
            ;;
        *)
            break
            ;;
    esac
done

if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
    echo "usage: $0 [--include-regex REGEX] [--warn-only] [--threshold-override REGEX:MEDIAN:P95] BASELINE_JSON CURRENT_JSON [MEDIAN_REGRESSION_PCT] [P95_REGRESSION_PCT]" >&2
    exit 2
fi

baseline=$1
current=$2
median_threshold=${3:-40}
p95_threshold=${4:-70}

if [ ! -f "$baseline" ]; then
    echo "bench-compare: baseline file not found: $baseline" >&2
    exit 2
fi
if [ ! -f "$current" ]; then
    echo "bench-compare: current file not found: $current" >&2
    exit 2
fi

awk -v baseline="$baseline" -v current="$current" \
    -v median_threshold="$median_threshold" -v p95_threshold="$p95_threshold" \
    -v include_regex="$include_regex" -v threshold_overrides="$threshold_overrides" '
BEGIN {
    parse_threshold_overrides(threshold_overrides)
    while ((getline line < baseline) > 0) {
        name = extract(line, "name")
        if (include_regex != "" && name !~ include_regex) {
            continue
        }
        median = extract(line, "ns_per_op_median") + 0
        p95 = extract(line, "ns_per_op_p95") + 0
        if (name != "") {
            base_med[name] = median
            base_p95[name] = p95
            base_seen[name] = 0
            base_order[++base_count] = name
        }
    }
    close(baseline)

    fail = 0
    have_any = 0
    printf "%-40s %-7s %15s %15s %10s %s\n", "case", "metric", "baseline_ns", "current_ns", "delta%", "status"
    while ((getline line < current) > 0) {
        name = extract(line, "name")
        if (include_regex != "" && name !~ include_regex) {
            continue
        }
        median = extract(line, "ns_per_op_median") + 0
        p95 = extract(line, "ns_per_op_p95") + 0
        if (name == "") {
            continue
        }
        have_any = 1
        if (!(name in base_med)) {
            printf "%-40s %-7s %15s %15d %10s %s\n", name, "median", "-", median, "-", "NEW"
            printf "%-40s %-7s %15s %15d %10s %s\n", name, "p95", "-", p95, "-", "NEW"
            fail = 1
            continue
        }
        base_seen[name] = 1
        compare_metric(name, "median", base_med[name], median, median_threshold)
        compare_metric(name, "p95", base_p95[name], p95, p95_threshold)
    }
    close(current)
    if (!have_any) {
        print "bench-compare: no cases in current results" > "/dev/stderr"
        exit 2
    }
    for (i = 1; i <= base_count; ++i) {
        name = base_order[i]
        if (base_seen[name] == 0) {
            printf "%-40s %-7s %15d %15s %10s %s\n", name, "median", base_med[name], "-", "-", "MISSING"
            printf "%-40s %-7s %15d %15s %10s %s\n", name, "p95", base_p95[name], "-", "-", "MISSING"
            fail = 1
        }
    }
    exit fail
}
function compare_metric(name, metric, baseline_value, current_value, threshold,    delta, status, effective_threshold) {
    effective_threshold = threshold_for(name, metric, threshold)
    if (metric == "p95" && baseline_value < 1000) {
        printf "%-40s %-7s %15d %15d %10s %s\n", name, metric, baseline_value, current_value, "-", "SKIP"
        return
    }
    if (metric == "p95" && name ~ /^cli_cold_start_/) {
        printf "%-40s %-7s %15d %15d %10s %s\n", name, metric, baseline_value, current_value, "-", "SKIP"
        return
    }
    if (baseline_value <= 0) {
        printf "%-40s %-7s %15d %15d %10s %s\n", name, metric, baseline_value, current_value, "-", "SKIP"
        return
    }
    delta = ((current_value - baseline_value) * 100.0) / baseline_value
    status = "OK"
    if (delta > effective_threshold) {
        status = "REGRESS"
        fail = 1
    } else if (delta < -effective_threshold) {
        status = "improved"
    }
    printf "%-40s %-7s %15d %15d %9.1f%% %s\n", name, metric, baseline_value, current_value, delta, status
}
function parse_threshold_overrides(raw,    entries, parts, i, n, m) {
    if (raw == "") {
        return
    }
    n = split(raw, entries, ";")
    for (i = 1; i <= n; ++i) {
        m = split(entries[i], parts, ":")
        if (m == 3) {
            override_regex[i] = parts[1]
            override_med[i] = parts[2] + 0
            override_p95[i] = parts[3] + 0
            override_count = i
        }
    }
}
function threshold_for(name, metric, fallback,    i) {
    for (i = 1; i <= override_count; ++i) {
        if (name ~ override_regex[i]) {
            return (metric == "median") ? override_med[i] : override_p95[i]
        }
    }
    return fallback
}
function extract(line, key,    re, m, s, e, val) {
    re = "\"" key "\":"
    m = index(line, re)
    if (m == 0) return ""
    s = m + length(re)
    rest = substr(line, s)
    if (substr(rest, 1, 1) == "\"") {
        rest = substr(rest, 2)
        e = index(rest, "\"")
        if (e == 0) return ""
        return substr(rest, 1, e - 1)
    }
    e = match(rest, /[,}]/)
    if (e == 0) return rest
    val = substr(rest, 1, e - 1)
    gsub(/[ \t]/, "", val)
    return val
}
'
rc=$?
if [ "$warn_only" -eq 1 ] && [ "$rc" -eq 1 ]; then
    echo "bench-compare: WARN-ONLY mode active; regressions reported but not failing" >&2
    exit 0
fi
exit "$rc"
