---
description: 'Instructions for maintaining SECURITY.md files'
applyTo: '**/SECURITY.md'
---

# Security Instructions

Follow these conventions when editing SECURITY.md files for any project.

## Required Sections
- **Supported Versions** — table of versions that receive security patches
- **Vulnerability Reporting** — private disclosure instructions (GitHub Security Advisories or email, never public issues)
- **Disclosure Timeline** — expected acknowledgment (48h) and fix timeline
- **Security Contact** — specific contact method
- **Security Scanning** — mention CI-enforced checks (`make test-sanitize`, `make coverage-check`, and CI gates)
- **Attack Surfaces** — public runner, CLI/process, and GitHub Secrets API threat vectors

## Content Rules
- Use clear, actionable language
- Keep supported versions table current with each release
- Link to external security policies if relevant
- Do not include sensitive information (keys, tokens, internal URLs)
- Document runner, log, and API attack surfaces without turning the policy into exploit instructions

## Project Security Posture
- Initialize libsodium with `sodium_init()` before crypto operations.
- Validate base64 decode results and require exact `crypto_box_PUBLICKEYBYTES` key length.
- Check all libsodium return values (`sodium_base642bin`, `crypto_box_seal`, `sodium_bin2base64`) and fail closed.
- Zero sensitive buffers before release using `sodium_memzero` when applicable.
- Keep user-facing errors deterministic and avoid printing secret/key material.
- Keep sanitizer and coverage gates enabled in CI for memory-safety regression detection.
