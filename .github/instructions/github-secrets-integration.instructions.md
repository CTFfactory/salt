---
description: 'Patterns for GitHub Actions Secrets API integration using salt for encryption'
applyTo: '**/examples/**/*.sh,**/README.md,**/SECURITY.md'
---

# GitHub Secrets Integration

Use these rules when documenting or implementing GitHub Actions Secrets API workflows with `salt`.

Authoritative reference:
[GitHub Actions secrets reference](https://docs.github.com/en/actions/reference/security/secrets).

## GitHub-imposed limits and naming rules

Validate every secret name and value against these rules before any encryption
or upload step (see [Naming your secrets](https://docs.github.com/en/actions/reference/security/secrets#naming-your-secrets)
and [Limits for secrets](https://docs.github.com/en/actions/reference/security/secrets#limits-for-secrets)).

- Secret names:
  - May only contain `[A-Za-z0-9_]`. Spaces and other punctuation are forbidden.
  - Must not start with a digit.
  - Must not start with the `GITHUB_` prefix (case-insensitive). The prefix is
    reserved by GitHub Actions.
  - Are case-insensitive when referenced and stored uppercase.
  - Must be unique within their scope (repository, environment, organization,
    or enterprise).
- Secret values:
  - Are capped at 48 KB (49152 bytes) per secret. Reject larger values up
    front rather than producing ciphertext that the API will refuse.
  - For payloads larger than 48 KB, follow the
    [storing large secrets](https://docs.github.com/en/actions/how-tos/security-for-github-actions/security-guides/using-secrets-in-github-actions#storing-large-secrets)
    guidance instead of raising the limit.
- Quantity limits (informational; relevant when bulk-provisioning):
  - 100 repository secrets, 100 environment secrets per environment, 1,000
    organization secrets. Workflows can read at most 100 organization secrets
    in a single run.
- Secret precedence when names collide: environment beats repository beats
  organization. Reuse the same name across scopes only when this precedence is
  intentional.
- Avoid storing structured data (multi-field JSON, key=value pairs) as a single
  secret value; GitHub's log redaction may fail to mask substrings.

The `salt` CLI enforces the 48 KB plaintext cap via
`SALT_GITHUB_SECRET_MAX_VALUE_LEN`. Bash example scripts in `examples/` perform
the matching name and size validation before calling `salt`.

## Endpoint variants

- Repository secrets:
  - Public key: `GET /repos/{owner}/{repo}/actions/secrets/public-key`
  - Upsert secret: `PUT /repos/{owner}/{repo}/actions/secrets/{secret_name}`
- Environment secrets:
  - Public key: `GET /repos/{owner}/{repo}/environments/{environment_name}/secrets/public-key`
  - Upsert secret: `PUT /repos/{owner}/{repo}/environments/{environment_name}/secrets/{secret_name}`
  - `environment_name` must be URL-encoded in the path.
- Organization secrets:
  - Public key: `GET /orgs/{org}/actions/secrets/public-key`
  - Upsert secret: `PUT /orgs/{org}/actions/secrets/{secret_name}`
  - Request body must include `visibility`.

## Required request shape

- Fetch a fresh public key response before each encryption flow.
- Extract both `key` and `key_id` from the public-key response.
- Encrypt with `salt` locally and submit payload with:
  - `encrypted_value`
  - `key_id`
- Include `X-GitHub-Api-Version: 2022-11-28` and `Accept: application/vnd.github+json` headers on every call.

## Safety and logging

- Never print plaintext secret values or access tokens to stdout.
- In workflow contexts, add a mask before any potential output expansion:
  - `echo "::add-mask::${ENCRYPTED_VALUE}"`
- Treat only HTTP `201` and `204` as success for secret upsert calls.
- Treat all other HTTP status codes as failures and exit non-zero.

## Shell conventions

- Use `set -euo pipefail` in all integration scripts.
- Parse JSON with `jq` rather than ad-hoc string processing.
- Keep stderr messages deterministic and concise.
