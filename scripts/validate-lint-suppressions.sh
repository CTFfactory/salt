#!/usr/bin/env bash
# Validate that active lint suppression knobs are documented in docs/lint-suppressions.md.
set -euo pipefail

ROOT_DIR="${1:-.}"
cd "$ROOT_DIR"

LEDGER="docs/lint-suppressions.md"
CLANG_TIDY_FILE=".clang-tidy"
MAKEFILE="Makefile"
MAKEFILE_AM="Makefile.am"

fail() {
  printf 'ERROR: %s\n' "$1" >&2
  exit 1
}

[[ -f "$LEDGER" ]] || fail "missing suppression ledger: $LEDGER"
[[ -f "$CLANG_TIDY_FILE" ]] || fail "missing clang-tidy config: $CLANG_TIDY_FILE"
if [[ -f "$MAKEFILE" ]]; then
  MAKEFILE_SOURCE="$MAKEFILE"
elif [[ -f "$MAKEFILE_AM" ]]; then
  MAKEFILE_SOURCE="$MAKEFILE_AM"
else
  fail "missing makefile inputs: $MAKEFILE and $MAKEFILE_AM"
fi

# Extract disabled clang-tidy checks from the Checks block.
mapfile -t disabled_checks < <(
  awk '/^Checks:[[:space:]]*>-/{in_checks=1; next} /^WarningsAsErrors:/{in_checks=0} in_checks {print}' "$CLANG_TIDY_FILE" \
    | tr ',' '\n' \
    | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' \
    | grep '^-' \
    | sort -u
)

for check in "${disabled_checks[@]}"; do
  if ! grep -Fq -- "$check" "$LEDGER"; then
    fail "$LEDGER is missing a documented rationale for clang-tidy suppression '$check'"
  fi
done

required_makefile_flags=(
  "--suppress=missingIncludeSystem"
  "--inline-suppr"
  "--ignore-words-list="
  "--skip="
  "-disable-checker security.insecureAPI.DeprecatedOrUnsafeBufferHandling"
)

for flag in "${required_makefile_flags[@]}"; do
  if ! grep -Fq -- "$flag" "$MAKEFILE_SOURCE"; then
    fail "$MAKEFILE_SOURCE no longer contains expected suppression flag '$flag'; update validator expectations"
  fi
  if ! grep -Fq -- "$flag" "$LEDGER"; then
    fail "$LEDGER is missing a documented rationale for suppression flag '$flag'"
  fi
done

printf 'Lint suppression validation passed.\n'
