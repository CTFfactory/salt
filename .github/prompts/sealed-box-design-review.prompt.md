---
description: 'Review prompt for libsodium sealed-box implementation quality and safety'
argument-hint: '[optional focus area]'
agent: Libsodium Expert
model: Claude Sonnet 4.5
---

# Sealed Box Design Review

Review the implementation for:

- Input sanitization and argument validation.
- Correct libsodium initialization and API usage.
- Public key decoding and length checks.
- `crypto_box_seal` usage correctness.
- Base64 encoding output correctness.
- Error handling quality and deterministic output contract.
- Sensitive data handling and memory-safety practices.

Return findings ordered by severity with specific file references.
