#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# shellcheck disable=SC1091
source "$SCRIPT_DIR/sbom-runtime.sh"
sbom_set_error_prefix 'add-libsodium-to-sbom'

readonly LIBSODIUM_SPDX_ID="SPDXRef-Package-libsodium"
readonly LIBSODIUM_PURL_PREFIX="pkg:generic/libsodium@"
readonly SALT_PURL="pkg:github/CTFfactory/salt"
readonly LIBSODIUM_DOWNLOAD_LOCATION="https://download.libsodium.org/libsodium/releases/"
readonly LIBSODIUM_HOMEPAGE="https://libsodium.org"
readonly SALT_SUMMARY="C CLI for libsodium sealed-box encryption."
readonly SALT_DESCRIPTION="salt encrypts plaintext with libsodium sealed boxes and emits base64 ciphertext or a JSON envelope for GitHub Actions Secrets API workflows."
readonly LIBSODIUM_SUMMARY="Portable sodium cryptography library."
readonly LIBSODIUM_DESCRIPTION="libsodium is the NaCl-compatible cryptography library used by salt for public-key sealed-box encryption."
readonly POST_PROCESSOR_CREATOR="Tool: salt-sbom-metadata"
readonly SPDX_FILE_LICENSE_COMMENT="License assertion is inherited from the release package metadata for this generated artifact."

usage() {
  printf 'Usage: %s [options] SBOM [SBOM...]\n' "${0##*/}" >&2
  exit 2
}

die() {
  sbom_die "$*"
}

sbom_require_cmds jq sha1sum realpath git

project_url='https://github.com/CTFfactory/salt'
license_id='Unlicense'
copyright_text='Copyright 2026 CTFfactory'
libsodium_version=''
salt_version='0.0.0'
source_ref=''
declare -a artifact_file_raw=()
declare -a sbom_args=()

while (($# > 0)); do
  case "$1" in
    --project-url)
      sbom_require_option_value "$#" "$1"
      project_url="$2"
      shift 2
      ;;
    --license)
      sbom_require_option_value "$#" "$1"
      license_id="$2"
      shift 2
      ;;
    --copyright)
      sbom_require_option_value "$#" "$1"
      copyright_text="$2"
      shift 2
      ;;
    --version)
      sbom_require_option_value "$#" "$1"
      libsodium_version="$2"
      shift 2
      ;;
    --salt-version)
      sbom_require_option_value "$#" "$1"
      salt_version="$2"
      shift 2
      ;;
    --source-ref)
      sbom_require_option_value "$#" "$1"
      source_ref="$2"
      shift 2
      ;;
    --artifact-file)
      sbom_require_option_value "$#" "$1"
      sbom_push_repeatable artifact_file_raw "$2"
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

[[ -n "$libsodium_version" ]] || die "the following arguments are required: --version"
((${#sbom_args[@]} > 0)) || usage

declare -a artifact_names=()
declare -a artifact_paths=()
for item in "${artifact_file_raw[@]}"; do
  sbom_parse_name_path "$item" name path
  artifact_names+=("$name")
  artifact_paths+=("$path")
done

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

spdx_profile() {
  local sbom_name="${1##*/}"
  case "$sbom_name" in
    salt-linux-amd64.spdx.json)
      printf 'salt-linux-amd64|salt-linux-amd64.tar.gz|salt|DYNAMIC_LINK\n'
      ;;
    salt-static-linux-amd64.spdx.json)
      printf 'salt-static-linux-amd64|salt-linux-amd64.tar.gz|salt-static|STATIC_LINK\n'
      ;;
    *)
      die "$1 is not an SPDX release SBOM"
      ;;
  esac
}

spdx_sha_pair_ok() {
  local sbom_path="$1"
  local file_name="$2"
  local sha1 sha256
  sha1=$(sbom_spdx_checksum_value "$sbom_path" "$file_name" 'SHA1')
  sha256=$(sbom_spdx_checksum_value "$sbom_path" "$file_name" 'SHA256')
  sbom_is_sha1_nonzero "$sha1" && sbom_is_sha256 "$sha256"
}

cdx_sha_pair_ok() {
  local sbom_path="$1"
  local file_name="$2"
  local sha1 sha256
  sha1=$(sbom_cdx_hash_value "$sbom_path" "$file_name" 'SHA-1')
  sha256=$(sbom_cdx_hash_value "$sbom_path" "$file_name" 'SHA-256')
  sbom_is_sha1_nonzero "$sha1" && sbom_is_sha256 "$sha256"
}

spdx_verification_code() {
  sbom_spdx_verification_code "$@"
}

for sbom_name in "${sbom_args[@]}"; do
  sbom_path=$(resolve_known_sbom "$sbom_name")
  jq -e 'type == "object"' "$sbom_path" >/dev/null || die "$sbom_path is not a JSON object"

  if jq -e '.spdxVersion == "SPDX-2.3"' "$sbom_path" >/dev/null; then
    IFS='|' read -r document_name package_file_name binary_name link_relationship < <(spdx_profile "$sbom_name")
    salt_spdx_id=$(jq -r '.packages | if type != "array" then error("SBOM missing packages array") else . end | first(map(select(.name == "salt" and (.SPDXID|type=="string") and (.SPDXID|length>0)))[] | .SPDXID) // error("SBOM missing salt package")' "$sbom_path")

    shipped_json='[]'
    declare -a shipped_names=()
    for i in "${!artifact_names[@]}"; do
      file_name="${artifact_names[$i]}"
      file_path="${artifact_paths[$i]}"
      [[ -f "$file_path" ]] || die "$file_path is not a file"
      jq -e --arg n "$file_name" '.files | type == "array" and any(.[]; type=="object" and .fileName == $n)' "$sbom_path" >/dev/null || die "SBOM missing file entry for $file_name"
      spdx_sha_pair_ok "$sbom_path" "$file_name" || die "SBOM has incomplete Syft-generated checksums for $file_name"
      file_spdx_id=$(jq -r --arg n "$file_name" 'first((.files // [])[] | select(.fileName == $n) | .SPDXID) // ""' "$sbom_path")
      [[ -n "$file_spdx_id" ]] || die "SBOM missing file entry for $file_name"
      if [[ "$file_name" == *.1 ]]; then
        file_type='DOCUMENTATION'
        file_comment='Release man page.'
      else
        file_type='BINARY'
        file_comment='Release executable.'
      fi
      shipped_json=$(jq -c --arg n "$file_name" --arg id "$file_spdx_id" --arg t "$file_type" --arg c "$file_comment" '. + [{name:$n,id:$id,fileType:$t,comment:$c}]' <<<"$shipped_json")
      shipped_names+=("$file_name")
    done

    verification_code=$(spdx_verification_code "$sbom_path" "${shipped_names[@]}")
    if [[ -n "$source_ref" ]]; then
      download_location="git+${project_url}.git@${source_ref}"
      source_info="Built from the CTFfactory/salt repository release staging layout at ${source_ref}."
    else
      download_location="$project_url"
      source_info='Built from the CTFfactory/salt repository release staging layout.'
    fi
    libsodium_ref="${LIBSODIUM_PURL_PREFIX}${libsodium_version}"
    libsodium_source_info="External cryptographic library dependency used by salt for sealed-box encryption; represented as a metadata-only package for the ${link_relationship,,} release artifact."

    # shellcheck disable=SC2016
    sbom_jq_rewrite_in_place "$sbom_path" --arg document_name "$document_name" \
      --arg package_file_name "$package_file_name" \
      --arg salt_spdx_id "$salt_spdx_id" \
      --arg post_creator "$POST_PROCESSOR_CREATOR" \
      --arg project_url "$project_url" \
      --arg license_id "$license_id" \
      --arg copyright_text "$copyright_text" \
      --arg salt_version "$salt_version" \
      --arg download_location "$download_location" \
      --arg source_info "$source_info" \
      --arg salt_summary "$SALT_SUMMARY" \
      --arg salt_description "$SALT_DESCRIPTION" \
      --arg salt_purl "$SALT_PURL" \
      --arg libsodium_spdx_id "$LIBSODIUM_SPDX_ID" \
      --arg libsodium_version "$libsodium_version" \
      --arg libsodium_download "$LIBSODIUM_DOWNLOAD_LOCATION" \
      --arg libsodium_homepage "$LIBSODIUM_HOMEPAGE" \
      --arg libsodium_ref "$libsodium_ref" \
      --arg libsodium_summary "$LIBSODIUM_SUMMARY" \
      --arg libsodium_description "$LIBSODIUM_DESCRIPTION" \
      --arg libsodium_source_info "$libsodium_source_info" \
      --arg verification_code "$verification_code" \
      --arg file_license_comment "$SPDX_FILE_LICENSE_COMMENT" \
      --arg binary_name "$binary_name" \
      --arg link_relationship "$link_relationship" \
      --argjson shipped "$shipped_json" '
      def upsert_rel($a; $b; $t):
        .relationships |= ((if . == null then [] elif type == "array" then . else error("SBOM relationships field is not an array") end)
          | if any(.[]; .spdxElementId==$a and .relatedSpdxElement==$b and .relationshipType==$t)
            then .
            else . + [{spdxElementId:$a,relatedSpdxElement:$b,relationshipType:$t}]
            end);
      .name = $document_name
      | .documentDescribes = [$salt_spdx_id]
      | .comment = "Artifact-scoped SPDX SBOM for \($package_file_name); generated from the release staging layout and augmented with deterministic salt/libsodium metadata."
      | .creationInfo |= (if . == null then {} elif type == "object" then . else error("SPDX creationInfo field is not an object") end)
      | .creationInfo.creators |= ((if . == null then [] elif type == "array" then . else error("SPDX creationInfo.creators field is not an array") end)
          | if index($post_creator) == null then . + [$post_creator] else . end)
      | .files |= (if type == "array" then . else error("SBOM missing files array") end)
      | .files |= map(
          . as $f
          | if ($f.fileName == "" and (($f.checksums // []) | any(.checksumValue == ("0" * 40)))) then empty
            else (($shipped | map(select(.name == $f.fileName)) | .[0]) // null) as $sf
            | if $sf == null then .
              else .licenseConcluded = $license_id
                | .licenseInfoInFiles = [$license_id]
                | .licenseComments = $file_license_comment
                | .copyrightText = $copyright_text
                | .fileTypes = [$sf.fileType]
                | .comment = $sf.comment
              end
            end
        )
      | .packages |= (if type == "array" then . else error("SBOM packages field is not an array") end)
      | .packages |= map(
          if .name == "salt" then
            .versionInfo = $salt_version
            | .packageFileName = $package_file_name
            | .supplier = "Organization: CTFfactory"
            | .originator = "Organization: CTFfactory"
            | .downloadLocation = $download_location
            | .filesAnalyzed = true
            | .hasFiles = ($shipped | map(.id))
            | .packageVerificationCode = {packageVerificationCodeValue:$verification_code}
            | .homepage = $project_url
            | .sourceInfo = $source_info
            | .licenseConcluded = $license_id
            | .licenseDeclared = $license_id
            | .licenseInfoFromFiles = [$license_id]
            | .copyrightText = $copyright_text
            | .summary = $salt_summary
            | .description = $salt_description
            | .comment = "Release artifact package containing the salt executable and man page; external runtime/build dependency metadata is represented separately."
            | .primaryPackagePurpose = "APPLICATION"
            | .externalRefs |= ((if . == null then [] elif type == "array" then . else error("salt package externalRefs field is not an array") end)
                | if any(.[]; .referenceCategory == "PACKAGE-MANAGER" and .referenceType == "purl" and .referenceLocator == $salt_purl)
                  then .
                  else . + [{referenceCategory:"PACKAGE-MANAGER",referenceType:"purl",referenceLocator:$salt_purl}]
                  end)
          else . end
        )
      | .packages |= (map(select(.SPDXID != $libsodium_spdx_id)) + [{
          name:"libsodium",
          SPDXID:$libsodium_spdx_id,
          versionInfo:$libsodium_version,
          supplier:"Organization: libsodium",
          originator:"Organization: libsodium",
          downloadLocation:$libsodium_download,
          filesAnalyzed:false,
          homepage:$libsodium_homepage,
          sourceInfo:$libsodium_source_info,
          licenseConcluded:"ISC",
          licenseDeclared:"ISC",
          copyrightText:"NOASSERTION",
          summary:$libsodium_summary,
          description:$libsodium_description,
          comment:"libsodium is declared explicitly because the minimal release staging layout does not contain package-manager metadata Syft can infer.",
          primaryPackagePurpose:"LIBRARY",
          externalRefs:[{referenceCategory:"PACKAGE-MANAGER",referenceType:"purl",referenceLocator:$libsodium_ref}]
        }])
      | upsert_rel("SPDXRef-DOCUMENT"; $salt_spdx_id; "DESCRIBES")
      | upsert_rel($salt_spdx_id; $libsodium_spdx_id; "DEPENDS_ON")
      | reduce ($shipped[]) as $sf (.;
          upsert_rel($salt_spdx_id; $sf.id; "CONTAINS")
          | if $sf.name == "salt.1" then upsert_rel($sf.id; $salt_spdx_id; "DOCUMENTATION_OF") else . end
          | if $sf.name == $binary_name then upsert_rel($sf.id; $libsodium_spdx_id; $link_relationship) else . end
        )
    '

  elif jq -e '.bomFormat == "CycloneDX"' "$sbom_path" >/dev/null; then
    for i in "${!artifact_names[@]}"; do
      file_name="${artifact_names[$i]}"
      file_path="${artifact_paths[$i]}"
      [[ -f "$file_path" ]] || die "$file_path is not a file"
      jq -e --arg n "$file_name" '.components | type == "array" and any(.[]; type == "object" and .type == "file" and (.name|type=="string") and (.name == $n or (.name | endswith("/" + $n))))' "$sbom_path" >/dev/null || die "CycloneDX SBOM missing file component for $file_name"
      cdx_sha_pair_ok "$sbom_path" "$file_name" || die "CycloneDX SBOM has incomplete Syft-generated hashes for $file_name"
    done

    artifact_json='[]'
    for file_name in "${artifact_names[@]}"; do
      artifact_json=$(jq -c --arg n "$file_name" '. + [$n]' <<<"$artifact_json")
    done
    libsodium_ref="${LIBSODIUM_PURL_PREFIX}${libsodium_version}"

    # shellcheck disable=SC2016
    sbom_jq_rewrite_in_place "$sbom_path" --arg project_url "$project_url" \
      --arg license_id "$license_id" \
      --arg copyright_text "$copyright_text" \
      --arg salt_version "$salt_version" \
      --arg salt_purl "$SALT_PURL" \
      --arg libsodium_ref "$libsodium_ref" \
      --arg libsodium_version "$libsodium_version" \
      --arg libsodium_download "$LIBSODIUM_DOWNLOAD_LOCATION" \
      --argjson artifact_names "$artifact_json" '
      .metadata |= (if . == null then {} elif type == "object" then . else error("CycloneDX metadata field is not an object") end)
      | .metadata.component |= (if . == null then {} elif type == "object" then . else error("CycloneDX metadata.component field is not an object") end)
      | .metadata.component["bom-ref"] |= (. // $salt_purl)
      | .metadata.component.type = "application"
      | .metadata.component.name = "salt"
      | .metadata.component.version = $salt_version
      | .metadata.component.supplier = {name:"CTFfactory"}
      | .metadata.component.licenses = [{license:{id:$license_id}}]
      | .metadata.component.copyright = $copyright_text
      | .metadata.component.purl = $salt_purl
      | .metadata.component.externalReferences |= ((if . == null then [] elif type == "array" then . else error("CycloneDX metadata.component externalReferences field is not an array") end)
          | if any(.[]; .type == "vcs" and .url == $project_url) then . else . + [{type:"vcs",url:$project_url}] end)
      | .components |= (if type == "array" then . else error("CycloneDX components field is not an array") end)
      | .components |= map(
          . as $c
          | if ($c.type == "file" and ($c.name|type=="string") and any($artifact_names[]; . as $n | ($c.name == $n or ($c.name | endswith("/" + $n))))) then
              (any($artifact_names[]; . as $n | (($c.name == $n or ($c.name | endswith("/" + $n))) and ($n | endswith(".1"))))) as $is_man_page
              | .licenses = [{license:{id:$license_id}}]
              | .copyright = $copyright_text
              | .properties = ((.properties // []) + (if $is_man_page then [{name:"salt:file-type",value:"documentation"}] else [{name:"salt:file-type",value:"executable"}] end))
            else .
            end
        )
      | .components |= (map(select(.name != "libsodium" and .["bom-ref"] != $libsodium_ref)) + [{
          "bom-ref":$libsodium_ref,
          type:"library",
          name:"libsodium",
          version:$libsodium_version,
          supplier:{name:"libsodium"},
          licenses:[{license:{id:"ISC"}}],
          purl:$libsodium_ref,
          externalReferences:[{type:"distribution",url:$libsodium_download}]
        }])
      | .dependencies |= (if . == null then [] elif type == "array" then . else error("CycloneDX dependencies field is not an array") end)
      | (.metadata.component["bom-ref"]) as $salt_ref
      | .dependencies |= (map(select(.ref != $salt_ref and .ref != $libsodium_ref)) + [{ref:$salt_ref,dependsOn:[$libsodium_ref]},{ref:$libsodium_ref,dependsOn:[]}])
    '
  else
    die "$sbom_path is not supported SPDX 2.3 or CycloneDX JSON"
  fi
done
