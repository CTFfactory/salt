# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| main    | yes       |

Until the first tagged release, `main` is the only supported line for security
fixes.

## Vulnerability Reporting

Report security issues privately. Do not use public issues for vulnerability disclosure.

Preferred channel:

- GitHub Security Advisories (private report)

If Security Advisories are unavailable, contact maintainers privately through repository contact channels.

## Disclosure Timeline

- Initial acknowledgment target: within 48 hours
- Triage and impact assessment: within 5 business days
- Fix timeline: depends on severity and complexity
  - **Critical** (remote code execution, credential leakage, authentication bypass):
    patch within 7 days when practical; emergency release if actively exploited
  - **High** (privilege escalation, local secret exposure, cryptographic weakness):
    patch within 30 days
  - **Medium** (information disclosure, denial of service):
    patch in next scheduled release or within 90 days
  - **Low** (minor information leak, quality issue with security relevance):
    patch in next release cycle

Timelines are targets, not guarantees. Complex fixes, dependency constraints, or
the need for coordinated disclosure may require adjustment.

## Security Contact

Use private GitHub Security Advisory reporting for this repository:
<https://github.com/CTFfactory/salt/security/advisories/new>.

## Security Scanning

This project enforces security and safety checks through Make-driven CI gates.
`TESTING.md` is the source of truth for how those checks fit into the overall
test plan.

Aggregate gates:

- `make ci-fast`
- `make ci`

Individual checks included in those gates and used during triage:

- `make validate-copilot`
- `make actionlint`
- `make tidy` (clang-tidy static analysis)
- `make format` (clang-format compliance check)
- `make cppcheck` (cppcheck static analysis)
- `make shellcheck` (Bash script linting)
- `make codespell` (typo and term sweep)
- `make lint` (aggregates tidy, format, cppcheck, shellcheck, codespell, and `-fsyntax-only` checks)
- `make test`
- `make test-clang` (cmocka unit tests built with clang)
- `make test-sanitize`
- `make test-valgrind`
- `make coverage-check`
- `make fuzz` (libFuzzer harnesses for parse, CLI, output, boundary, and roundtrip paths)
- `make man`

Cryptographic and memory-safety hygiene is additionally enforced via repository
instructions for libsodium usage and C memory safety.

## Attack Surfaces

This repository handles attacker-influenced inputs in the CLI, in workflow
automation, and in GitHub Secrets API request flows. The sections below document
the main attack surfaces; `TESTING.md` describes how they are covered by tests
and quality gates. Passing those tests reduces risk, but does not prove the
absence of exploitable flaws.

### Public GitHub runners

- Treat public runners as zero-trust, ephemeral execution environments. Do not
  assume runner-local files, caches, artifacts, or neighboring jobs are trusted.
- Keep `GITHUB_TOKEN` and other credentials least-privileged with explicit
  workflow `permissions:` blocks; do not rely on broad defaults.
- Do not expose repository or environment secrets to untrusted fork-PR
  execution paths. Keep privileged secret access behind environment protection
  rules and trusted workflow triggers.
- Pin third-party GitHub Actions to immutable commit SHAs and review those pins
  as part of dependency maintenance.
- Never echo secrets, sensitive values, or user-controlled values to logs
  without masking. This includes plaintext, bearer tokens, and encrypted
  payloads that might be re-emitted by shell tracing or debug output.
- Treat artifacts produced by untrusted jobs as untrusted input. Do not execute
  or unpack them in privileged jobs without explicit review and validation.

### GitHub Secrets API workflows

- Fetch a fresh public key and `key_id` before each encryption flow. If GitHub
  rejects an update because the key rotated, retry only after fetching a new
  public key response.
- Scope token permissions narrowly to the secret scope being updated and avoid
  using long-lived credentials when OIDC or short-lived credentials are
  available.
- Keep `Authorization: Bearer ...` values out of process arguments. The example
  scripts use process substitution so tokens do not appear in
  `/proc/<pid>/cmdline`.
- Mask encrypted payloads before any step that could print them in workflow
  logs, even though they are ciphertext rather than plaintext.
- Validate secret names and the 48 KiB (49152-byte) plaintext size limit before
  calling `salt` so workflows fail early and deterministically.
- Prefer plain-string secret values over structured multi-field secrets because
  GitHub log redaction is less reliable for substrings embedded in structured
  values.

### Local CLI and process surfaces

- Prefer stdin over positional plaintext arguments when practical. Positional
  arguments are scrubbed in place, but stdin avoids exposing plaintext through
  the original command line in the first place.
- Keep stdout reserved for ciphertext or JSON output and stderr for deterministic
  diagnostics so shell pipelines do not accidentally mix secrets with logs.
- Keep core dumps disabled and sensitive buffers zeroed or munlocked on all exit
  paths.

## Runtime Hardening

- `salt_seal_base64()` initializes libsodium before any decode, base64 helper, or
  sealed-box operation and requires the decoded public key length to match
  `crypto_box_PUBLICKEYBYTES` exactly.
- The implementation checks every libsodium return value and fails closed on
  malformed key input, encoding failure, or encryption failure.
- Sensitive runtime buffers are locked with `sodium_mlock()` where practical and
  zeroed on release with `sodium_munlock()` or `sodium_memzero()` as required by
  the buffer type.
- The production `main()` path disables core dumps and, on Linux, disables
  `ptrace` attach via `prctl(PR_SET_DUMPABLE, 0)`.
- Positional plaintext is copied out of `argv` into locked heap memory and the
  original `argv` bytes are scrubbed in place to reduce `/proc/<pid>/cmdline`
  exposure.
- User-facing diagnostics stay deterministic and never print plaintext, decoded
  key bytes, or bearer tokens.

## Secret Handling

- GitHub automatically redacts known secret patterns in workflow logs, but
  structured values can reduce masking effectiveness. Prefer plain string secret
  values when possible.
- The example scripts in `examples/` validate secret names against GitHub's
  naming rules, reject the reserved `GITHUB_` prefix, and enforce the 48 KiB
  (49152-byte) plaintext limit before invoking `salt`.
- The example scripts pass the bearer token via process substitution so
  `Authorization: Bearer ...` does not appear in `/proc/<pid>/cmdline`.
- Use environment secrets for production credentials so jobs must satisfy
  environment protection rules before secret access.
- Prefer OIDC-based short-lived cloud credentials (AWS, Azure, GCP) instead of
  storing long-lived cloud API keys as Actions secrets.
- `salt` performs local encryption only and outputs ciphertext suitable for API
  submission; it does not send secrets to GitHub on its own.
