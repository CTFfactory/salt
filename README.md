         ________________________________                     ____________________________
        /  _    /  _____/  _    /  _____/ ___  _______       /___/  _    /  __   /  _____/
       /   /   /   /   /   /   / \   \    | | / / ___/      /   /   /   /   ____/ \   \
      /   /   /   /   /   /   /___\   \   | |/ /_\  \      /   /   /   /   /_______\   \
     /   ____/___/   /_______/________/   |___/_____/ ____/   /_______/_______/________/
    /___/                                            /_______/

      ________    __    ___ ___________
     /"       )  /""\  |"  ("      _  ")
    (:   \___/  /    \ ||  |)__/  \\__/
     \___  \   /' /\  \|:  |   \\_ /
      __/  \\ //  __'  \\  |___|.  |
     /" \   :)   /  \\  \\_|:  \:  |
    (_______/___/    \___)______)__|

# salt - Sodium Asymmetric/Anonymous Locking Tool

**`salt`** is a lightweight CLI tool that leverages libsodium's sealed-box primitives. It encrypts data using only a recipient's public key, so only the holder of the corresponding private key can decrypt the value.

["If I have seen further, it is by standing on the shoulders of giants."](https://en.wikipedia.org/wiki/Standing_on_the_shoulders_of_giants) -- Sir Isaac Newton

## Overview

`salt` is designed for workflows like GitHub Actions Secrets where you encrypt locally with a public key and send encrypted data to a remote API. Sealed boxes require only the recipient's public key at encryption time, which avoids sender-side key-management complexity in the CLI.

## Why salt?

- Uses audited libsodium primitives (`crypto_box_seal`) instead of custom crypto.
- Supports both raw base64 keys and JSON key payloads used by API workflows.
- Produces deterministic, parseable stderr messages and stable exit codes for automation.

## Installation

For installation and build instructions, use **[INSTALL.md](INSTALL.md)**.

## Quick Start

Encrypt plaintext with a base64 public key:

```sh
salt --key "2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234" "hello-world"
```

Encrypt for JSON API submission:

```sh
salt --output json --key '{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}' 'hello-world'
```

## Demo

Render terminal demo assets (GIF and MP4) from `vhs/salt.tape`:

```sh
vhs vhs/salt.tape
```

This requires [`vhs`](https://github.com/charmbracelet/vhs) to be installed and available on `PATH`.

Output files are written to `vhs/salt-demo.gif` and `vhs/salt-demo.mp4`.

Validate tape syntax without rendering:

```sh
vhs validate vhs/salt.tape
```

## Behavior

- Unix-style option parsing with short and long flags
- key input can be base64, JSON key object, or stdin
- JSON key objects are strict: only `key` and optional `key_id` are accepted
- plaintext input can be positional arg or stdin
- plaintext length must be between 1 byte and 48 KiB (49152 bytes) to match GitHub Actions' per-secret value limit
([reference](https://docs.github.com/en/actions/reference/security/secrets#limits-for-secrets));
the `salt` implementation accepts up to 1 MiB outside that workflow-oriented CLI guard
- stdin key input is capped at 16 KiB
- output: base64 ciphertext (`text`) or REST-ready JSON (`json`) to stdout
- errors: deterministic messages to stderr

## Build Requirements

Build requirements are documented in **[INSTALL.md](INSTALL.md)**.

## Testing

For testing and fuzzing workflows, use **[TESTING.md](TESTING.md)**.

## Usage

```sh
salt [OPTIONS] [PLAINTEXT]
```

### Options

| Long | Short | Required | Description |
|------|-------|----------|-------------|
| `--key VALUE` | `-k VALUE` | Yes | Public key input. `VALUE` may be a base64 key, a JSON object string with `key` and optional `key_id`, or `-` to read key JSON/base64 from stdin. |
| `--key-id ID` | `-i ID` | No | Key ID override/injector (required for `--output json` when key source does not provide `key_id`). |
| `--output text\|json` | `-o text\|json` | No | Output mode. Default: `text`. |
| `--key-format auto\|base64\|json` | `-f auto\|base64\|json` | No | Key parser mode. Default: `auto`. |
| `--help` | `-h` | No | Print usage and exit 0. |
| `--version` | `-V` | No | Print version and exit 0. |

### Input rules

- If `PLAINTEXT` is omitted or `-`, plaintext is read from stdin.
- If `--key -` is used, key input is read from stdin.
- Key and plaintext cannot both be read from stdin in one invocation.
- `--key-format auto` inspects the trimmed key input and treats a leading `{` as JSON; use `--key-format base64` or `--key-format json` when you want to force one parser path during automation or debugging.

> **Security note:** Sensitive plaintext should be passed via stdin. Positional arguments are visible in process listings (`ps`, `/proc/<pid>/cmdline`).

### Examples

Base64 key + positional plaintext (text mode):

```sh
salt -k "2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234" "Hello World!"
```

JSON key object + JSON output:

```sh
salt --output json --key '{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}' 'Hello World!' | jq -c '.'
```

JSON key from stdin:

```sh
echo '{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}' | salt --output json --key - "Hello World!" | jq -c '.'
```

Value from stdin with JSON key:

```sh
echo 'Hello World!' | salt --output json --key '{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}' - | jq -c '.'
```

Base64 key + explicit key-id for JSON output:

```sh
salt --output json --key "2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234" --key-id "012345678912345678" "Hello World!" | jq -c '.'
```

## Configuration

`salt` itself does not use configuration files or environment variables. The CLI is configured entirely through flags plus stdin for key/plaintext input.

The helper scripts under `examples/` use environment variables such as `GITHUB_TOKEN`, `OWNER`, `REPO`, `ORG`, `ENVIRONMENT_NAME`, `SECRET_NAME`, `SECRET_VALUE`, and `SECRET_VALUE_FILE` because they orchestrate GitHub REST API calls around the `salt` binary.

## Output Formats

| Format | Description | When to use |
|--------|-------------|-------------|
| `text` | Prints the base64-encoded ciphertext on a single line. | Shell pipelines or when another tool already knows the `key_id`. |
| `json` | Prints `{"encrypted_value":"...","key_id":"..."}`. | GitHub Actions Secrets API payload assembly and other REST workflows. |

`json` output requires a `key_id` from JSON key input or `--key-id`.

### Choosing text vs JSON output

| Input format | Output needed | Use this command pattern |
|--------------|---------------|--------------------------|
| Base64 key only | Base64 ciphertext for a pipeline | `salt -k "<base64-key>" "plaintext"` |
| Base64 key + known key_id | JSON for REST API | `salt --output json -k "<base64-key>" -i "<key-id>" "plaintext"` |
| JSON key object | JSON for REST API | `salt --output json -k '{"key_id":"...","key":"..."}' "plaintext"` |
| JSON key object | Base64 ciphertext only | `salt -k '{"key_id":"...","key":"..."}' "plaintext"` |

### Key format examples

**Base64 key** (32 bytes base64-encoded):
```
2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234
```

**JSON key object** (strict RFC 8259, only `key` and `key_id` fields):
```json
{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}
```

**Key from stdin** (use `-` to read key input from stdin instead of command line).

## Supported Platforms

The table below documents tested and validated build environments. Other Linux distributions and toolchains with C11 support, libsodium 1.0.18+, and POSIX APIs are expected to work but are not covered by CI gates.

| Environment | amd64 | Notes |
|-------------|-------|-------|
| Generic Linux source build | ✅ | Build with the canonical GNU flow (`./configure && make && make check`) when libsodium and cmocka development packages are available. |
| Ubuntu 22.04 container | ✅ | Tested via `docker/ubuntu/22/Dockerfile`. |
| Ubuntu 24.04 container | ✅ | Tested via `docker/ubuntu/24/Dockerfile`; main CI host environment. |
| Ubuntu 26.04 container | ✅ | Tested via `docker/ubuntu/26/Dockerfile`. |
| Release archives | ✅ | `salt-linux-amd64.tar.gz` (template layout rooted with docs, scripts, coverage HTML, man page, and both `salt` and `salt-static` binaries in `bin/`), Ubuntu 22/24/26 `.deb` packages, and matching SPDX JSON/CycloneDX JSON SBOMs. |

## Exit Codes

- `0`: success
- `1`: input or usage error
- `2`: cryptographic error
- `3`: internal error
- `4`: runtime I/O or output-stream failure
- `5`: signal-driven interruption

## Error Handling

- `1` groups caller and input problems such as missing options, malformed JSON key input, invalid public-key encoding, and empty plaintext.
- `2` is reserved for libsodium-backed failures such as initialization or sealed-box encryption failure.
- `3` covers allocation and other internal failures where the program cannot safely continue.
- `4` reports runtime stdin/stdout/stderr failures such as broken pipes, `/dev/full`, or truncated output streams.
- `5` reports signal-driven interruption observed through the CLI signal path.
- Deterministic stderr text still carries the detailed diagnosis, while the numeric exit codes stay stable enough for shell and CI automation to branch without scraping wording.

## Man Page

The manual page source is located at `man/salt.1` and validated with:

```sh
make man
```

## CI/CD

GitHub Actions is used for CI/CD:

- `.github/workflows/ci.yml` runs `make ci-fast` for the smoke gate and `make ci` for the full GNU Autotools gate.
- `.github/workflows/release.yml` runs only for SemVer tags matching `v*`, reuses the same out-of-tree GNU flow (`./bootstrap`, `../../configure --enable-tests`, `make ci`), and then publishes template-layout `salt-linux-amd64.tar.gz` (containing both `salt` and `salt-static` under `salt.tar/bin/`), Ubuntu 22/24/26 `.deb` packages, matching SPDX JSON and CycloneDX JSON SBOMs, and `SHA256SUMS`; CycloneDX SBOM enrichment includes Ubuntu package/version provenance for build inputs (for example `gcc`, `clang`, `make`, `libc-bin`/`ldd`, `binutils`, `autoconf`, `automake`, `libtool`, `pkg-config`, and libsodium packages), while SPDX remains focused on runtime package and shipped-file metadata.

## Verifying Release Integrity

Release integrity can be verified by validating SBOMs and release checksums. This section covers how auditors and security teams can reproduce and cross-verify release artifacts.

### Validating SBOMs with Syft

Each release publishes four SBOMs (Software Bill of Materials):
- `salt-linux-amd64.spdx.json` and `salt-linux-amd64.cyclonedx.json` (dynamic binary)
- `salt-static-linux-amd64.spdx.json` and `salt-static-linux-amd64.cyclonedx.json` (static binary)

To regenerate and validate an SBOM locally using Syft 1.44.0:

```sh
# Install Syft 1.44.0 pinned by SHA256
SYFT_VERSION="1.44.0"
SYFT_SHA256="0e91737aee2b5baf1d255b959630194a302335d848ff97bb07921eb6205b5f5a"
# See .github/workflows/release.yml for exact SHA256 and download URL

# Extract release staging binaries and regenerate SPDX/CycloneDX formats
syft dir:./salt-linux-amd64 \
  --source-name salt \
  --source-supplier CTFfactory \
  --source-version v0.0.0 \
  -o spdx-json=salt-linux-amd64-local.spdx.json \
  -o cyclonedx-json=salt-linux-amd64-local.cyclonedx.json
```

### Cross-Verifying SBOM Checksums

After generating SBOMs locally, verify they match the released checksums:

```sh
# Download release SHA256SUMS
curl -sS -L https://github.com/CTFfactory/salt/releases/download/v0.0.0/SHA256SUMS \
  | grep -E '(spdx|cyclonedx)' | sha256sum -c -

# The output should confirm all SBOM checksums match
```

### Security Scanning with Snyk and Trivy

Release SBOMs can be scanned for known vulnerabilities using Snyk or Trivy:

```sh
# Snyk scan (requires Snyk account)
snyk sbom test salt-linux-amd64.cyclonedx.json

# Trivy scan (no account required)
trivy sbom salt-linux-amd64.spdx.json
```

For guidance on SBOM standards and generation, see `.github/instructions/sbom.instructions.md`.

## Troubleshooting

- `error: --key/-k is required`:
  Provide `--key`, either base64 text, JSON object, or `-` for stdin key input.
- `error: --key/-k value must not be empty`:
  `--key` was supplied with an empty string. Pass a non-empty key value, JSON object, or `-` for stdin.
- `error: invalid option: <name>`:
  An unrecognized short or long option was passed. Re-run with `--help` to see the supported flags.
- `error: key and plaintext cannot both be read from stdin`:
  Use stdin for only one input source per invocation.
- `error: invalid public key encoding or length`:
  Ensure key decodes to exactly `crypto_box_PUBLICKEYBYTES` bytes.
- `error: JSON output requires key_id`:
  Provide `--key-id` or pass JSON key input containing `key_id`.
- `error: plaintext must not be empty`:
  Provide a non-empty positional plaintext argument or non-empty stdin input.
- `error: invalid output format 'X' (expected text|json)`:
  Use `--output text` or `--output json`.
- `error: invalid key format 'X' (expected auto|base64|json)`:
  Use `--key-format auto`, `--key-format base64`, or `--key-format json`.
- `error: stdin input exceeds maximum length (N bytes)`:
  Keep stdin payload within limits (plaintext up to 48 KiB / 49152 bytes to
  satisfy GitHub Actions' secret-value cap, key input up to 16 KiB).
- **mlock() failures or `RLIMIT_MEMLOCK` warnings**:
  `salt` attempts to lock sensitive buffers in resident memory using `sodium_mlock()`.
  If the system `RLIMIT_MEMLOCK` is too low (common in some container runtimes),
  locking fails non-fatally and `salt` falls back to zeroing memory on release with
  `sodium_memzero()`. This does not affect correctness, but means transient
  plaintext and key material may be paged to swap. To eliminate the warning or
  increase security, raise the limit with `ulimit -l unlimited` (requires privilege)
  or configure your container runtime to allow higher locked-memory limits.

## Design Decisions

- Base64 ciphertext output is default because target APIs and shell pipelines commonly expect text payloads.
- The public library API intentionally stays base64-oriented; callers that need raw binary sealed boxes should call libsodium `crypto_box_seal()` directly instead of going through `salt`.
- Strict JSON key parsing rejects unknown fields and malformed objects to avoid ambiguous input handling.
- Error taxonomy is intentionally coarse at the CLI boundary to keep scripting behavior predictable.
- CLI signal handling is process-wide by design, so the embedded CLI entry points are not reentrant across concurrent threads in the same process.

## GitHub Actions Secrets Workflow

Use this flow whenever you need to store a secret through the GitHub REST API:

1. Fetch a target public key and `key_id`.
2. Encrypt locally with `salt`.
3. Pipe `salt` JSON into `jq` and `curl --data-binary @-` so the encrypted value, key ID, and final JSON request body are not placed on command lines.

Every API call below should include:

- `Accept: application/vnd.github+json`
- `Authorization: Bearer ${GITHUB_TOKEN}`
- `X-GitHub-Api-Version: 2022-11-28`

**API Version Policy**: This documentation and the example scripts reference GitHub REST API version `2022-11-28`, which is the current stable version for Actions Secrets endpoints as of 2026-04-29. Consult the [GitHub REST API versioning documentation](https://docs.github.com/en/rest/about-the-rest-api/api-versions) if newer versions introduce changes to request/response formats or required headers.

The examples below and the packaged `examples/set-*-secret.sh` scripts use a small helper plus process substitution (`-H @<(...)`) so `GITHUB_TOKEN` does not appear in `/proc/<pid>/cmdline`. The packaged scripts also accept a `SECRET_VALUE_FILE` environment variable so plaintext does not enter `/proc/<pid>/environ`; prefer it over `SECRET_VALUE` for real secrets.

Token permissions by scope:

| Secret scope | Fine-grained token permission | Classic token scope |
|---|---|---|
| Repository | `Secrets` repository permission: `write` | `repo` for private repos; `public_repo` for public repos |
| Environment | `Environments` repository permission: `write` | `repo` |
| Organization | `Secrets` organization permission: `write` | `admin:org` |

GitHub Actions secret values are capped at 48 KiB (49152 bytes). `salt` and the helper scripts enforce this workflow limit before encryption so an oversized plaintext fails locally instead of producing an encrypted payload that GitHub will reject.

### Repository secret

The packaged helper performs the full fetch/encrypt/PUT flow:

```sh
export GITHUB_TOKEN OWNER REPO SECRET_NAME
export SECRET_VALUE_FILE=./secret-value.txt
./examples/set-repo-secret.sh
```

Equivalent request-shaping pattern for custom automation:

```sh
set -euo pipefail

readonly GITHUB_API_VERSION="2022-11-28"
auth_header_fd() { printf 'Authorization: Bearer %s\n' "${GITHUB_TOKEN}"; }

KEY_RESPONSE="$(curl -sS -L -w '\n%{http_code}' \
  -H "Accept: application/vnd.github+json" \
  -H @<(auth_header_fd) \
  -H "X-GitHub-Api-Version: ${GITHUB_API_VERSION}" \
  "https://api.github.com/repos/${OWNER}/${REPO}/actions/secrets/public-key")"
KEY_STATUS="${KEY_RESPONSE##*$'\n'}"
KEY_JSON="${KEY_RESPONSE%$'\n'*}"
if [ "${KEY_STATUS}" != "200" ]; then
  printf 'failed to fetch repository public key, status=%s\n' "${KEY_STATUS}" >&2
  exit 1
fi

KEY_ID="$(printf '%s' "${KEY_JSON}" | jq -r '.key_id')"
PUB_KEY="$(printf '%s' "${KEY_JSON}" | jq -r '.key')"
SALT_JSON="$(salt --output json --key "${PUB_KEY}" --key-id "${KEY_ID}" - <"${SECRET_VALUE_FILE}")"

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
```

### Environment secret

Environment names are URL-encoded in API paths. The packaged helper performs that encoding and applies the same safe request-shaping pattern:

```sh
export GITHUB_TOKEN OWNER REPO ENVIRONMENT_NAME SECRET_NAME
export SECRET_VALUE_FILE=./secret-value.txt
./examples/set-environment-secret.sh
```

### Organization secret

Organization secrets require `VISIBILITY=all`, `private`, or `selected` (default: `private`). When `VISIBILITY=selected`, provide `SELECTED_REPOSITORY_IDS` as a JSON array of unique positive integer repository IDs; the helper validates and sorts the IDs before sending `selected_repository_ids`.

```sh
export GITHUB_TOKEN ORG SECRET_NAME
export SECRET_VALUE_FILE=./secret-value.txt
export VISIBILITY=selected
export SELECTED_REPOSITORY_IDS='[123456789,987654321]'
./examples/set-org-secret.sh
```

### Scripting and security notes

- Use `set -euo pipefail` in automation scripts.
- Always fetch a fresh `key_id` and public key before encrypting.
- Check the HTTP status for public-key fetches before parsing JSON; do not print response bodies on failures because API errors may include sensitive context.
- Validate `SECRET_NAME` against GitHub's `[A-Za-z_][A-Za-z0-9_]*` naming rule and reject the reserved `GITHUB_` prefix before attempting an upload.
- Reject plaintext values larger than 48 KiB (49152 bytes) up front.
- Pipe the `salt --output json` result to `jq` and then to `curl --data-binary @-`; avoid command substitution with `curl -d`, and do not pass `encrypted_value` or `key_id` through `jq` command-line arguments.
- Use environment secrets for production credentials to enforce deployment protection rules such as required reviewers.
- Prefer OIDC-based short-lived credentials for cloud providers instead of storing long-lived API keys.
- Treat only `201` and `204` as success when creating/updating secrets via REST.

See executable scope-specific scripts in `examples/`.

## References

- libsodium sealed boxes: https://doc.libsodium.org/public-key_cryptography/sealed_boxes
- libsodium base64 helpers: https://doc.libsodium.org/helpers
- GitHub Actions Secrets API workflow: https://docs.github.com/en/rest/actions/secrets

## Project Status

This project is under active development with stable CLI behavior and API contracts. The repository has not yet published a tagged release (version 0.1.0 or 1.0.0). Once a semver tag is applied, the release will indicate production-readiness and semantic versioning guarantees will apply to the public API surface defined in `include/salt.h` and the CLI interface documented in the man page and README.

Until then, treat `main` as the development line with evolving documentation, test coverage, and CI infrastructure, while keeping the core encryption and CLI behavior functionally stable.

## Documentation Inventory

- **[README.md](README.md)** (this file) - Project overview, usage guide, and GitHub Actions Secrets API workflow
- **[INSTALL.md](INSTALL.md)** - Build and installation instructions for contributors and package maintainers
- **[TESTING.md](TESTING.md)** - Testing strategy, quality gates, and validation requirements
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Contribution workflow, release process, and documentation maintenance policy
- **[SECURITY.md](SECURITY.md)** - Vulnerability disclosure policy and documented attack surfaces
- **[AGENTS.md](AGENTS.md)** - Repository guidance for coding agents and automated tooling
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and release notes following Keep a Changelog format
- **man/salt.1** - Manual page for the `salt` CLI (validate with `make man`)
- **docs/** - Supporting documentation including lint suppressions, libsodium usage inventory, and Ubuntu package lists
- **examples/** - Executable helper scripts for GitHub Actions secret workflows (repository, environment, and organization scopes)
- **m4/** - Custom GNU Autotools macros for libsodium/cmocka discovery and SBOM tool detection (maintainer reference only)

## Contributing

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for the complete contribution workflow, quality gates, validation requirements, and development setup.

## Security

See **[SECURITY.md](SECURITY.md)** for the disclosure policy and documented attack surfaces.

## Testing

See **[TESTING.md](TESTING.md)** for the testing strategy that covers trust boundaries, test taxonomy, and quality-control gates.

## Hardening

Runtime hardening built into the production binary:

- Sealed-box plaintext, decoded public key, ciphertext, and stdin/argv copies are `sodium_mlock`'d so they cannot be paged to swap, and zeroed via `sodium_munlock` on every exit path. Locking failures (typically a low `RLIMIT_MEMLOCK` inside containers) are non-fatal and fall back to plain `sodium_memzero`-on-release.
- `main()` calls `setrlimit(RLIMIT_CORE, 0)` and `prctl(PR_SET_DUMPABLE, 0)` on Linux so transient secrets cannot be extracted from a core file or by an attaching debugger sharing the same UID. Library callers (`salt_cli_run_with_streams`) opt in to their own hardening.
- Plaintext passed via the positional argument is copied into mlock'd memory and the original `argv` slot is scrubbed in place. Prefer stdin for sensitive plaintext anyway — `--help` documents the trade-off.
- The hardened build is compiled with `-D_FORTIFY_SOURCE=2`, `-fstack-protector-strong`, and linked with `-Wl,-z,relro,-z,now`.

## License

This project is released under The Unlicense. See `LICENSE`.

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this software dedicate any and all copyright interest in the software to the public domain. We make this dedication for the benefit of the public at large and to the detriment of our heirs and successors. We intend this dedication to be an overt act of relinquishment in perpetuity of all present and future rights to this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
