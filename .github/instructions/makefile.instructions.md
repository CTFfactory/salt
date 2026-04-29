---
description: 'Best practices for authoring GNU Make Makefiles for this C/libsodium/cmocka project'
applyTo: '**/Makefile,**/makefile,**/*.mk,**/GNUmakefile,**/Makefile.am,**/Makefile.in'
---

# Makefile Development Instructions (C)

Write clear and deterministic GNU Make targets for this C CLI project.

## Required Targets

- `build` — compile the salt binary
- `test` — run cmocka unit tests
- `test-sanitize` — run tests with sanitizer-enabled build flags
- `coverage-check` — enforce minimum coverage threshold
- `lint` — static checks and style checks for C code
- `docs` — validate documentation artifacts
- `man` — validate man page source and rendering
- `ci-fast` — quick local/CI gate
- `ci` — full local/CI gate
- `clean` — remove build artifacts
- `help` — show documented targets

## General Rules

- Use `.PHONY` for non-file targets.
- Use tabs for recipe indentation.
- Keep recipes deterministic and idempotent.
- Prefer variables for compiler, flags, and output paths.
- Keep dependency footprint minimal and explicit.

## Recommended Variables

```makefile
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?= -lsodium

TEST_LDLIBS ?= -lcmocka -lsodium
COVERAGE ?= 80
BIN_DIR ?= bin
BUILD_DIR ?= build
```

## Docs and Man Policy

- Keep a checked-in roff source at `man/salt.1`.
- `make man` should validate with `man -l man/salt.1 >/dev/null`.
- Avoid introducing doc-generation dependencies by default.
- If optional generators are introduced later, keep roff validation as the default path.

## Coverage Gate Pattern

- Keep coverage enforcement centralized in `make coverage-check`.
- CI must fail when coverage is below threshold.
- Prefer branch + line coverage when tooling supports it.

## CI Aggregator Targets

- `ci-fast`: lint + unit tests + basic build checks
- `ci`: ci-fast + sanitizer tests + coverage-check + docs + man

Keep these targets stable because workflows and contributor docs depend on them.
