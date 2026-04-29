#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# shellcheck disable=SC1091
source "$SCRIPT_DIR/sbom-runtime.sh"
sbom_set_error_prefix 'add-ubuntu-provenance'

usage() {
  printf 'Usage: %s --deps PATH SBOM [SBOM...]\n' "${0##*/}" >&2
  exit 2
}

die() {
  sbom_die "$*"
}

sbom_require_cmds jq realpath git

deps_path=''
declare -a sbom_args=()

while (($# > 0)); do
  case "$1" in
    --deps)
      sbom_require_option_value "$#" "$1"
      deps_path="$2"
      shift 2
      ;;
    --help|-h)
      usage
      ;;
    --*)
      die "unknown option: $1"
      ;;
    *)
      sbom_args+=("$1")
      shift
      ;;
  esac
done

[[ -n "$deps_path" ]] || usage
[[ -f "$deps_path" ]] || die "$deps_path: No such file"
((${#sbom_args[@]} > 0)) || usage
jq -e '.schema == "salt-ubuntu-deps-v1" and (.packages|type=="array") and (.dependencies|type=="array")' "$deps_path" >/dev/null || die "$deps_path is not a supported deps JSON"

repo_root=$(git rev-parse --show-toplevel 2>/dev/null || pwd -P)
repo_root=$(realpath "$repo_root")

resolve_known_sbom() {
  local sbom_name="$1"
  local candidate resolved
  for candidate in "build/sbom/$sbom_name" "$sbom_name"; do
    [[ -f "$candidate" ]] || continue
    resolved=$(realpath "$candidate")
    case "$resolved" in
      "$repo_root"/*|"$repo_root")
        printf '%s\n' "$resolved"
        return 0
        ;;
      *)
        die "$candidate is outside repository root"
        ;;
    esac
  done
  die "$sbom_name not found in expected locations: build/sbom/$sbom_name, $sbom_name"
}

for sbom_name in "${sbom_args[@]}"; do
  sbom_path=$(resolve_known_sbom "$sbom_name")
  jq -e 'type == "object"' "$sbom_path" >/dev/null || die "$sbom_path is not a JSON object"

  if jq -e '.spdxVersion == "SPDX-2.3"' "$sbom_path" >/dev/null; then
    # SPDX remains runtime-focused in this repository (salt + libsodium + shipped files).
    continue
  elif jq -e '.bomFormat == "CycloneDX"' "$sbom_path" >/dev/null; then
    salt_ref=$(jq -r '.metadata.component["bom-ref"] // ""' "$sbom_path")
    libsodium_ref=$(jq -r 'first((.components // [])[] | select(.name=="libsodium") | .["bom-ref"]) // ""' "$sbom_path")
    [[ -n "$salt_ref" ]] || die "$sbom_path missing salt bom-ref"
    [[ -n "$libsodium_ref" ]] || die "$sbom_path missing libsodium component ref"

    # shellcheck disable=SC2016
    sbom_jq_rewrite_in_place "$sbom_path" \
      --argjson deps "$(cat "$deps_path")" \
      --arg salt_ref "$salt_ref" \
      --arg libsodium_ref "$libsodium_ref" '
      def cdxref($name; $version):
        if ($version // "") == "" or $version == "NOASSERTION"
        then "pkg:deb/ubuntu/" + $name
        else "pkg:deb/ubuntu/" + $name + "@" + $version
        end;
      def dep_upsert($ref; $deps):
        .dependencies |= ((if . == null then [] elif type == "array" then . else error("CycloneDX dependencies field is not an array") end)
          | map(if .ref == $ref then .dependsOn = (($deps + (.dependsOn // [])) | unique | sort) else . end)
          | if any(.[]; .ref == $ref)
            then .
            else . + [{ref:$ref, dependsOn:($deps | unique | sort)}]
            end);

      .components |= (if type == "array" then . else error("CycloneDX components field is not an array") end)
      | .dependencies |= (if . == null then [] elif type == "array" then . else error("CycloneDX dependencies field is not an array") end)
      | .components |= (
          . as $existing
          | ($deps.packages // []) as $deps_pkgs
          | reduce $deps_pkgs[] as $p ($existing;
              (cdxref($p.name; ($p.version // "NOASSERTION"))) as $ref
              | map(select(.["bom-ref"] != $ref))
              + [{
                  "bom-ref": $ref,
                  type: "library",
                  name: $p.name,
                  version: ($p.version // "NOASSERTION"),
                  scope: (if (($p.roles // []) | index("salt-build-tool")) != null then "excluded" else "required" end),
                  supplier: {name: "Ubuntu"},
                  licenses: [{license: {name: "NOASSERTION"}}],
                  purl: $ref,
                  properties: [{name: "salt:roles", value: (($p.roles // []) | join(","))}]
                }]
            )
        )
      | reduce (($deps.dependencies // [])[]) as $d (.;
          dep_upsert(cdxref($d.from; (first(($deps.packages // [])[] | select(.name == $d.from) | .version) // "NOASSERTION")); [cdxref($d.to; (first(($deps.packages // [])[] | select(.name == $d.to) | .version) // "NOASSERTION"))])
        )
      | reduce (($deps.roots // [])[] | select(.role == "salt-build-tool")) as $root (.;
          dep_upsert($salt_ref; [cdxref($root.name; (first(($deps.packages // [])[] | select(.name == $root.name) | .version) // "NOASSERTION"))])
        )
      | reduce (($deps.roots // [])[] | select(.role == "libsodium-runtime" or .role == "libsodium-build")) as $root (.;
          dep_upsert($libsodium_ref; [cdxref($root.name; (first(($deps.packages // [])[] | select(.name == $root.name) | .version) // "NOASSERTION"))])
        )
    '
  else
    die "$sbom_path is not supported SPDX 2.3 or CycloneDX JSON"
  fi
done
