#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PACK_DIR="$ROOT_DIR/.github/copilot-pack"

failures=0

log() {
  printf '%s\n' "$*"
}

fail() {
  log "ERROR: $*"
  failures=$((failures + 1))
}

check_unresolved_placeholders() {
  log "Checking for unresolved placeholders in READY files..."

  local manifest
  manifest="$PACK_DIR/MANIFEST.md"

  if [[ ! -f "$manifest" ]]; then
    fail "Missing manifest file: $manifest"
    return
  fi

  local ready_refs
  ready_refs="$(sed -n 's/^- READY: \(\.github\/[^[:space:]]*\)$/\1/p' "$manifest")"

  if [[ -z "$ready_refs" ]]; then
    fail "No READY files found in manifest."
    return
  fi

  local matches=""
  while IFS= read -r rel_path; do
    [[ -z "$rel_path" ]] && continue
    local abs_path="$ROOT_DIR/$rel_path"
    if [[ -f "$abs_path" ]]; then
      local file_matches
      file_matches="$(rg -n "\{\{[A-Z0-9_]+\}\}" "$abs_path" 2>/dev/null || true)"
      if [[ -n "$file_matches" ]]; then
        if [[ -n "$matches" ]]; then
          matches+=$'\n'
        fi
        matches+="$file_matches"
      fi
    else
      fail "READY file missing: $rel_path"
    fi
  done <<< "$ready_refs"

  if [[ -n "$matches" ]]; then
    fail "Unresolved placeholders found:"
    printf '%s\n' "$matches"
  else
    log "OK: No unresolved placeholders found."
  fi
}

check_markdown_local_links() {
  log "Checking local markdown links in copilot pack docs..."

  local md_files
  md_files="$(find "$PACK_DIR" -type f -name "*.md" | sort)"

  while IFS= read -r file; do
    [[ -z "$file" ]] && continue

    while IFS= read -r target; do
      [[ -z "$target" ]] && continue

      if [[ "$target" =~ ^https?:// ]] || [[ "$target" =~ ^mailto: ]] || [[ "$target" =~ ^# ]]; then
        continue
      fi

      local path_only
      path_only="${target%%#*}"
      path_only="${path_only//%20/ }"

      if [[ "$path_only" == /* ]]; then
        if [[ ! -e "$path_only" ]]; then
          fail "Broken absolute link in $file -> $target"
        fi
      else
        local base
        base="$(cd "$(dirname "$file")" && pwd)"
        if [[ ! -e "$base/$path_only" ]]; then
          fail "Broken relative link in $file -> $target"
        fi
      fi
    done < <(rg -o "\[[^\]]+\]\(([^)]+)\)" "$file" | sed -E 's/^.*\(([^)]+)\)$/\1/')
  done <<< "$md_files"

  if [[ "$failures" -eq 0 ]]; then
    log "OK: Local markdown links validated."
  fi
}

check_manifest_references() {
  log "Checking manifest file references..."

  local manifest
  manifest="$PACK_DIR/MANIFEST.md"

  if [[ ! -f "$manifest" ]]; then
    fail "Missing manifest file: $manifest"
    return
  fi

  local refs
  refs="$(rg -o "\.github/[A-Za-z0-9_./-]+" "$manifest" | sort -u)"

  while IFS= read -r rel_path; do
    [[ -z "$rel_path" ]] && continue
    if [[ ! -e "$ROOT_DIR/$rel_path" ]]; then
      fail "Manifest references missing file: $rel_path"
    fi
  done <<< "$refs"

  if [[ "$failures" -eq 0 ]]; then
    log "OK: Manifest references validated."
  fi
}

check_manifest_coverage() {
  log "Checking manifest coverage for all customization files..."

  local manifest
  manifest="$PACK_DIR/MANIFEST.md"

  if [[ ! -f "$manifest" ]]; then
    fail "Missing manifest file: $manifest"
    return
  fi

  local actual_file="${TMPDIR:-/tmp}/copilot_pack_actual_$$.txt"
  local manifest_file="${TMPDIR:-/tmp}/copilot_pack_manifest_$$.txt"

  {
    find "$ROOT_DIR/.github/agents" -type f -name "*.agent.md"
    find "$ROOT_DIR/.github/instructions" -type f -name "*.instructions.md"
    find "$ROOT_DIR/.github/prompts" -type f -name "*.prompt.md"
    find "$ROOT_DIR/.github/skills" -type f -name "SKILL.md"
  } | sed "s#^$ROOT_DIR/##" | sort -u > "$actual_file"

  rg -o "\.github/[A-Za-z0-9_./-]+" "$manifest" | sort -u > "$manifest_file"

  local missing
  missing="$(comm -23 "$actual_file" "$manifest_file")"
  if [[ -n "$missing" ]]; then
    fail "Manifest is missing customization files:"
    printf '%s\n' "$missing"
  fi

  local stale
  stale="$(comm -13 "$actual_file" "$manifest_file")"
  if [[ -n "$stale" ]]; then
    fail "Manifest contains unknown entries:"
    printf '%s\n' "$stale"
  fi

  rm -f "$actual_file" "$manifest_file"

  if [[ "$failures" -eq 0 ]]; then
    log "OK: Manifest coverage is complete."
  fi
}

main() {
  check_unresolved_placeholders
  check_markdown_local_links
  check_manifest_references
  check_manifest_coverage

  if [[ "$failures" -gt 0 ]]; then
    log "Validation failed with $failures issue(s)."
    exit 1
  fi

  log "Validation succeeded."
}

main "$@"
