---
name: update-gnu-build-system
description: 'Update and validate GNU Autotools inputs and GNU Build System flows (bootstrap, configure, build, test, and distcheck)'
argument-hint: '[objective or file scope]'
---

# Update GNU Build System

Use this skill to create, modify, or troubleshoot GNU Autotools and GNU Build System behavior in this repository.

## Scope

- Autoconf inputs: `configure.ac`
- Automake inputs: `Makefile.am`
- Bootstrap entrypoint: `bootstrap`
- Build flow: `autoreconf -fi`, `./configure`, `make`, `make check`, `make distcheck`

## Workflow

1. Read `configure.ac`, `Makefile.am`, and `bootstrap` together before editing.
2. Preserve deterministic dependency checks for libsodium and cmocka (when tests are enabled).
3. Apply minimal edits to Autoconf macros, substitutions, and Automake target wiring.
4. Regenerate build scripts from source inputs:
   ```bash
   ./bootstrap
   ```
5. Validate out-of-tree build behavior:
   ```bash
   mkdir -p build/autotools-skill && cd build/autotools-skill
   ../../configure
   make -j"$(nproc)"
   make check
   ```
6. Run distribution verification when build-system behavior changed:
   ```bash
   make distcheck
   ```
7. Re-run repository quality entry points that must remain stable:
   ```bash
   make build && make test
   ```

## Rules

- Keep configure failures explicit for required dependencies; do not introduce silent fallbacks.
- Keep variables and conditionals synchronized between `configure.ac` and `Makefile.am`.
- Keep bootstrap deterministic and avoid environment-specific hacks.
- Prefer out-of-tree builds when validating structural changes to build-system inputs.
