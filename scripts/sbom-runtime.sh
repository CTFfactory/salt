#!/usr/bin/env bash
set -euo pipefail

__sbom_error_prefix='sbom'

sbom_set_error_prefix() {
  local prefix="${1:-}"
  [[ -n "$prefix" ]] || prefix='sbom'
  __sbom_error_prefix="$prefix"
}

sbom_die() {
  printf '%s: %s\n' "$__sbom_error_prefix" "$*" >&2
  exit 1
}

sbom_require_cmds() {
  local cmd
  for cmd in "$@"; do
    command -v "$cmd" >/dev/null 2>&1 || sbom_die "missing required command: $cmd"
  done
}

sbom_require_option_value() {
  local argc="$1"
  local opt="$2"
  ((argc >= 2)) || sbom_die "missing value for option: $opt"
}

sbom_push_repeatable() {
  local -n _out_array="$1"
  local value="$2"
  [[ -n "$value" ]] || sbom_die "repeatable option value must not be empty"
  _out_array+=("$value")
}

sbom_parse_path_artifact() {
  local raw="$1"
  local -n _out_path="$2"
  local -n _out_artifact="$3"
  [[ "$raw" == *:* ]] || sbom_die "expected PATH:ARTIFACT, got '$raw'"
  _out_path=${raw%%:*}
  _out_artifact=${raw#*:}
  [[ -n "$_out_path" && -n "$_out_artifact" ]] || sbom_die "expected PATH:ARTIFACT, got '$raw'"
}

sbom_parse_name_path() {
  local raw="$1"
  local -n _out_name="$2"
  local -n _out_path="$3"
  [[ "$raw" == *=* ]] || sbom_die "invalid NAME=PATH value: $raw"
  _out_name=${raw%%=*}
  _out_path=${raw#*=}
  [[ -n "$_out_name" && -n "$_out_path" ]] || sbom_die "invalid NAME=PATH value: $raw"
}

sbom_is_sha1_nonzero() {
  local value="$1"
  [[ "$value" =~ ^[[:xdigit:]]{40}$ && ! "$value" =~ ^0{40}$ ]]
}

sbom_is_sha256() {
  local value="$1"
  [[ "$value" =~ ^[[:xdigit:]]{64}$ ]]
}

sbom_spdx_checksum_value() {
  local path="$1" file_name="$2" alg="$3"
  jq -r --arg n "$file_name" --arg alg "$alg" 'first((.files // [])[] | select(.fileName == $n) | (.checksums // [])[] | select(.algorithm == $alg) | .checksumValue) // ""' "$path"
}

sbom_cdx_hash_value() {
  local path="$1" file_name="$2" alg="$3"
  jq -r --arg n "$file_name" --arg alg "$alg" 'first((.components // [])[] | select(.type=="file" and (.name|type=="string") and (.name==$n or (.name | endswith("/" + $n)))) | (.hashes // [])[] | select(.alg == $alg) | .content) // ""' "$path"
}

sbom_spdx_verification_code() {
  local path="$1"
  shift
  local joined=''
  local sha1
  while IFS= read -r sha1; do
    joined+="$sha1"
  done < <(
    local file_name
    for file_name in "$@"; do
      sbom_spdx_checksum_value "$path" "$file_name" 'SHA1'
    done | LC_ALL=C sort
  )
  printf '%s' "$joined" | sha1sum | awk '{print $1}'
}

sbom_jq_rewrite_in_place() {
  local path="$1"
  shift

  local tmp_path="${path}.tmp.$$"
  rm -f -- "$tmp_path"
  jq "$@" "$path" >"$tmp_path"
  mv -- "$tmp_path" "$path"
}

# shellcheck disable=SC2016
sbom_spdx_upsert_rel_jq_def() {
  printf '%s\n' 'def upsert_rel($a; $b; $t):'
  printf '%s\n' '  .relationships |= ((if . == null then [] elif type == "array" then . else error("SBOM relationships field is not an array") end)'
  printf '%s\n' '    | if any(.[]; .spdxElementId==$a and .relatedSpdxElement==$b and .relationshipType==$t)'
  printf '%s\n' '      then .'
  printf '%s\n' '      else . + [{spdxElementId:$a,relatedSpdxElement:$b,relationshipType:$t}]'
  printf '%s\n' '      end);'
}
