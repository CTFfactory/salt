#!/usr/bin/env bash
#
# scripts/fuzz.sh - Long-running orchestrator for the salt libFuzzer harnesses.
#
# Runs a selected harness set for a configurable total budget (default 7h),
# persists corpora and crash artifacts under .fuzz/, optionally restores/syncs
# the long-run working corpus via user-provided hook commands, runs an
# end-of-run corpus minimization, and exits nonzero if any harness produced
# crash artifacts.
#
# Usage (overnight):
#   nohup ./scripts/fuzz.sh > .fuzz/run.log 2>&1 &
#
# Env overrides:
#   TOTAL_BUDGET_SECONDS  Total wall-clock budget across all harnesses (default 25200 = 7h)
#   HARNESS_SET           core|all (default core = parse/cli/output)
#   BUDGET_PARSE          Per-harness override (seconds)
#   BUDGET_CLI            Per-harness override (seconds)
#   BUDGET_OUTPUT         Per-harness override (seconds)
#   BUDGET_BOUNDARY       Per-harness override (seconds)
#   BUDGET_ROUNDTRIP      Per-harness override (seconds)
#   PARALLEL              libFuzzer -jobs/-workers per harness (default 1)
#   RSS_LIMIT_MB          libFuzzer -rss_limit_mb (default 2048)
#   MALLOC_LIMIT_MB       libFuzzer -malloc_limit_mb (default 256)
#   TIMEOUT_PER_INPUT     libFuzzer -timeout (default 25)
#   MAX_LEN_PARSE         libFuzzer -max_len for parse (default 4096)
#   MAX_LEN_CLI           libFuzzer -max_len for cli (default 4096)
#   MAX_LEN_OUTPUT        libFuzzer -max_len for output (default 4096)
#   MAX_LEN_BOUNDARY      libFuzzer -max_len for boundary (default 256)
#   MAX_LEN_ROUNDTRIP     libFuzzer -max_len for roundtrip (default 4096)
#   FUZZ_ROOT             Root for corpora/logs/crashes (default .fuzz)
#   FUZZ_SEED_CORPUS_ROOT Read-only committed seed corpora root (default fuzz/corpus)
#   MINIMIZE              1 = run -merge=1 minimization at end (default 1)
#   KEEP_GOING            1 = continue across harnesses on crash (default 1)
#   SKIP_BUILD            1 = skip make fuzz-build (default 0)
#   FUZZ_EXTERNAL_CORPUS_RESTORE_CMD
#                         Optional shell command that restores the long-run
#                         working corpus into $FUZZ_ROOT before fuzzing.
#   FUZZ_EXTERNAL_CORPUS_SYNC_CMD
#                         Optional shell command that syncs the minimized
#                         long-run working corpus from $FUZZ_ROOT after fuzzing.
#                         These hooks are for long-running campaigns only;
#                         CI, fuzz-regression, and releases continue to use the
#                         committed seed corpus in fuzz/corpus/.

set -Eeuo pipefail
IFS=$'\n\t'

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

TOTAL_BUDGET_SECONDS="${TOTAL_BUDGET_SECONDS:-25200}"
HARNESS_SET="${HARNESS_SET:-core}"
PARALLEL="${PARALLEL:-1}"
RSS_LIMIT_MB="${RSS_LIMIT_MB:-2048}"
MALLOC_LIMIT_MB="${MALLOC_LIMIT_MB:-256}"
TIMEOUT_PER_INPUT="${TIMEOUT_PER_INPUT:-25}"
MAX_LEN_PARSE="${MAX_LEN_PARSE:-4096}"
MAX_LEN_CLI="${MAX_LEN_CLI:-4096}"
MAX_LEN_OUTPUT="${MAX_LEN_OUTPUT:-4096}"
MAX_LEN_BOUNDARY="${MAX_LEN_BOUNDARY:-256}"
MAX_LEN_ROUNDTRIP="${MAX_LEN_ROUNDTRIP:-4096}"
FUZZ_ROOT="${FUZZ_ROOT:-.fuzz}"
FUZZ_SEED_CORPUS_ROOT="${FUZZ_SEED_CORPUS_ROOT:-fuzz/corpus}"
MINIMIZE="${MINIMIZE:-1}"
KEEP_GOING="${KEEP_GOING:-1}"
SKIP_BUILD="${SKIP_BUILD:-0}"
FUZZ_EXTERNAL_CORPUS_RESTORE_CMD="${FUZZ_EXTERNAL_CORPUS_RESTORE_CMD:-}"
FUZZ_EXTERNAL_CORPUS_SYNC_CMD="${FUZZ_EXTERNAL_CORPUS_SYNC_CMD:-}"

case "$HARNESS_SET" in
    core)
        HARNESS_NAMES=(parse cli output)
        ;;
    all)
        HARNESS_NAMES=(parse cli output boundary roundtrip)
        ;;
    *)
        printf '[fuzz.sh %s] ERROR: HARNESS_SET must be core or all (got %s)\n' \
            "$(date -u +%FT%TZ)" "$HARNESS_SET" >&2
        exit 2
        ;;
esac

FUZZ_ACTIVE_HARNESSES="$(IFS=,; printf '%s' "${HARNESS_NAMES[*]}")"

PER_HARNESS_DEFAULT=$(( TOTAL_BUDGET_SECONDS / ${#HARNESS_NAMES[@]} ))
BUDGET_PARSE="${BUDGET_PARSE:-$PER_HARNESS_DEFAULT}"
BUDGET_CLI="${BUDGET_CLI:-$PER_HARNESS_DEFAULT}"
BUDGET_OUTPUT="${BUDGET_OUTPUT:-$PER_HARNESS_DEFAULT}"
BUDGET_BOUNDARY="${BUDGET_BOUNDARY:-$PER_HARNESS_DEFAULT}"
BUDGET_ROUNDTRIP="${BUDGET_ROUNDTRIP:-$PER_HARNESS_DEFAULT}"

SUMMARY_FILE="$FUZZ_ROOT/summary.txt"
CHILD_PID=""
CRASH_FOUND=0

log() {
    printf '[fuzz.sh %s] %s\n' "$(date -u +%FT%TZ)" "$*"
}

run_external_hook() {
    local phase="$1"
    local hook_cmd="$2"
    local rc

    if [ -z "$hook_cmd" ]; then
        log "Skipping $phase external corpus hook"
        return 0
    fi

    log "==> $phase external corpus hook"
    set +e
    env \
        REPO_ROOT="$REPO_ROOT" \
        FUZZ_ROOT="$FUZZ_ROOT" \
        FUZZ_SEED_CORPUS_ROOT="$FUZZ_SEED_CORPUS_ROOT" \
        FUZZ_ACTIVE_HARNESSES="$FUZZ_ACTIVE_HARNESSES" \
        HARNESS_SET="$HARNESS_SET" \
        bash -lc "$hook_cmd"
    rc=$?
    set -e

    if [ "$rc" -ne 0 ]; then
        log "ERROR: $phase external corpus hook failed (rc=$rc)"
        # RESTORE (pre-fuzz) hook failure is fatal: cannot proceed without seed corpus.
        # SYNC (post-fuzz) hook failure is non-fatal: minimized corpus is preserved locally.
        # Wrap this script with monitoring to distinguish pre-run vs post-run failures
        # (e.g., exit code 10 for restore, 11 for sync).
        return "$rc"
    fi

    log "OK: $phase external corpus hook completed"
    return 0
}

ensure_dirs() {
    local name
    mkdir -p "$FUZZ_ROOT"
    for name in "${HARNESS_NAMES[@]}"; do
        mkdir -p "$FUZZ_ROOT/$name/corpus" \
                 "$FUZZ_ROOT/$name/crashes" \
                 "$FUZZ_ROOT/$name/logs"
    done
}

harness_bin() {
    case "$1" in
        parse)     printf 'build/fuzz_parse\n' ;;
        cli)       printf 'build/fuzz_cli\n' ;;
        output)    printf 'build/fuzz_output\n' ;;
        boundary)  printf 'build/fuzz_boundary\n' ;;
        roundtrip) printf 'build/fuzz_roundtrip\n' ;;
        *)
            log "ERROR: unknown harness: $1"
            return 2
            ;;
    esac
}

harness_budget() {
    case "$1" in
        parse)     printf '%s\n' "$BUDGET_PARSE" ;;
        cli)       printf '%s\n' "$BUDGET_CLI" ;;
        output)    printf '%s\n' "$BUDGET_OUTPUT" ;;
        boundary)  printf '%s\n' "$BUDGET_BOUNDARY" ;;
        roundtrip) printf '%s\n' "$BUDGET_ROUNDTRIP" ;;
        *)
            log "ERROR: unknown harness: $1"
            return 2
            ;;
    esac
}

harness_max_len() {
    case "$1" in
        parse)     printf '%s\n' "$MAX_LEN_PARSE" ;;
        cli)       printf '%s\n' "$MAX_LEN_CLI" ;;
        output)    printf '%s\n' "$MAX_LEN_OUTPUT" ;;
        boundary)  printf '%s\n' "$MAX_LEN_BOUNDARY" ;;
        roundtrip) printf '%s\n' "$MAX_LEN_ROUNDTRIP" ;;
        *)
            log "ERROR: unknown harness: $1"
            return 2
            ;;
    esac
}

sum_stat_from_log() {
    local logfile="$1"
    local key="$2"
    awk -v key="stat::${key}" '
        index($0, key ":") == 1 {
            value = substr($0, length(key) + 2)
            sub(/^[[:space:]]+/, "", value)
            sum += value
            found = 1
        }
        END { if (found) print sum }
    ' "$logfile" 2>/dev/null
}

max_stat_from_log() {
    local logfile="$1"
    local key="$2"
    awk -v key="stat::${key}" '
        index($0, key ":") == 1 {
            value = substr($0, length(key) + 2)
            sub(/^[[:space:]]+/, "", value)
            if (!found || value > max) {
                max = value
            }
            found = 1
        }
        END { if (found) print max }
    ' "$logfile" 2>/dev/null
}

merge_summary_from_log() {
    local logfile="$1"
    awk '
        /MERGE-OUTER: [0-9]+ new files with [0-9]+ new features added; [0-9]+ new coverage edges/ {
            files = $2
            features = $6
            edges = $10
        }
        END {
            if (files != "") {
                printf "%s files/%s ft/%s edges", files, features, edges
            }
        }
    ' "$logfile" 2>/dev/null
}

preflight() {
    if ! command -v clang >/dev/null 2>&1; then
        log "ERROR: clang not found in PATH (required by FUZZ_CC)"
        exit 2
    fi
    local free_kb
    free_kb=$(df -kP "$FUZZ_ROOT" 2>/dev/null | awk 'NR==2 {print $4}' || echo 0)
    if [ "${free_kb:-0}" -lt 2097152 ]; then
        log "WARN: less than 2 GiB free under build/ (have ${free_kb} KiB)"
    fi
    if [ "$SKIP_BUILD" != "1" ]; then
        log "Building fuzz harnesses (make fuzz-build)"
        make fuzz-build
    fi
}

cleanup() {
    # shellcheck disable=SC2317  # invoked via trap
    if [ -n "$CHILD_PID" ] && kill -0 "$CHILD_PID" 2>/dev/null; then
        log "Signal received; stopping current fuzzer (pid=$CHILD_PID)"
        kill -TERM "$CHILD_PID" 2>/dev/null || true
        wait "$CHILD_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

run_harness() {
    local name="$1"
    local bin="$2"
    local budget="$3"
    local corpus="$FUZZ_ROOT/$name/corpus"
    local crashes="$FUZZ_ROOT/$name/crashes"
    local logfile="$FUZZ_ROOT/$name/logs/fuzz.log"
    local startfile="$FUZZ_ROOT/$name/logs/start.txt"
    local endfile="$FUZZ_ROOT/$name/logs/end.txt"
    local seed_corpus="$FUZZ_SEED_CORPUS_ROOT/$name"
    local seed_label="<none>"
    local max_len
    local dict="fuzz/dictionaries/${name}.dict"
    local dict_arg=()
    local corpus_inputs=("$corpus")
    if [ -f "$dict" ]; then
        dict_arg=("-dict=$dict")
    fi
    if [ -d "$seed_corpus" ]; then
        corpus_inputs+=("$seed_corpus")
        seed_label="$seed_corpus"
    fi

    if [ ! -x "$bin" ]; then
        log "ERROR: harness binary missing: $bin"
        return 2
    fi
    max_len="$(harness_max_len "$name")"

    log "==> $name: budget=${budget}s max_len=${max_len} corpus=$corpus seeds=$seed_label crashes=$crashes dict=${dict_arg[*]:-<none>}"
    date -u +%FT%TZ > "$startfile"

    local rc=0
    set +e
    ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:halt_on_error=1:strict_string_checks=1:detect_stack_use_after_return=1:print_summary=1:allocator_may_return_null=1" \
    UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
    "$bin" \
        -max_total_time="$budget" \
        -max_len="$max_len" \
        -rss_limit_mb="$RSS_LIMIT_MB" \
        -malloc_limit_mb="$MALLOC_LIMIT_MB" \
        -timeout="$TIMEOUT_PER_INPUT" \
        -print_final_stats=1 \
        -jobs="$PARALLEL" \
        -workers="$PARALLEL" \
        -artifact_prefix="$crashes/" \
        "${dict_arg[@]}" \
        "${corpus_inputs[@]}" \
        >>"$logfile" 2>&1 &
    CHILD_PID=$!
    wait "$CHILD_PID"
    rc=$?
    set -e
    CHILD_PID=""

    date -u +%FT%TZ > "$endfile"

    local crash_count
    crash_count=$(find "$crashes" -mindepth 1 -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
    if [ "$crash_count" -gt 0 ]; then
        CRASH_FOUND=1
        log "CRASH: $name produced $crash_count artifact(s) in $crashes (rc=$rc)"
        if [ "$KEEP_GOING" != "1" ]; then
            return 1
        fi
    elif [ "$rc" -ne 0 ]; then
        log "ERROR: $name exited with non-zero status (rc=$rc, log=$logfile)"
        if [ "$KEEP_GOING" != "1" ]; then
            return "$rc"
        fi
    else
        log "OK: $name finished cleanly (rc=$rc, log=$logfile)"
    fi
    return 0
}

minimize_corpus() {
    local name="$1"
    local bin="$2"
    local corpus="$FUZZ_ROOT/$name/corpus"
    local minimized="$FUZZ_ROOT/$name/corpus.min"
    local prev="$FUZZ_ROOT/$name/corpus.prev"
    local mergelog="$FUZZ_ROOT/$name/logs/merge.log"
    local max_len

    if [ ! -x "$bin" ]; then
        return 0
    fi
    max_len="$(harness_max_len "$name")"

    log "==> $name: minimizing corpus via -merge=1"
    rm -rf "$minimized"
    mkdir -p "$minimized"

    set +e
    "$bin" -max_len="$max_len" -merge=1 "$minimized" "$corpus" >>"$mergelog" 2>&1
    local rc=$?
    set -e

    if [ "$rc" -ne 0 ]; then
        log "WARN: $name merge failed (rc=$rc); leaving original corpus intact"
        rm -rf "$minimized"
        return 0
    fi

    rm -rf "$prev"
    mv "$corpus" "$prev"
    mv "$minimized" "$corpus"
}

write_summary() {
    local name budget corpus crashes crash_count corpus_count logfile mergelog
    local exec_units exec_rate new_units peak_rss merge_summary
    local warn_found=0
    {
        echo "salt fuzz overnight run summary"
        echo "generated: $(date -u +%FT%TZ)"
        echo "total_budget_seconds: $TOTAL_BUDGET_SECONDS"
        echo "harness_set: $HARNESS_SET"
        echo "parallel_jobs: $PARALLEL"
        echo "rss_limit_mb: $RSS_LIMIT_MB"
        echo "timeout_per_input: $TIMEOUT_PER_INPUT"
        echo "active_harnesses: $FUZZ_ACTIVE_HARNESSES"
        echo "external_restore_hook: $(if [ -n "$FUZZ_EXTERNAL_CORPUS_RESTORE_CMD" ]; then printf configured; else printf disabled; fi)"
        echo "external_sync_hook: $(if [ -n "$FUZZ_EXTERNAL_CORPUS_SYNC_CMD" ]; then printf configured; else printf disabled; fi)"
        echo "max_len_parse: $MAX_LEN_PARSE"
        echo "max_len_cli: $MAX_LEN_CLI"
        echo "max_len_output: $MAX_LEN_OUTPUT"
        echo "max_len_boundary: $MAX_LEN_BOUNDARY"
        echo "max_len_roundtrip: $MAX_LEN_ROUNDTRIP"
        echo
        printf '%-8s | %-9s | %-7s | %-7s | %-12s | %-8s | %-9s | %-7s | %s\n' \
            "harness" "budget(s)" "corpus" "crashes" "exec_units" "exec/s" "new_units" "rss_mb" "merge"
        printf '%-8s-+-%-9s-+-%-7s-+-%-7s-+-%-12s-+-%-8s-+-%-9s-+-%-7s-+-%s\n' \
            "--------" "---------" "-------" "-------" "------------" "------" "---------" "------" "-----"
        for name in "${HARNESS_NAMES[@]}"; do
            budget="$(harness_budget "$name")"
            corpus="$FUZZ_ROOT/$name/corpus"
            crashes="$FUZZ_ROOT/$name/crashes"
            logfile="$FUZZ_ROOT/$name/logs/fuzz.log"
            mergelog="$FUZZ_ROOT/$name/logs/merge.log"
            corpus_count=$(find "$corpus" -mindepth 1 -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
            crash_count=$(find "$crashes" -mindepth 1 -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
            exec_units="$(sum_stat_from_log "$logfile" "number_of_executed_units")"
            exec_rate="$(sum_stat_from_log "$logfile" "average_exec_per_sec")"
            new_units="$(sum_stat_from_log "$logfile" "new_units_added")"
            peak_rss="$(max_stat_from_log "$logfile" "peak_rss_mb")"
            merge_summary="$(merge_summary_from_log "$mergelog")"
            [ -n "$exec_units" ] || exec_units="-"
            [ -n "$exec_rate" ] || exec_rate="-"
            [ -n "$new_units" ] || new_units="-"
            [ -n "$peak_rss" ] || peak_rss="-"
            [ -n "$merge_summary" ] || merge_summary="-"
            printf '%-8s | %-9s | %-7s | %-7s | %-12s | %-8s | %-9s | %-7s | %s\n' \
                "$name" "$budget" "$corpus_count" "$crash_count" \
                "$exec_units" "$exec_rate" "$new_units" "$peak_rss" "$merge_summary"
            printf '  log[%s]: %s\n' "$name" "$logfile"
            if [ "$crash_count" -eq 0 ] && [ "$new_units" = "0" ]; then
                warn_found=1
                printf '  WARN[%s]: clean run but zero new units added\n' "$name"
            fi
            if [ "$corpus_count" -eq 0 ]; then
                warn_found=1
                printf '  WARN[%s]: resulting corpus is empty\n' "$name"
            fi
            if [ "$MINIMIZE" = "1" ] && [ "$merge_summary" = "-" ]; then
                warn_found=1
                printf '  WARN[%s]: merge summary missing from %s\n' "$name" "$mergelog"
            fi
        done
        echo
        if [ "$CRASH_FOUND" -eq 1 ]; then
            echo "RESULT: CRASHES FOUND - inspect $FUZZ_ROOT/*/crashes/"
        elif [ "$warn_found" -eq 1 ]; then
            echo "RESULT: clean with warnings - review stalled or low-signal harness notes above"
        else
            echo "RESULT: clean (no crash artifacts)"
        fi
    } | tee "$SUMMARY_FILE"
}

main() {
    local name bin budget harness_failed
    log "Starting overnight fuzz run; total_budget=${TOTAL_BUDGET_SECONDS}s harness_set=${HARNESS_SET}"
    ensure_dirs
    run_external_hook "restore" "$FUZZ_EXTERNAL_CORPUS_RESTORE_CMD"
    preflight

    harness_failed=0
    for name in "${HARNESS_NAMES[@]}"; do
        bin="$(harness_bin "$name")"
        budget="$(harness_budget "$name")"
        if ! run_harness "$name" "$bin" "$budget"; then
            harness_failed=1
            if [ "$KEEP_GOING" != "1" ]; then
                log "Stopping due to harness failure (KEEP_GOING=0)"
                break
            fi
        fi
    done

    if [ "$MINIMIZE" = "1" ]; then
        for name in "${HARNESS_NAMES[@]}"; do
            bin="$(harness_bin "$name")"
            minimize_corpus "$name" "$bin"
        done
    fi

    run_external_hook "sync" "$FUZZ_EXTERNAL_CORPUS_SYNC_CMD"

    write_summary

    if [ "$CRASH_FOUND" -eq 1 ] || [ "$harness_failed" -eq 1 ]; then
        exit 1
    fi
    exit 0
}

main "$@"
