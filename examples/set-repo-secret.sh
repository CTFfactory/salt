#!/usr/bin/env bash
set -euo pipefail

readonly GITHUB_API_VERSION="2022-11-28"

for cmd in curl jq salt; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    printf 'missing required command: %s\n' "${cmd}" >&2
    exit 127
  fi
done

# Required environment variables:
#   GITHUB_TOKEN - GitHub token with repository Secrets:write permission
#   OWNER        - Repository owner
#   REPO         - Repository name
#   SECRET_NAME  - Secret name to create or update
# Provide the plaintext via exactly one of:
#   SECRET_VALUE_FILE - Path to a file whose contents are the plaintext
#                       (preferred; avoids exposing the value in the
#                       process environment, which is readable via
#                       /proc/<pid>/environ for the same UID).
#   SECRET_VALUE      - Plaintext secret value (fallback; less secure).

: "${GITHUB_TOKEN:?GITHUB_TOKEN is required}"
: "${OWNER:?OWNER is required}"
: "${REPO:?REPO is required}"
: "${SECRET_NAME:?SECRET_NAME is required}"

if [ -n "${SECRET_VALUE_FILE:-}" ] && [ -n "${SECRET_VALUE:-}" ]; then
  printf 'set only one of SECRET_VALUE_FILE or SECRET_VALUE\n' >&2
  exit 2
fi
if [ -z "${SECRET_VALUE_FILE:-}" ] && [ -z "${SECRET_VALUE:-}" ]; then
  printf 'one of SECRET_VALUE_FILE or SECRET_VALUE is required\n' >&2
  exit 2
fi
if [ -n "${SECRET_VALUE_FILE:-}" ] && [ ! -r "${SECRET_VALUE_FILE}" ]; then
  printf 'SECRET_VALUE_FILE not readable: %s\n' "${SECRET_VALUE_FILE}" >&2
  exit 2
fi

# Enforce GitHub Actions secret naming rules:
#   https://docs.github.com/en/actions/reference/security/secrets#naming-your-secrets
# Secret names may only contain [A-Za-z0-9_], must not start with a digit,
# and must not start with the GITHUB_ prefix (case-insensitive).
if ! printf '%s' "${SECRET_NAME}" | grep -Eq '^[A-Za-z_][A-Za-z0-9_]*$'; then
  printf 'invalid SECRET_NAME: must match ^[A-Za-z_][A-Za-z0-9_]*$ (alphanumerics or underscore, no leading digit)\n' >&2
  exit 2
fi
if printf '%s' "${SECRET_NAME}" | grep -Eqi '^GITHUB_'; then
  printf 'invalid SECRET_NAME: must not start with GITHUB_ prefix (reserved by GitHub Actions)\n' >&2
  exit 2
fi

# Enforce GitHub Actions per-secret value size limit of 48 KB:
#   https://docs.github.com/en/actions/reference/security/secrets#limits-for-secrets
if [ -n "${SECRET_VALUE_FILE:-}" ]; then
  SECRET_VALUE_BYTES="$(wc -c <"${SECRET_VALUE_FILE}" | tr -d ' ')"
else
  SECRET_VALUE_BYTES="$(printf '%s' "${SECRET_VALUE}" | wc -c | tr -d ' ')"
fi
if [ "${SECRET_VALUE_BYTES}" -gt 49152 ]; then
  printf 'secret value is %s bytes; GitHub Actions caps secret values at 48 KB (49152 bytes)\n' "${SECRET_VALUE_BYTES}" >&2
  exit 2
fi

# Pass the bearer token via a process-substitution header file rather than -H
# on the command line so the token is not visible in /proc/<pid>/cmdline or
# `ps -ef` to other processes running under the same UID.
auth_header_fd() { printf 'Authorization: Bearer %s\n' "${GITHUB_TOKEN}"; }

fetch_public_key_json() {
  local url="${1:?url is required}"
  local scope="${2:?scope is required}"
  local response=""
  local status=""
  local body=""

  if ! response="$(curl -sS -L -w '\n%{http_code}' \
    -H "Accept: application/vnd.github+json" \
    -H @<(auth_header_fd) \
    -H "X-GitHub-Api-Version: ${GITHUB_API_VERSION}" \
    "${url}")"; then
    printf 'failed to fetch %s public key\n' "${scope}" >&2
    return 1
  fi

  status="${response##*$'\n'}"
  body="${response%$'\n'*}"

  if [ "${status}" != "200" ]; then
    printf 'failed to fetch %s public key, status=%s\n' "${scope}" "${status}" >&2
    return 1
  fi

  printf '%s' "${body}"
}

KEY_JSON="$(fetch_public_key_json \
  "https://api.github.com/repos/${OWNER}/${REPO}/actions/secrets/public-key" \
  "repository")"

KEY_ID="$(printf '%s' "${KEY_JSON}" | jq -r '.key_id')"
PUB_KEY="$(printf '%s' "${KEY_JSON}" | jq -r '.key')"

if [ -z "${KEY_ID}" ] || [ "${KEY_ID}" = "null" ] || [ -z "${PUB_KEY}" ] || [ "${PUB_KEY}" = "null" ]; then
  printf 'failed to retrieve key_id or key from repository public key response\n' >&2
  exit 1
fi

SALT_JSON="$({
  if [ -n "${SECRET_VALUE_FILE:-}" ]; then
    salt --output json --key "${PUB_KEY}" --key-id "${KEY_ID}" - <"${SECRET_VALUE_FILE}"
  else
    printf '%s' "${SECRET_VALUE}" | salt --output json --key "${PUB_KEY}" --key-id "${KEY_ID}" -
  fi
})"

if ! printf '%s' "${SALT_JSON}" | jq -e '(.encrypted_value | strings | length > 0) and (.key_id | strings | length > 0)' >/dev/null; then
  printf 'failed to extract encrypted_value or key_id from salt output\n' >&2
  exit 1
fi

STATUS="$(printf '%s' "${SALT_JSON}" | jq -c '{encrypted_value:.encrypted_value,key_id:.key_id}' | \
  curl -sS -L -o /dev/null -w '%{http_code}' \
    -X PUT \
    -H "Accept: application/vnd.github+json" \
    -H "Content-Type: application/json" \
    -H @<(auth_header_fd) \
    -H "X-GitHub-Api-Version: ${GITHUB_API_VERSION}" \
    "https://api.github.com/repos/${OWNER}/${REPO}/actions/secrets/${SECRET_NAME}" \
    --data-binary @-)"

if [ "${STATUS}" = "201" ] || [ "${STATUS}" = "204" ]; then
  printf 'repository secret %s applied\n' "${SECRET_NAME}"
else
  printf 'repository secret apply failed, status=%s\n' "${STATUS}" >&2
  exit 1
fi
