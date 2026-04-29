---
name: create-makefile
description: 'Create a Makefile for the salt C CLI project with build, test, sanitizer, coverage, docs, and man targets'
argument-hint: '[project path]'
---

# Create Makefile (C/libsodium/cmocka)

Create a deterministic GNU Makefile for this repository.

## Workflow

1. Detect source, include, test, and man-page paths.
2. Define compiler and linker variables.
3. Add required targets:
   - `build`
   - `test`
   - `test-sanitize`
   - `coverage-check`
   - `lint`
   - `docs`
   - `man`
   - `ci-fast`
   - `ci`
   - `clean`
   - `help`
4. Ensure tests link cmocka and libsodium.
5. Ensure man-page targets validate `man/salt.1` without adding default generator dependencies.
6. Keep `ci-fast` and `ci` as stable public interfaces for workflows.

## Rules

- Use `.PHONY` for non-file targets.
- Keep recipes tab-indented.
- Use variable-driven flags and paths.
- Enforce coverage via `coverage-check`.
- Keep dependencies minimal and explicit.
