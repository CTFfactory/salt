#!/usr/bin/env bash
# SPDX-License-Identifier: Unlicense
#
# Black-box QA harness for the salt CLI.
#
# Exercises every documented option and flag combination against the compiled
# binary and asserts correct exit codes, stdout content, and stderr content.
# Performs end-to-end crypto roundtrip validation using the verify-roundtrip
# helper.
#
# Usage:
#   make test-qa
#   ./scripts/qa.sh          (from repository root)
#
# Prerequisites:
#   bin/salt                      (make build)
#   build/verify_roundtrip_helper (make verify-roundtrip-helper)

set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SALT_BIN=""
HELPER_BIN=""

resolve_executable() {
    local -r label="$1"
    shift
    local candidate
    for candidate in "$@"; do
        if [[ -x "$candidate" ]]; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    fail_fatal "missing ${label}; checked: $*"
}

PASS_COUNT=0
FAIL_COUNT=0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

fail_fatal() {
    printf 'qa.sh: FATAL: %s\n' "$1" >&2
    exit 1
}

SALT_BIN="$(resolve_executable "salt binary" \
    "${ROOT_DIR}/bin/salt" \
    "${ROOT_DIR}/build/autotools/salt" \
    "${ROOT_DIR}/build/salt")"
HELPER_BIN="$(resolve_executable "verify_roundtrip_helper" \
    "${ROOT_DIR}/build/verify_roundtrip_helper" \
    "${ROOT_DIR}/build/autotools/verify_roundtrip_helper" \
    "${ROOT_DIR}/verify_roundtrip_helper")"

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/salt-qa.XXXXXX")"
# shellcheck disable=SC2317  # called by trap
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

pass() {
    local id="$1"
    local desc="$2"
    printf 'PASS [%s] %s\n' "$id" "$desc"
    PASS_COUNT=$(( PASS_COUNT + 1 ))
}

fail() {
    local id="$1"
    local desc="$2"
    local reason="${3:-}"
    printf 'FAIL [%s] %s%s\n' "$id" "$desc" "${reason:+: $reason}" >&2
    FAIL_COUNT=$(( FAIL_COUNT + 1 ))
}

# assert_exit_code <id> <desc> <expected_exit> <actual_exit>
assert_exit_code() {
    local id="$1" desc="$2" expected="$3" actual="$4"
    if [[ "$actual" -ne "$expected" ]]; then
        fail "$id" "$desc" "expected exit $expected, got $actual"
        return 1
    fi
    return 0
}

# assert_stdout_nonempty <id> <desc> <stdout_file>
assert_stdout_nonempty() {
    local id="$1" desc="$2" file="$3"
    if [[ ! -s "$file" ]]; then
        fail "$id" "$desc" "stdout was empty"
        return 1
    fi
    return 0
}

# assert_stdout_contains <id> <desc> <stdout_file> <pattern>
assert_stdout_contains() {
    local id="$1" desc="$2" file="$3" pattern="$4"
    if ! grep -q -e "$pattern" "$file"; then
        fail "$id" "$desc" "stdout did not contain: $pattern"
        return 1
    fi
    return 0
}

# assert_stdout_contains_fixed <id> <desc> <stdout_file> <pattern>
assert_stdout_contains_fixed() {
    local id="$1" desc="$2" file="$3" pattern="$4"
    if ! grep -q -F -e "$pattern" "$file"; then
        fail "$id" "$desc" "stdout did not contain: $pattern"
        return 1
    fi
    return 0
}

# assert_stderr_contains <id> <desc> <stderr_file> <pattern>
assert_stderr_contains() {
    local id="$1" desc="$2" file="$3" pattern="$4"
    if ! grep -q -e "$pattern" "$file"; then
        fail "$id" "$desc" "stderr did not contain: $pattern"
        return 1
    fi
    return 0
}

# assert_stdout_empty <id> <desc> <stdout_file>
assert_stdout_empty() {
    local id="$1" desc="$2" file="$3"
    if [[ -s "$file" ]]; then
        fail "$id" "$desc" "stdout was not empty"
        return 1
    fi
    return 0
}

# assert_stderr_empty <id> <desc> <stderr_file>
assert_stderr_empty() {
    local id="$1" desc="$2" file="$3"
    if [[ -s "$file" ]]; then
        fail "$id" "$desc" "stderr was not empty: $(cat "$file")"
        return 1
    fi
    return 0
}

# run_salt_rc <stdout_file> <stderr_file> [args...]
# Writes exit code to stdout (safe under set -e).
run_salt_rc() {
    local out_file="$1"
    local err_file="$2"
    shift 2
    local rc=0
    "$SALT_BIN" "$@" >"$out_file" 2>"$err_file" || rc=$?
    printf '%s' "$rc"
}

# run_salt_rc_stdout_target <stdout_target> <stderr_file> [args...]
# Writes exit code to stdout while allowing stdout to target a specific device.
run_salt_rc_stdout_target() {
    local stdout_target="$1"
    local err_file="$2"
    shift 2
    local rc=0
    "$SALT_BIN" "$@" >"$stdout_target" 2>"$err_file" || rc=$?
    printf '%s' "$rc"
}

# generate_keypair <pub_var> <sec_var>
generate_keypair() {
    local pub_var="$1"
    local sec_var="$2"
    local keys=()
    mapfile -t keys < <("$HELPER_BIN" keygen)
    [[ "${#keys[@]}" -eq 2 ]] || fail_fatal "helper keygen returned ${#keys[@]} lines (expected 2)"
    printf -v "$pub_var" '%s' "${keys[0]}"
    printf -v "$sec_var" '%s' "${keys[1]}"
}

# decrypt_ciphertext <pub_b64> <sec_b64> <ciphertext_b64> — writes plaintext to stdout
decrypt_ciphertext() {
    local pub="$1" sec="$2" ct="$3"
    "$HELPER_BIN" decrypt --public-key "$pub" --secret-key "$sec" --ciphertext "$ct"
}

# ---------------------------------------------------------------------------
# Key pair for tests that need encryption to succeed
# ---------------------------------------------------------------------------
PUB_KEY=""
SEC_KEY=""
generate_keypair PUB_KEY SEC_KEY

# JSON key object (no key_id)
JSON_KEY_NO_ID="{\"key\":\"${PUB_KEY}\"}"

# JSON key object with key_id
JSON_KEY_WITH_ID="{\"key_id\":\"test-key-id-42\",\"key\":\"${PUB_KEY}\"}"

OUT="${TMP_DIR}/out"
ERR="${TMP_DIR}/err"

# ---------------------------------------------------------------------------
# Group 1: Meta flags
# ---------------------------------------------------------------------------

run_test_qa01() {
    local id="qa-01" desc="--help exits 0 and prints usage"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --help)"
    assert_exit_code  "$id" "$desc" 0 "$rc"    || return
    assert_stdout_contains "$id" "$desc" "$OUT" "--key" || return
    assert_stderr_empty    "$id" "$desc" "$ERR" || return
    pass "$id" "$desc"
}

run_test_qa02() {
    local id="qa-02" desc="-h exits 0 and prints usage (short form)"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -h)"
    assert_exit_code  "$id" "$desc" 0 "$rc"    || return
    assert_stdout_contains "$id" "$desc" "$OUT" "--key" || return
    pass "$id" "$desc"
}

run_test_qa03() {
    local id="qa-03" desc="--version exits 0 and prints version"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --version)"
    assert_exit_code  "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT" || return
    pass "$id" "$desc"
}

run_test_qa04() {
    local id="qa-04" desc="-V exits 0 and prints version (short form)"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -V)"
    assert_exit_code  "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT" || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 2: Basic encryption — positional plaintext
# ---------------------------------------------------------------------------

run_test_qa10() {
    local id="qa-10" desc="--key <b64> positional plaintext exits 0, non-empty base64 output"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc"    || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"      || return
    assert_stderr_empty    "$id" "$desc" "$ERR"      || return
    pass "$id" "$desc"
}

run_test_qa11() {
    local id="qa-11" desc="-k <b64> short form exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -k "$PUB_KEY" 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa12() {
    local id="qa-12" desc="Whitespace-padded base64 key is accepted (trimmed)"
    local padded="  ${PUB_KEY}  "
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$padded" 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa13() {
    local id="qa-13" desc="1-byte plaintext is accepted"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" 'x')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa14() {
    local id="qa-14" desc="Exactly 48 KB plaintext (GitHub limit) is accepted"
    local payload_file="${TMP_DIR}/payload48k"
    dd if=/dev/zero bs=1 count=49152 2>/dev/null | tr '\0' 'A' > "$payload_file"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" - < "$payload_file")"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa15() {
    local id="qa-15" desc="Interleaved options and positional arguments succeed"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" 'hello' --key "$PUB_KEY")"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa16() {
    local id="qa-16" desc="The '--' POSIX separator allows encrypting literal '--help'"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" -- --help)"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return

    local ciphertext
    ciphertext="$(cat "$OUT")"
    local decrypted
    decrypted="$(decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" "$ciphertext")"
    if [[ "$decrypted" != "--help" ]]; then
        fail "$id" "$desc" "did not cleanly encrypt literal --help string"
        return
    fi
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 3: Plaintext via stdin
# ---------------------------------------------------------------------------

run_test_qa20() {
    local id="qa-20" desc="Positional '-' reads plaintext from stdin"
    local rc
    rc="$(printf 'stdin-test' | run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" -)"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa21() {
    local id="qa-21" desc="Omitted positional reads plaintext from stdin"
    local rc
    rc="$(printf 'stdin-omit' | run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY")"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa22() {
    local id="qa-22" desc="Binary data (all 256 byte values) from stdin is accepted"
    local binary_file="${TMP_DIR}/binary256"
    awk 'BEGIN { for (i = 0; i < 256; ++i) printf "%c", i }' > "$binary_file"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" - < "$binary_file")"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa23() {
    local id="qa-23" desc="Empty stdin plaintext exits 1 with error on stderr (CWE-391)"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" - < /dev/null)"
    assert_exit_code       "$id" "$desc" 1 "$rc"    || return
    assert_stdout_empty    "$id" "$desc" "$OUT"      || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'empty' || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 4: Key via stdin
# ---------------------------------------------------------------------------

run_test_qa30() {
    local id="qa-30" desc="--key - reads key from stdin with positional plaintext"
    local rc
    rc="$(printf '%s' "$PUB_KEY" | run_salt_rc "$OUT" "$ERR" --key - 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa31() {
    local id="qa-31" desc="-k - short form reads key from stdin"
    local rc
    rc="$(printf '%s' "$PUB_KEY" | run_salt_rc "$OUT" "$ERR" -k - 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa32() {
    local id="qa-32" desc="Empty key via stdin exits 1 with error on stderr (CWE-391)"
    local rc
    rc="$(printf '' | run_salt_rc "$OUT" "$ERR" --key - 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc"    || return
    assert_stdout_empty    "$id" "$desc" "$OUT"      || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'empty' || return
    pass "$id" "$desc"
}

run_test_qa33() {
    local id="qa-33" desc="Excessive stream appended to key ingest limits safely rejecting memory exhaustion (CWE-400)"
    local oversized="${TMP_DIR}/oversized.key"
    dd if=/dev/zero bs=1024 count=100 2>/dev/null > "$oversized"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key - 'hello' < "$oversized")"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'length' || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 5: Dual-stdin rejection
# ---------------------------------------------------------------------------

run_test_qa40() {
    local id="qa-40" desc="--key - and positional '-' both from stdin is rejected (exit 1)"
    local rc
    rc="$(printf '%s' "$PUB_KEY" | run_salt_rc "$OUT" "$ERR" --key - -)"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 6: --output / -o
# ---------------------------------------------------------------------------

run_test_qa50() {
    local id="qa-50" desc="--output text (explicit) produces raw base64 on stdout"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --output text 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    # text output should NOT start with '{' (not JSON)
    if grep -q '^{' "$OUT"; then
        fail "$id" "$desc" "stdout looks like JSON, expected raw base64"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa51() {
    local id="qa-51" desc="-o text short form exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -k "$PUB_KEY" -o text 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa52() {
    local id="qa-52" desc="--output json with JSON key (has key_id) produces JSON envelope"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_WITH_ID" --output json 'hello')"
    assert_exit_code          "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains    "$id" "$desc" "$OUT" 'encrypted_value' || return
    assert_stdout_contains    "$id" "$desc" "$OUT" 'key_id'          || return
    assert_stderr_empty       "$id" "$desc" "$ERR"                   || return
    pass "$id" "$desc"
}

run_test_qa53() {
    local id="qa-53" desc="-o json short form exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -k "$JSON_KEY_WITH_ID" -o json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'encrypted_value' || return
    pass "$id" "$desc"
}

run_test_qa54() {
    local id="qa-54" desc="--output json + --key-id with plain base64 key includes key_id in output"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-id 'my-id-99' --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'my-id-99' || return
    pass "$id" "$desc"
}

run_test_qa55() {
    local id="qa-55" desc="--output json without any key_id source exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --output json 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa56() {
    local id="qa-56" desc="--output invalid exits 1 with error on stderr"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --output bogus 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa57() {
    local id="qa-57" desc="Conflicting options resolve predictably to the right-most parameter (-o json -o text)"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_WITH_ID" -o json -o text 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    # Needs to be text format, not json
    if grep -q '^{' "$OUT"; then
        fail "$id" "$desc" "stdout looks like JSON, expected raw base64 from text override"
        return
    fi
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 7: --key-id / -i
# ---------------------------------------------------------------------------

run_test_qa60() {
    local id="qa-60" desc="--key-id overrides JSON key's key_id in output"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_WITH_ID" --key-id 'override-id' --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'override-id' || return
    pass "$id" "$desc"
}

run_test_qa61() {
    local id="qa-61" desc="-i short form exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -k "$PUB_KEY" -i 'short-id' -o json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'short-id' || return
    pass "$id" "$desc"
}

run_test_qa62() {
    local id="qa-62" desc="Empty --key-id falls back to key-parsed key_id"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_WITH_ID" --key-id '' --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'test-key-id-42' || return
    pass "$id" "$desc"
}

run_test_qa63() {
    local id="qa-63" desc="--key-id with --output text is silently ignored (no error)"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-id 'ignored' --output text 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa64() {
    local id="qa-64" desc="--key-id containing quotes and backslashes is properly escaped in JSON (CWE-116)"
    local tricky_id='bad"id\test'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-id "$tricky_id" --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'bad\\"id\\\\test' || return
    pass "$id" "$desc"
}

run_test_qa65() {
    local id="qa-65" desc="--key-id exceeding 256 bytes exits 1 (CWE-119)"
    local long_id
    long_id="$(dd if=/dev/zero bs=1 count=1000 2>/dev/null | tr '\0' 'A')"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-id "$long_id" --output json 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'length' || return
    pass "$id" "$desc"
}

run_test_qa66() {
    local id="qa-66" desc="--key-id containing control bytes exits cleanly with valid JSON escaping (CWE-116)"
    local tricky_id=$'bad\n\tid\r\x07'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-id "$tricky_id" --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'bad\\n\\tid\\r\\u0007' || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 8: --key-format / -f
# ---------------------------------------------------------------------------

run_test_qa70() {
    local id="qa-70" desc="--key-format auto with plain base64 key exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-format auto 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa71() {
    local id="qa-71" desc="--key-format auto with JSON key object exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_NO_ID" --key-format auto 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa72() {
    local id="qa-72" desc="--key-format base64 with plain base64 key exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-format base64 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa73() {
    local id="qa-73" desc="--key-format base64 with JSON key object input exits 1 (forced base64 fails)"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_NO_ID" --key-format base64 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa74() {
    local id="qa-74" desc="--key-format json with JSON key object exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_NO_ID" --key-format json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa75() {
    local id="qa-75" desc="--key-format json with plain base64 key exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-format json 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa76() {
    local id="qa-76" desc="-f auto short form exits 0"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" -k "$PUB_KEY" -f auto 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa77() {
    local id="qa-77" desc="--key-format invalid exits 1 with error on stderr"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --key-format bogus 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 9: JSON key object parsing
# ---------------------------------------------------------------------------

run_test_qa80() {
    local id="qa-80" desc='JSON key {"key":"<b64>"} without key_id accepted in text mode'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_NO_ID" 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa81() {
    local id="qa-81" desc='JSON key with key_id and --output json includes key_id in envelope'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_WITH_ID" --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'test-key-id-42' || return
    pass "$id" "$desc"
}

run_test_qa82() {
    local id="qa-82" desc='JSON key with reversed field order (key_id before key) is accepted'
    local reversed_key="{\"key_id\":\"rev-id\",\"key\":\"${PUB_KEY}\"}"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$reversed_key" --output json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" 'rev-id' || return
    pass "$id" "$desc"
}

run_test_qa83() {
    local id="qa-83" desc='JSON key with unknown field is rejected (exit 1)'
    local bad_key="{\"key\":\"${PUB_KEY}\",\"extra\":\"oops\"}"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$bad_key" 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa84() {
    local id="qa-84" desc='JSON key missing "key" field is rejected (CWE-20)'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key '{"key_id":"only-id"}' 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa84a() {
    local id="qa-84a" desc='JSON key with duplicate "key" field is rejected (CWE-20)'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "{\"key\":\"${PUB_KEY}\",\"key\":\"${PUB_KEY}\"}" 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'duplicate field' || return
    pass "$id" "$desc"
}

run_test_qa84b() {
    local id="qa-84b" desc='JSON key with duplicate "key_id" field is rejected (CWE-20)'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "{\"key\":\"${PUB_KEY}\",\"key_id\":\"a\",\"key_id\":\"b\"}" 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'duplicate field' || return
    pass "$id" "$desc"
}

run_test_qa84c() {
    local id="qa-84c" desc='JSON key with empty "key" string is rejected (CWE-20)'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key '{"key":""}' 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'must not be empty' || return
    pass "$id" "$desc"
}

run_test_qa85() {
    local id="qa-85" desc='Empty JSON object {} is rejected (CWE-20)'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key '{}' 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa86() {
    local id="qa-86" desc='Malformed JSON {key: value} (unquoted field) is rejected (exit 1)'
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key '{key: value}' 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa87() {
    local id="qa-87" desc="Whitespace-heavy JSON key correctly parses (CWE-20)"
    local ws_key
    ws_key="$(printf '{\n\t   \n  "key": \n\t   "%s"\n\n\n}' "$PUB_KEY")"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$ws_key" 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_nonempty "$id" "$desc" "$OUT"   || return
    pass "$id" "$desc"
}

run_test_qa88() {
    local id="qa-88" desc="Truncated unclosed JSON mapping safely rejects (CWE-20)"
    local bad_key="{\"key_id\":\"short\",\"key\":\"${PUB_KEY}\""
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$bad_key" 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'invalid' || return
    pass "$id" "$desc"
}

run_test_qa88a() {
    local id="qa-88a" desc="JSON key object with trailing garbage is rejected (CWE-20)"
    local trailing_key="{\"key\":\"${PUB_KEY}\"}garbage_trailing_data"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$trailing_key" 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'invalid' || return
    pass "$id" "$desc"
}

run_test_qa88b() {
    local id="qa-88b" desc="JSON key containing embedded null bytes evaluates safely without misinterpretation (CWE-115)"
    local rc
    rc="$(printf '{"key_id":"test\0null", "key":"%s"}' "$PUB_KEY" | run_salt_rc "$OUT" "$ERR" --key - -o json 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'string' || return
    pass "$id" "$desc"
}

run_test_qa88c() {
    local id="qa-88c" desc="Deeply nested JSON objects safely rejected without recursion exhaustion (CWE-674)"
    local rc
    rc="$(python3 -c "print('{\"key\": ' * 100 + '\"test\"' + '}' * 100)" | run_salt_rc "$OUT" "$ERR" --key - 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'string' || return
    pass "$id" "$desc"
}

run_test_qa88d() {
    local id="qa-88d" desc="Unterminated JSON string safely bounces without out-of-bounds read (CWE-125/CWE-787)"
    local rc
    rc="$(printf '{"key": "abc' | run_salt_rc "$OUT" "$ERR" --key - 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'string' || return
    pass "$id" "$desc"
}

run_test_qa89() {
    local id="qa-89" desc="UTF-8 and Emojis map directly into JSON emission unmodified"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" -i '🚀-id' -o json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains "$id" "$desc" "$OUT" '🚀-id' || return
    pass "$id" "$desc"
}

run_test_qa89a() {
    local id="qa-89a" desc="JSON key string containing unescaped control character is rejected (RFC 8259)"
    local bad_key="{\"key\":\"${PUB_KEY}\",\"key_id\":\"bad"$'\n'"\"}"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$bad_key" 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'string' || return
    pass "$id" "$desc"
}

run_test_qa89b() {
    local id="qa-89b" desc="JSON key string containing valid escape sequences parses and emits correctly (CWE-116)"
    local escaped_key="{\"key\":\"${PUB_KEY}\",\"key_id\":\"escaped\\\\n\\\\t\\\\r\\\\\\\\\\\"/\"}"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$escaped_key" -o json 'hello')"
    assert_exit_code       "$id" "$desc" 0 "$rc" || return
    assert_stdout_contains_fixed "$id" "$desc" "$OUT" 'escaped\\n\\t\\r\\\\\"/' || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 10: Error conditions and exit codes
# ---------------------------------------------------------------------------

run_test_qa90() {
    local id="qa-90" desc="Missing --key exits 1 with error mentioning --key on stderr"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" 'hello')"
    assert_exit_code       "$id" "$desc" 1 "$rc"    || return
    assert_stdout_empty    "$id" "$desc" "$OUT"      || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'key' || return
    pass "$id" "$desc"
}

run_test_qa91() {
    local id="qa-91" desc="Invalid base64 key exits 1 with error on stderr"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key 'NOT_VALID_BASE64!!!@@##' 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa92() {
    local id="qa-92" desc="Base64 key decoding to wrong length exits 1"
    # Valid base64 but only 4 bytes when decoded — wrong key length
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key 'AAAA' 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa92a() {
    local id="qa-92a" desc="Base64 key decoding to >32 bytes exits 1"
    # 33 zero bytes encoded to base64
    local over_key="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$over_key" 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa93() {
    local id="qa-93" desc="Empty positional plaintext exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" '')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa94() {
    local id="qa-94" desc="Plaintext > 48 KB exits 1 with size error on stderr (CWE-119)"
    local oversized="${TMP_DIR}/oversized"
    # 49153 bytes = 48 KB + 1 byte over GitHub limit
    dd if=/dev/zero bs=1 count=49153 2>/dev/null | tr '\0' 'B' > "$oversized"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" - < "$oversized")"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa95() {
    local id="qa-95" desc="Unknown long option --bogus exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" --bogus 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa96() {
    local id="qa-96" desc="Unknown short option -Z exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" -Z 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa97() {
    local id="qa-97" desc="Too many positional arguments exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" 'hello' 'extra')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa98() {
    local id="qa-98" desc="Missing argument to --key exits 1"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key)"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa99() {
    local id="qa-99" desc="Output write failure exits 4 and reports stderr diagnostics"
    local rc
    rc="$(run_salt_rc_stdout_target /dev/full "$ERR" --key "$PUB_KEY" 'hello')"
    assert_exit_code       "$id" "$desc" 4 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'failed to write encrypted output' || return
    pass "$id" "$desc"
}

run_test_qa99a() {
    local id="qa-99a" desc="SIGTERM during stdin ingestion exits 5 (CWE-755)"
    local rc=0

    "$SALT_BIN" --key "$PUB_KEY" - >"$OUT" 2>"$ERR" < <(sleep 5) &
    local pid=$!
    sleep 0.2
    kill -TERM "$pid"
    wait "$pid" || rc=$?

    assert_exit_code       "$id" "$desc" 5 "$rc" || return
    assert_stdout_empty    "$id" "$desc" "$OUT" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'operation interrupted' || return
    pass "$id" "$desc"
}

run_test_qa99b() {
    local id="qa-99b" desc="SIGPIPE from closed stdout pipe exits gracefully (CWE-755)"
    local payload_file="${TMP_DIR}/payload_pipe"
    dd if=/dev/zero bs=1 count=49152 2>/dev/null | tr '\0' 'A' > "$payload_file"

    # Test piping into head -c 0
    local rc=0
    "$SALT_BIN" --key "$PUB_KEY" - < "$payload_file" 2>"$ERR" | head -c 0 >/dev/null || rc="${PIPESTATUS[0]}"

    # 141 is SIGPIPE. salt may also report graceful stream failure as SALT_ERR_IO (4),
    # and some shells/stdio paths may normalize to 1 or 0.
    if [[ "$rc" -ne 141 ]] && [[ "$rc" -ne 4 ]] && [[ "$rc" -ne 1 ]] && [[ "$rc" -ne 0 ]]; then
        fail "$id" "$desc" "expected SIGPIPE (141) or graceful exit (4/1/0), got $rc"
        return
    fi

    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 11: Output correctness — crypto roundtrip
# ---------------------------------------------------------------------------

run_test_qa100() {
    local id="qa-100" desc="text output decrypts to original plaintext (roundtrip)"
    local plaintext="roundtrip-qa-test-payload"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" "$plaintext")"
    assert_exit_code    "$id" "$desc" 0 "$rc" || return

    local ciphertext
    ciphertext="$(cat "$OUT")"
    local decrypted
    decrypted="$(decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" "$ciphertext")"
    if [[ "$decrypted" != "$plaintext" ]]; then
        fail "$id" "$desc" "decrypted text does not match original"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa101() {
    local id="qa-101" desc="json output encrypted_value decrypts correctly (roundtrip)"
    local plaintext="json-roundtrip-payload"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$JSON_KEY_WITH_ID" --output json "$plaintext")"
    assert_exit_code "$id" "$desc" 0 "$rc" || return

    # Extract encrypted_value from JSON using portable string parsing
    local enc_value
    enc_value="$(grep -o '"encrypted_value":"[^"]*"' "$OUT" | sed 's/"encrypted_value":"//;s/"//')"
    if [[ -z "$enc_value" ]]; then
        fail "$id" "$desc" "could not extract encrypted_value from JSON output"
        return
    fi

    local decrypted
    decrypted="$(decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" "$enc_value")"
    if [[ "$decrypted" != "$plaintext" ]]; then
        fail "$id" "$desc" "decrypted JSON payload does not match original"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa102() {
    local id="qa-102" desc="Binary payload (stdin) roundtrip decrypts byte-for-byte"
    local binary_file="${TMP_DIR}/binary_rt"
    local decrypted_file="${TMP_DIR}/binary_rt_dec"
    awk 'BEGIN { for (i = 1; i < 256; ++i) printf "%c", i }' > "$binary_file"

    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" - < "$binary_file")"
    assert_exit_code "$id" "$desc" 0 "$rc" || return

    local ciphertext
    ciphertext="$(cat "$OUT")"
    decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" "$ciphertext" > "$decrypted_file"
    if ! cmp -s "$binary_file" "$decrypted_file"; then
        fail "$id" "$desc" "decrypted binary payload mismatch"
        return
    fi
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 12: Stdout/stderr hygiene
# ---------------------------------------------------------------------------

run_test_qa110() {
    local id="qa-110" desc="Successful encryption produces nothing on stderr"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" 'hello')"
    assert_exit_code    "$id" "$desc" 0 "$rc" || return
    assert_stderr_empty "$id" "$desc" "$ERR"  || return
    pass "$id" "$desc"
}

run_test_qa111() {
    local id="qa-111" desc="Error path produces nothing on stdout"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" 'hello')"
    assert_exit_code    "$id" "$desc" 1 "$rc" || return
    assert_stdout_empty "$id" "$desc" "$OUT"  || return
    pass "$id" "$desc"
}

run_test_qa112() {
    local id="qa-112" desc="--help output goes to stdout, nothing on stderr"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --help)"
    assert_exit_code    "$id" "$desc" 0 "$rc" || return
    assert_stderr_empty "$id" "$desc" "$ERR"  || return
    assert_stdout_nonempty "$id" "$desc" "$OUT" || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Group 13: Cryptographic correctness — boundary sizes and negative checks
# ---------------------------------------------------------------------------

run_test_qa120() {
    local id="qa-120" desc="48 KB boundary payload (49151 bytes) roundtrip decrypts byte-for-byte"
    local boundary_file="${TMP_DIR}/boundary_48k"
    local decrypted_file="${TMP_DIR}/boundary_48k_dec"
    awk 'BEGIN { for (i = 0; i < 49151; ++i) printf "%c", ((i * 73) % 256) }' > "$boundary_file"

    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" - < "$boundary_file")"
    assert_exit_code "$id" "$desc" 0 "$rc" || return

    local ciphertext
    ciphertext="$(cat "$OUT")"
    decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" "$ciphertext" > "$decrypted_file"
    if ! cmp -s "$boundary_file" "$decrypted_file"; then
        fail "$id" "$desc" "decrypted boundary payload mismatch"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa121() {
    local id="qa-121" desc="Decryption with wrong private key fails"
    # shellcheck disable=SC2034
    local _pub2="" sec2=""
    generate_keypair _pub2 sec2

    local rc ciphertext
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" 'wrong-key-check')"
    assert_exit_code "$id" "$desc" 0 "$rc" || return
    ciphertext="$(cat "$OUT")"

    if decrypt_ciphertext "$PUB_KEY" "$sec2" "$ciphertext" >/dev/null 2>&1; then
        fail "$id" "$desc" "decrypt with wrong private key unexpectedly succeeded"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa122() {
    local id="qa-122" desc="Tampered ciphertext is rejected by decryption"
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$PUB_KEY" 'tamper-check')"
    assert_exit_code "$id" "$desc" 0 "$rc" || return

    local ciphertext first replacement tampered
    ciphertext="$(cat "$OUT")"
    first="${ciphertext:0:1}"
    replacement='A'; [[ "$first" == 'A' ]] && replacement='B'
    tampered="${replacement}${ciphertext:1}"

    if decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" "$tampered" >/dev/null 2>&1; then
        fail "$id" "$desc" "tampered ciphertext unexpectedly decrypted"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa123() {
    local id="qa-123" desc="Malformed ciphertext '@@@' is rejected by decryption"
    if decrypt_ciphertext "$PUB_KEY" "$SEC_KEY" '@@@' >/dev/null 2>&1; then
        fail "$id" "$desc" "malformed ciphertext unexpectedly decrypted"
        return
    fi
    pass "$id" "$desc"
}

run_test_qa124() {
    local id="qa-124" desc="Mathematically invalid all-zero public key safely rejected by crypto subsystem (CWE-345)"
    local weak_key="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
    local rc
    rc="$(run_salt_rc "$OUT" "$ERR" --key "$weak_key" 'hello')"
    assert_exit_code       "$id" "$desc" 2 "$rc" || return
    assert_stderr_contains "$id" "$desc" "$ERR" 'encrypt' || return
    pass "$id" "$desc"
}

# ---------------------------------------------------------------------------
# Run all tests
# ---------------------------------------------------------------------------

run_test_qa01
run_test_qa02
run_test_qa03
run_test_qa04

run_test_qa10
run_test_qa11
run_test_qa12
run_test_qa13
run_test_qa14
run_test_qa15
run_test_qa16

run_test_qa20
run_test_qa21
run_test_qa22
run_test_qa23

run_test_qa30
run_test_qa31
run_test_qa32
run_test_qa33

run_test_qa40

run_test_qa50
run_test_qa51
run_test_qa52
run_test_qa53
run_test_qa54
run_test_qa55
run_test_qa56
run_test_qa57

run_test_qa60
run_test_qa61
run_test_qa62
run_test_qa63
run_test_qa64
run_test_qa65
run_test_qa66

run_test_qa70
run_test_qa71
run_test_qa72
run_test_qa73
run_test_qa74
run_test_qa75
run_test_qa76
run_test_qa77

run_test_qa80
run_test_qa81
run_test_qa82
run_test_qa83
run_test_qa84
run_test_qa84a
run_test_qa84b
run_test_qa84c
run_test_qa85
run_test_qa86
run_test_qa87
run_test_qa88
run_test_qa88a
run_test_qa88b
    run_test_qa88c
    run_test_qa88d
run_test_qa89
run_test_qa89a
run_test_qa89b

run_test_qa90
run_test_qa91
run_test_qa92
run_test_qa92a
run_test_qa93
run_test_qa94
run_test_qa95
run_test_qa96
run_test_qa97
run_test_qa98
run_test_qa99
run_test_qa99a
run_test_qa99b

run_test_qa100
run_test_qa101
run_test_qa102

run_test_qa110
run_test_qa111
run_test_qa112

run_test_qa120
run_test_qa121
run_test_qa122
run_test_qa123
    run_test_qa124

TOTAL=$(( PASS_COUNT + FAIL_COUNT ))
printf '\n'
printf 'qa.sh: %d/%d tests passed\n' "$PASS_COUNT" "$TOTAL"

if [[ "$FAIL_COUNT" -gt 0 ]]; then
    printf 'qa.sh: %d test(s) FAILED\n' "$FAIL_COUNT" >&2
    exit 1
fi

exit 0
