#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# shellcheck disable=SC1091
source "$SCRIPT_DIR/sbom-runtime.sh"
sbom_set_error_prefix 'validate-sbom'

readonly PROJECT_URL='https://github.com/CTFfactory/salt'
readonly SALT_PURL='pkg:github/CTFfactory/salt'
readonly SALT_LICENSE='Unlicense'
readonly SALT_COPYRIGHT='Copyright 2026 CTFfactory'
readonly SALT_SUPPLIER='CTFfactory'
readonly SALT_SUMMARY='C CLI for libsodium sealed-box encryption.'
readonly LIBSODIUM_LICENSE='ISC'
readonly LIBSODIUM_HOMEPAGE='https://libsodium.org'
readonly LIBSODIUM_PURL_PREFIX='pkg:generic/libsodium@'

usage() {
  printf 'Usage: %s [--spdx PATH:ARTIFACT] [--cyclonedx PATH:ARTIFACT] [--salt-version VERSION] [--ubuntu-deps PATH] [--syft-version VERSION] [--parlay-version VERSION] [--verify-deterministic] [--component-count-min N]\n' "${0##*/}" >&2
  exit 2
}

die() {
  sbom_die "$*"
}

sbom_require_cmds jq sha1sum

salt_version='0.0.0'
ubuntu_deps_path=''
syft_version=''
parlay_version=''
verify_deterministic=false
component_count_min=90
declare -a spdx_specs=()
declare -a cdx_specs=()

while (($# > 0)); do
  case "$1" in
    --spdx)
      sbom_require_option_value "$#" "$1"
      sbom_push_repeatable spdx_specs "$2"
      shift 2
      ;;
    --cyclonedx)
      sbom_require_option_value "$#" "$1"
      sbom_push_repeatable cdx_specs "$2"
      shift 2
      ;;
    --salt-version)
      sbom_require_option_value "$#" "$1"
      salt_version="$2"
      shift 2
      ;;
    --ubuntu-deps)
      sbom_require_option_value "$#" "$1"
      ubuntu_deps_path="$2"
      shift 2
      ;;
    --syft-version)
      sbom_require_option_value "$#" "$1"
      syft_version="$2"
      shift 2
      ;;
    --parlay-version)
      sbom_require_option_value "$#" "$1"
      parlay_version="$2"
      shift 2
      ;;
    --verify-deterministic)
      verify_deterministic=true
      shift 1
      ;;
    --component-count-min)
      sbom_require_option_value "$#" "$1"
      component_count_min="$2"
      shift 2
      ;;
    --help|-h)
      usage
      ;;
    *)
      usage
      ;;
  esac
done

if [[ -n "$ubuntu_deps_path" ]]; then
  [[ -f "$ubuntu_deps_path" ]] || die "$ubuntu_deps_path: No such file"
  jq -e '.schema == "salt-ubuntu-deps-v1" and (.packages|type=="array") and (.dependencies|type=="array") and (.roots|type=="array")' "$ubuntu_deps_path" >/dev/null || die "$ubuntu_deps_path is not a supported Ubuntu deps file"
fi

if [[ -n "$syft_version" ]]; then
  jq -e --arg v "$syft_version" '(.creationInfo.creators // []) | any(.[]; . == ("Tool: syft-" + $v))' "${spdx_specs[0]%%:*}" >/dev/null 2>&1 || printf 'WARNING: SPDX SBOM creator does not list syft-%s\n' "$syft_version" >&2
fi

if [[ -n "$parlay_version" ]]; then
  jq -e --arg v "$parlay_version" '(.creationInfo.creators // []) | any(.[]; . == ("Tool: parlay-" + $v))' "${spdx_specs[0]%%:*}" >/dev/null 2>&1 || printf 'WARNING: SPDX SBOM creator does not list parlay-%s\n' "$parlay_version" >&2
fi

ubuntu_cdx_ref() {
  local name="$1" version="$2"
  if [[ -z "$version" || "$version" == 'NOASSERTION' ]]; then
    printf 'pkg:deb/ubuntu/%s' "$name"
  else
    printf 'pkg:deb/ubuntu/%s@%s' "$name" "$version"
  fi
}

artifact_profile() {
  case "$1" in
    salt)
      printf 'salt-linux-amd64.tar.gz|DYNAMIC_LINK\n'
      ;;
    salt-static)
      printf 'salt-linux-amd64.tar.gz|STATIC_LINK\n'
      ;;
    *)
      die "unsupported artifact $1"
      ;;
  esac
}

spdx_has_rel() {
  local path="$1" a="$2" b="$3" t="$4"
  jq -e --arg a "$a" --arg b "$b" --arg t "$t" '.relationships | type=="array" and any(.[]; type=="object" and .spdxElementId==$a and .relatedSpdxElement==$b and .relationshipType==$t)' "$path" >/dev/null
}

spdx_checksum_value() {
  sbom_spdx_checksum_value "$@"
}

spdx_package_verification_code() {
  sbom_spdx_verification_code "$@"
}

cdx_hash_value() {
  sbom_cdx_hash_value "$@"
}

# Verify SPDX SBOM uses deterministic documentNamespace and timestamps
verify_spdx_deterministic() {
  local path="$1"
  [[ -f "$path" ]] || die "$path: No such file"
  # Check documentNamespace doesn't contain timestamps or UUIDs
  namespace=$(jq -r '.documentNamespace // ""' "$path")
  [[ -n "$namespace" ]] || die "$path: documentNamespace is empty"
  # Warn if namespace contains timestamp patterns (YYYY-MM-DD, ISO8601, etc)
  if [[ "$namespace" =~ [0-9]{4}-[0-9]{2}-[0-9]{2} ]]; then
    printf 'WARNING: SPDX documentNamespace contains date pattern: %s\n' "$namespace" >&2
  fi
  # Check creationInfo.created is not in the future or current timestamp
  created=$(jq -r '.creationInfo.created // ""' "$path")
  [[ -n "$created" ]] || die "$path: creationInfo.created is missing"
  printf 'INFO: SPDX creationInfo.created: %s\n' "$created" >&2
}

# Verify CycloneDX SBOM uses deterministic component metadata
verify_cyclonedx_deterministic() {
  local path="$1"
  [[ -f "$path" ]] || die "$path: No such file"
  # Check metadata.component is present and reproducible
  jq -e '.metadata.component' "$path" >/dev/null || die "$path: missing metadata.component"
  # Verify no dynamic fields like timestamps in component data
  modified=$(jq -r '.metadata.component.modified // ""' "$path")
  if [[ -n "$modified" ]]; then
    printf 'INFO: CycloneDX component modified: %s\n' "$modified" >&2
  fi
}

# Validate minimum component count in SBOM
validate_component_count() {
  local path="$1" format="$2" min_count="$3"
  [[ -f "$path" ]] || die "$path: No such file"

  local actual_count
  if [[ "$format" == "spdx" ]]; then
    actual_count=$(jq '.packages | length' "$path")
  elif [[ "$format" == "cyclonedx" ]]; then
    actual_count=$(jq '.components | length' "$path")
  else
    die "Unknown format: $format"
  fi

  if [[ $actual_count -lt $min_count ]]; then
    die "$path: component count $actual_count is below minimum $min_count"
  fi
  printf 'INFO: %s has %d components (minimum: %d)\n' "$path" "$actual_count" "$min_count" >&2
}

validate_spdx() {
  local path="$1" artifact="$2" version="$3"
  local package_file_name link_relationship
  IFS='|' read -r package_file_name link_relationship < <(artifact_profile "$artifact")
  [[ -f "$path" ]] || die "$path: No such file"

  jq -e 'type=="object"' "$path" >/dev/null || die "$path is not a JSON object"
  jq -e '.spdxVersion == "SPDX-2.3"' "$path" >/dev/null || die "$path is not SPDX-2.3 JSON"
  jq -e '.dataLicense == "CC0-1.0"' "$path" >/dev/null || die "$path missing SPDX data license"
  jq -e '.SPDXID == "SPDXRef-DOCUMENT"' "$path" >/dev/null || die "$path missing document SPDXID"
  jq -e '.documentNamespace | type=="string" and length>0' "$path" >/dev/null || die "$path missing documentNamespace"
  jq -e '.creationInfo | type=="object"' "$path" >/dev/null || die "$path missing creationInfo"
  jq -e '.creationInfo.creators | type=="array" and index("Tool: salt-sbom-metadata") != null' "$path" >/dev/null || die "$path missing salt SBOM metadata creator"

  jq -e '.packages | type=="array"' "$path" >/dev/null || die "$path missing packages array"
  salt_spdx_id=$(jq -r 'first((.packages // [])[] | select(.name=="salt") | .SPDXID) // ""' "$path")
  [[ -n "$salt_spdx_id" ]] || die "$path missing salt package"

  jq -e --arg sid "$salt_spdx_id" 'if has("documentDescribes") then .documentDescribes == [$sid] else true end' "$path" >/dev/null || die "$path has unexpected documentDescribes entries"
  jq -e '.packages | first(.[] | select(.name=="salt")) | .supplier == "Organization: CTFfactory"' "$path" >/dev/null || die "$path missing salt supplier metadata"
  jq -e '.packages | first(.[] | select(.name=="salt")) | .originator == "Organization: CTFfactory"' "$path" >/dev/null || die "$path missing salt originator metadata"
  jq -e --arg v "$version" '.packages | first(.[] | select(.name=="salt")) | .versionInfo == $v' "$path" >/dev/null || die "$path missing salt version metadata"
  jq -e --arg p "$PROJECT_URL" '.packages | first(.[] | select(.name=="salt")) | .downloadLocation as $d | ($d == $p or ($d|type=="string" and startswith("git+" + $p + ".git@")))' "$path" >/dev/null || die "$path missing salt project URL"
  jq -e '.packages | first(.[] | select(.name=="salt")) | .filesAnalyzed == true' "$path" >/dev/null || die "$path salt package must have filesAnalyzed true"
  jq -e --arg f "$package_file_name" '.packages | first(.[] | select(.name=="salt")) | .packageFileName == $f' "$path" >/dev/null || die "$path missing salt package filename"
  jq -e --arg p "$PROJECT_URL" '.packages | first(.[] | select(.name=="salt")) | .homepage == $p' "$path" >/dev/null || die "$path missing salt homepage"
  jq -e '.packages | first(.[] | select(.name=="salt")) | .primaryPackagePurpose == "APPLICATION"' "$path" >/dev/null || die "$path missing salt APPLICATION package purpose"
  jq -e --arg s "$SALT_SUMMARY" '.packages | first(.[] | select(.name=="salt")) | (.summary == $s and (.description|type=="string"))' "$path" >/dev/null || die "$path missing salt summary/description"
  jq -e --arg l "$SALT_LICENSE" '.packages | first(.[] | select(.name=="salt")) | (.licenseDeclared == $l and .licenseConcluded == $l)' "$path" >/dev/null || die "$path missing salt license metadata"
  jq -e --arg l "$SALT_LICENSE" '.packages | first(.[] | select(.name=="salt")) | .licenseInfoFromFiles == [$l]' "$path" >/dev/null || die "$path missing salt licenseInfoFromFiles metadata"
  jq -e --arg c "$SALT_COPYRIGHT" '.packages | first(.[] | select(.name=="salt")) | .copyrightText == $c' "$path" >/dev/null || die "$path missing salt copyright metadata"
  jq -e --arg p "$SALT_PURL" '.packages | first(.[] | select(.name=="salt")) | .externalRefs | type=="array" and any(.[]; type=="object" and .referenceType=="purl" and .referenceLocator==$p)' "$path" >/dev/null || die "$path missing salt purl external reference"

  libsodium_spdx_id=$(jq -r 'first((.packages // [])[] | select(.name=="libsodium") | .SPDXID) // ""' "$path")
  [[ -n "$libsodium_spdx_id" ]] || die "$path missing libsodium package"
  jq -e '.packages | first(.[] | select(.name=="libsodium")) | (.supplier == "Organization: libsodium" and .originator == "Organization: libsodium")' "$path" >/dev/null || die "$path missing libsodium supplier/originator metadata"
  jq -e '.packages | first(.[] | select(.name=="libsodium")) | .filesAnalyzed == false' "$path" >/dev/null || die "$path libsodium package must be metadata-only"
  jq -e '.packages | first(.[] | select(.name=="libsodium")) | (has("licenseInfoFromFiles") or has("hasFiles") or has("packageVerificationCode")) | not' "$path" >/dev/null || die "$path libsodium metadata-only package has file-analysis fields"
  jq -e --arg h "$LIBSODIUM_HOMEPAGE" '.packages | first(.[] | select(.name=="libsodium")) | .homepage == $h' "$path" >/dev/null || die "$path missing libsodium homepage"
  jq -e '.packages | first(.[] | select(.name=="libsodium")) | .primaryPackagePurpose == "LIBRARY"' "$path" >/dev/null || die "$path missing libsodium LIBRARY package purpose"
  jq -e --arg l "$LIBSODIUM_LICENSE" '.packages | first(.[] | select(.name=="libsodium")) | (.licenseDeclared == $l and .licenseConcluded == $l)' "$path" >/dev/null || die "$path missing libsodium license metadata"
  jq -e --arg pref "$LIBSODIUM_PURL_PREFIX" '.packages | first(.[] | select(.name=="libsodium")) | .externalRefs | type=="array" and any(.[]; type=="object" and .referenceType=="purl" and (.referenceLocator|type=="string") and (.referenceLocator|startswith($pref)))' "$path" >/dev/null || die "$path missing libsodium purl external reference"

  spdx_has_rel "$path" 'SPDXRef-DOCUMENT' "$salt_spdx_id" 'DESCRIBES' || die "$path missing document DESCRIBES salt relationship"
  spdx_has_rel "$path" "$salt_spdx_id" "$libsodium_spdx_id" 'DEPENDS_ON' || die "$path missing salt DEPENDS_ON libsodium relationship"

  jq -e '.files | type=="array"' "$path" >/dev/null || die "$path missing files array"
  if jq -e '.files | any(.[]; type=="object" and .fileName == "")' "$path" >/dev/null; then
    die "$path contains empty SPDX fileName placeholder"
  fi
  placeholder_file=$(jq -r 'first((.files // [])[] | select(type=="object") | . as $f | select((($f.checksums // []) | any((.algorithm=="SHA1" and .checksumValue == ("0"*40)) or (.algorithm=="SHA256" and .checksumValue == ("0"*64))))) | ($f.fileName // "")) // ""' "$path")
  if [[ -n "$placeholder_file" ]]; then
    die "$path:$placeholder_file contains placeholder zero checksum"
  fi

  declare -a shipped_ids=()
  for file_name in "$artifact" 'salt.1'; do
    jq -e --arg n "$file_name" '.files | any(.[]; type=="object" and .fileName==$n)' "$path" >/dev/null || die "$path missing $file_name file metadata"
    file_spdx_id=$(jq -r --arg n "$file_name" 'first((.files // [])[] | select(.fileName == $n) | .SPDXID) // ""' "$path")
    [[ -n "$file_spdx_id" ]] || die "$path:$file_name missing file SPDXID"
    sha1=$(spdx_checksum_value "$path" "$file_name" 'SHA1')
    sha256=$(spdx_checksum_value "$path" "$file_name" 'SHA256')
    if ! sbom_is_sha1_nonzero "$sha1" || ! sbom_is_sha256 "$sha256"; then
      die "$path:$file_name missing non-zero SHA1/SHA256 hashes"
    fi
    jq -e --arg n "$file_name" --arg l "$SALT_LICENSE" 'first((.files // [])[] | select(.fileName==$n)) | (.licenseConcluded == $l and .licenseInfoInFiles == [$l])' "$path" >/dev/null || die "$path:$file_name missing file license metadata"
    jq -e --arg n "$file_name" --arg c "$SALT_COPYRIGHT" 'first((.files // [])[] | select(.fileName==$n)) | .copyrightText == $c' "$path" >/dev/null || die "$path:$file_name missing file copyright metadata"
    if [[ "$file_name" == *.1 ]]; then
      expected_type='DOCUMENTATION'
    else
      expected_type='BINARY'
    fi
    jq -e --arg n "$file_name" --arg t "$expected_type" 'first((.files // [])[] | select(.fileName==$n)) | .fileTypes == [$t]' "$path" >/dev/null || die "$path:$file_name missing $expected_type file type"
    spdx_has_rel "$path" "$salt_spdx_id" "$file_spdx_id" 'CONTAINS' || die "$path missing salt CONTAINS $file_name relationship"
    if [[ "$file_name" == 'salt.1' ]]; then
      spdx_has_rel "$path" "$file_spdx_id" "$salt_spdx_id" 'DOCUMENTATION_OF' || die "$path missing salt.1 DOCUMENTATION_OF salt relationship"
    else
      spdx_has_rel "$path" "$file_spdx_id" "$libsodium_spdx_id" "$link_relationship" || die "$path missing $file_name $link_relationship libsodium relationship"
    fi
    shipped_ids+=("$file_spdx_id")
  done

  expected_hasfiles=$(printf '%s\n' "${shipped_ids[@]}" | jq -R . | jq -s .)
  jq -e --argjson hf "$expected_hasfiles" '.packages | first(.[] | select(.name=="salt")) | (if has("hasFiles") then .hasFiles == $hf else true end)' "$path" >/dev/null || die "$path has unexpected salt hasFiles metadata"
  expected_verification=$(spdx_package_verification_code "$path" "$artifact" 'salt.1')
  jq -e --arg v "$expected_verification" '.packages | first(.[] | select(.name=="salt")) | .packageVerificationCode | type=="object" and .packageVerificationCodeValue == $v' "$path" >/dev/null || die "$path missing valid salt package verification code"
}

validate_cyclonedx() {
  local path="$1" artifact="$2" version="$3"
  [[ -f "$path" ]] || die "$path: No such file"
  jq -e 'type=="object"' "$path" >/dev/null || die "$path is not a JSON object"
  jq -e '.bomFormat == "CycloneDX"' "$path" >/dev/null || die "$path is not CycloneDX JSON"
  jq -e '.specVersion == "1.6"' "$path" >/dev/null || die "$path is not CycloneDX 1.6 JSON"
  jq -e '.metadata | type=="object"' "$path" >/dev/null || die "$path missing metadata"
  jq -e '.metadata.component | type=="object"' "$path" >/dev/null || die "$path missing metadata component"
  salt_ref=$(jq -r '.metadata.component["bom-ref"] // ""' "$path")
  [[ -n "$salt_ref" ]] || die "$path missing salt bom-ref"
  jq -e '.metadata.component | (.type == "application" and .name == "salt")' "$path" >/dev/null || die "$path missing salt application metadata"
  jq -e --arg v "$version" '.metadata.component.version == $v' "$path" >/dev/null || die "$path missing salt version metadata"
  jq -e --arg s "$SALT_SUPPLIER" '.metadata.component.supplier | type=="object" and .name == $s' "$path" >/dev/null || die "$path missing salt supplier metadata"
  jq -e --arg l "$SALT_LICENSE" '.metadata.component.licenses | type=="array" and any(.[]; (.license|type=="object") and .license.id == $l)' "$path" >/dev/null || die "$path missing salt license metadata"
  jq -e --arg c "$SALT_COPYRIGHT" '.metadata.component.copyright == $c' "$path" >/dev/null || die "$path missing salt copyright metadata"
  jq -e --arg p "$SALT_PURL" '.metadata.component.purl == $p' "$path" >/dev/null || die "$path missing salt purl"

  jq -e '.components | type=="array"' "$path" >/dev/null || die "$path missing components array"
  libsodium_ref=$(jq -r 'first((.components // [])[] | select(.name=="libsodium") | .["bom-ref"]) // ""' "$path")
  [[ -n "$libsodium_ref" ]] || die "$path missing libsodium component"
  jq -e --arg l "$LIBSODIUM_LICENSE" '.components | first(.[] | select(.name=="libsodium")) | (.type == "library") and (.licenses|type=="array" and any(.[]; (.license|type=="object") and .license.id == $l))' "$path" >/dev/null || die "$path missing libsodium library/license metadata"

  jq -e --arg s "$salt_ref" --arg l "$libsodium_ref" '.dependencies | type=="array" and any(.[]; type=="object" and .ref==$s and (.dependsOn|type=="array") and ((.dependsOn|index($l)) != null))' "$path" >/dev/null || die "$path missing salt dependsOn libsodium dependency"

  if [[ -n "$ubuntu_deps_path" ]]; then
    while IFS=$'\t' read -r pkg_name pkg_version; do
      [[ -n "$pkg_name" ]] || continue
      pkg_ref=$(ubuntu_cdx_ref "$pkg_name" "$pkg_version")
      jq -e --arg n "$pkg_name" --arg r "$pkg_ref" '.components | any(.[]; .name==$n and .["bom-ref"]==$r)' "$path" >/dev/null || die "$path missing Ubuntu component $pkg_name"
    done < <(jq -r '.packages[] | [.name, (.version // "NOASSERTION")] | @tsv' "$ubuntu_deps_path")

    while IFS=$'\t' read -r dep_from dep_to; do
      [[ -n "$dep_from" && -n "$dep_to" ]] || continue
      from_ver=$(jq -r --arg n "$dep_from" 'first(.packages[] | select(.name==$n) | .version) // "NOASSERTION"' "$ubuntu_deps_path")
      to_ver=$(jq -r --arg n "$dep_to" 'first(.packages[] | select(.name==$n) | .version) // "NOASSERTION"' "$ubuntu_deps_path")
      from_ref=$(ubuntu_cdx_ref "$dep_from" "$from_ver")
      to_ref=$(ubuntu_cdx_ref "$dep_to" "$to_ver")
      jq -e --arg a "$from_ref" --arg b "$to_ref" '.dependencies | type=="array" and any(.[]; .ref==$a and (.dependsOn|type=="array") and ((.dependsOn|index($b)) != null))' "$path" >/dev/null || die "$path missing Ubuntu dependency edge $dep_from -> $dep_to"
    done < <(jq -r '.dependencies[] | [.from, .to] | @tsv' "$ubuntu_deps_path")

    while IFS=$'\t' read -r root_name root_role; do
      [[ -n "$root_name" ]] || continue
      root_ver=$(jq -r --arg n "$root_name" 'first(.packages[] | select(.name==$n) | .version) // "NOASSERTION"' "$ubuntu_deps_path")
      root_ref=$(ubuntu_cdx_ref "$root_name" "$root_ver")
      if [[ "$root_role" == 'salt-build-tool' ]]; then
        jq -e --arg n "$root_name" '.components | first(.[] | select(.name==$n)) | .scope == "excluded"' "$path" >/dev/null || die "$path build-tool component $root_name must use excluded scope"
        jq -e --arg s "$salt_ref" --arg r "$root_ref" '.dependencies | type=="array" and any(.[]; .ref==$s and (.dependsOn|type=="array") and ((.dependsOn|index($r)) != null))' "$path" >/dev/null || die "$path missing salt build-tool dependency $root_name"
      fi
      if [[ "$root_role" == 'libsodium-runtime' || "$root_role" == 'libsodium-build' ]]; then
        jq -e --arg l "$libsodium_ref" --arg r "$root_ref" '.dependencies | type=="array" and any(.[]; .ref==$l and (.dependsOn|type=="array") and ((.dependsOn|index($r)) != null))' "$path" >/dev/null || die "$path missing libsodium dependency root $root_name"
      fi
    done < <(jq -r '.roots[] | [.name, .role] | @tsv' "$ubuntu_deps_path")
  fi

  for file_name in "$artifact" 'salt.1'; do
    jq -e --arg n "$file_name" '.components | any(.[]; type=="object" and .type=="file" and (.name|type=="string") and (.name==$n or (.name | endswith("/" + $n))))' "$path" >/dev/null || die "$path missing $file_name file component"
    sha1=$(cdx_hash_value "$path" "$file_name" 'SHA-1')
    sha256=$(cdx_hash_value "$path" "$file_name" 'SHA-256')
    if ! sbom_is_sha1_nonzero "$sha1" || ! sbom_is_sha256 "$sha256"; then
      die "$path:$file_name missing non-zero SHA-1/SHA-256 hashes"
    fi
    jq -e --arg n "$file_name" --arg l "$SALT_LICENSE" 'first((.components // [])[] | select(.type=="file" and (.name|type=="string") and (.name==$n or (.name | endswith("/" + $n))))) | (.licenses|type=="array" and any(.[]; (.license|type=="object") and .license.id==$l))' "$path" >/dev/null || die "$path:$file_name missing file license metadata"
    jq -e --arg n "$file_name" --arg c "$SALT_COPYRIGHT" 'first((.components // [])[] | select(.type=="file" and (.name|type=="string") and (.name==$n or (.name | endswith("/" + $n))))) | .copyright == $c' "$path" >/dev/null || die "$path:$file_name missing file copyright metadata"
    if [[ "$file_name" == *.1 ]]; then
      expected_file_type='documentation'
    else
      expected_file_type='executable'
    fi
    jq -e --arg n "$file_name" --arg t "$expected_file_type" 'first((.components // [])[] | select(.type=="file" and (.name|type=="string") and (.name==$n or (.name | endswith("/" + $n))))) | (.properties|type=="array" and any(.[]; type=="object" and .name=="salt:file-type" and .value==$t))' "$path" >/dev/null || die "$path:$file_name missing salt:file-type property with value $expected_file_type"
  done
}

for spec in "${spdx_specs[@]}"; do
  sbom_parse_path_artifact "$spec" path artifact
  validate_spdx "$path" "$artifact" "$salt_version"
  validate_component_count "$path" "spdx" "$component_count_min"
  if [[ "$verify_deterministic" == "true" ]]; then
    verify_spdx_deterministic "$path"
  fi
done
for spec in "${cdx_specs[@]}"; do
  sbom_parse_path_artifact "$spec" path artifact
  validate_cyclonedx "$path" "$artifact" "$salt_version"
  validate_component_count "$path" "cyclonedx" "$component_count_min"
  if [[ "$verify_deterministic" == "true" ]]; then
    verify_cyclonedx_deterministic "$path"
  fi
done
for spec in "${spdx_specs[@]}" "${cdx_specs[@]}"; do
  sbom_parse_path_artifact "$spec" path _artifact
  printf 'Validated: %s\n' "$path"
done
