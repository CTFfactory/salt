#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: collect-ubuntu-deps.sh --output PATH [--root PACKAGE:ROLE ...]

Collect deterministic Ubuntu package dependency metadata for SBOM enrichment.
Each --root entry marks a package as a policy root with an associated role.
EOF
  exit 2
}

die() {
  printf 'collect-ubuntu-deps: %s\n' "$*" >&2
  exit 1
}

require_cmds() {
  local cmd
  for cmd in "$@"; do
    command -v "$cmd" >/dev/null 2>&1 || die "missing required command: $cmd"
  done
}

require_cmds jq apt-cache dpkg-query mktemp

output_path=''
declare -a root_specs=()

while (($# > 0)); do
  case "$1" in
    --output)
      (($# >= 2)) || die "missing value for --output"
      output_path="$2"
      shift 2
      ;;
    --root)
      (($# >= 2)) || die "missing value for --root"
      root_specs+=("$2")
      shift 2
      ;;
    --help|-h)
      usage
      ;;
    *)
      die "unknown option: $1"
      ;;
  esac
done

[[ -n "$output_path" ]] || usage

if ((${#root_specs[@]} == 0)); then
  root_specs+=(
    'libsodium23:libsodium-runtime'
    'libsodium-dev:libsodium-build'
    'gcc:salt-build-tool'
    'make:salt-build-tool'
    'libc6-dev:salt-build-tool'
  )
fi

declare -A pkg_seen=()
declare -A role_seen=()
declare -A edge_seen=()
declare -A queue_seen=()
declare -a queue=()
declare -a root_items=()
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT
pkg_file="$tmp_dir/packages.tsv"
edge_file="$tmp_dir/edges.tsv"
root_file="$tmp_dir/roots.tsv"
role_file="$tmp_dir/roles.tsv"

normalize_pkg() {
  local pkg="$1"
  pkg=${pkg##*:}
  printf '%s' "$pkg"
}

pkg_version() {
  local pkg="$1"
  local version=''
  if version=$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null); then
    printf '%s' "$version"
    return 0
  fi
  version=$(apt-cache policy "$pkg" 2>/dev/null | awk '/Candidate:/ {print $2; exit}')
  if [[ -n "$version" && "$version" != "(none)" ]]; then
    printf '%s' "$version"
    return 0
  fi
  printf 'NOASSERTION'
}

pkg_depends() {
  local pkg="$1"
  apt-cache depends --important "$pkg" 2>/dev/null \
    | awk '/^\s*Depends: /{print $2}' \
    | sed 's/:any$//' \
    | grep -Ev '^(<|libc-dev$)' \
    | LC_ALL=C sort -u || true
}

for spec in "${root_specs[@]}"; do
  [[ "$spec" == *:* ]] || die "invalid --root value: $spec (expected PACKAGE:ROLE)"
  pkg=$(normalize_pkg "${spec%%:*}")
  role=${spec#*:}
  [[ -n "$pkg" && -n "$role" ]] || die "invalid --root value: $spec"
  root_items+=("$pkg:$role")
  key="$pkg|$role"
  if [[ -z "${role_seen[$key]:-}" ]]; then
    role_seen["$key"]=1
    printf '%s\t%s\n' "$pkg" "$role" >>"$role_file"
  fi
  if [[ -z "${queue_seen[$pkg]:-}" ]]; then
    queue_seen["$pkg"]=1
    queue+=("$pkg")
  fi
done

while ((${#queue[@]} > 0)); do
  pkg="${queue[0]}"
  queue=("${queue[@]:1}")
  [[ -z "${pkg_seen[$pkg]:-}" ]] || continue
  pkg_seen["$pkg"]=1

  version=$(pkg_version "$pkg")
  printf '%s\t%s\n' "$pkg" "$version" >>"$pkg_file"

  while IFS= read -r dep; do
    dep=$(normalize_pkg "$dep")
    [[ -n "$dep" ]] || continue
    edge="$pkg|$dep"
    if [[ -z "${edge_seen[$edge]:-}" ]]; then
      edge_seen["$edge"]=1
      printf '%s\t%s\n' "$pkg" "$dep" >>"$edge_file"
    fi
    if [[ -z "${pkg_seen[$dep]:-}" && -z "${queue_seen[$dep]:-}" ]]; then
      queue_seen["$dep"]=1
      queue+=("$dep")
    fi
  done < <(pkg_depends "$pkg")
done

: >"$root_file"
for item in "${root_items[@]}"; do
  printf '%s\t%s\n' "${item%%:*}" "${item#*:}" >>"$root_file"
done

jq -n \
  --rawfile packages_raw "$pkg_file" \
  --rawfile edges_raw "$edge_file" \
  --rawfile roles_raw "$role_file" \
  --rawfile roots_raw "$root_file" '
  def lines($s):
    ($s | split("\n") | map(select(length > 0)));
  def parse_tsv($line):
    ($line | split("\t"));

  (lines($roles_raw) | map(parse_tsv(.)) | reduce .[] as $r ({}; .[$r[0]] += [$r[1]])) as $roles_by_pkg
  | (lines($roots_raw) | map(parse_tsv(.)) | map({name: .[0], role: .[1]}) | sort_by(.name, .role)) as $roots
  | (lines($packages_raw) | map(parse_tsv(.)) | map({
      name: .[0],
      version: .[1],
      roles: (($roles_by_pkg[.[0]] // []) | unique | sort)
    }) | sort_by(.name)) as $packages
  | (lines($edges_raw) | map(parse_tsv(.)) | map({from: .[0], to: .[1], type: "DEPENDS_ON"}) | unique | sort_by(.from, .to)) as $deps
  | {
      schema: "salt-ubuntu-deps-v1",
      distro: "ubuntu",
      roots: $roots,
      packages: $packages,
      dependencies: $deps
    }
' >"$output_path"

printf 'Wrote: %s\n' "$output_path"
