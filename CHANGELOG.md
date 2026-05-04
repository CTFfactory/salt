# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## Release Preparation Workflow

When preparing a release:

1. Rename `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD` using the target version number and release date in ISO 8601 format.
2. Add a new empty `## [Unreleased]` section immediately below the project header for future changes.
3. Update comparison links at the bottom of this file to reference the new version tag.
4. Verify all user-facing changes since the last release are documented as a flat feature list (one bullet per capability, behavior, or guarantee) without Added/Changed-style subsections.

---

## [Unreleased]

- Release automation publishes Ubuntu 22/24/26 `.deb` artifacts built with native `dpkg-deb` packaging and native embedded Debian signatures.
- Release assets now include `SHA256SUMS.asc`, and checksum verification remains enforced in workflow before publication.
- Release tarball layout now ships static `salt` and dynamic `salt-dynamic` under `salt/bin/`.

## [0.0.0] - 2026-04-29

- `salt` encrypts plaintext as a libsodium sealed box (`crypto_box_seal`) and emits base64 ciphertext by default.
- Plaintext input is accepted from a positional argument or stdin, with strict size limits for GitHub Actions Secret compatibility.
- Recipient public keys are accepted as base64, strict JSON (`key`, optional `key_id`), or stdin input.
- `--key-format auto|base64|json` selects parser mode, with `auto` as the default.
- `--output text|json` supports plain ciphertext output and a REST-ready JSON envelope with `encrypted_value` and `key_id`.
- `--key-id` injects or overrides `key_id` in JSON output when key input does not provide one.
- CLI flags include short and long forms for key input, key ID, output format, key format, help, and version.
- Exit codes are stable and deterministic: `0` success, `1` input/usage, `2` crypto failure, `3` internal failure, `4` runtime I/O failure, `5` signal interruption.
- Stderr diagnostics are deterministic and stdout is reserved for ciphertext or JSON result data.
- The public C API in `include/salt.h` exposes `salt_seal_base64` and `SALT_MAX_*` limits.
- Sensitive buffers are scrubbed and use libsodium secure-memory handling (`sodium_mlock` / `sodium_memzero`) where practical.
- The production entrypoint hardens runtime behavior by suppressing core dumps and limiting debugger attachment.
- Positional plaintext arguments are copied into secured memory and scrubbed from `argv` to reduce process-list exposure.
- The JSON key parser rejects unescaped control bytes to maintain RFC 8259-compliant and safe JSON handling.
- A manual page is provided at `man/salt.1`.
- Example scripts support repository, environment, and organization GitHub Actions secret uploads.
- The project includes cmocka unit tests, libFuzzer/AFL++ fuzzing harnesses, benchmark gates, and CI/release workflows.
