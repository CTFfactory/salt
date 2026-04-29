---
description: 'Generate or review cmocka tests for the salt CLI implementation'
argument-hint: '[source file or behavior]'
agent: Cmocka Testing Expert
model: Claude Sonnet 4.5
---

# cmocka Test Scaffold

Generate a cmocka test scaffold that includes:

- Setup/teardown fixture structure.
- Success case for valid plaintext + public key.
- Failure cases for malformed key input and crypto failure propagation.
- Assertions for exit code and output stream behavior.
- Compatibility with `make test` and `make test-sanitize`.

If tests already exist, produce a prioritized gap list instead of duplicating code.
