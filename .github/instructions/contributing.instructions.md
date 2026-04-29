---
description: 'Instructions for maintaining CONTRIBUTING.md files'
applyTo: '**/CONTRIBUTING.md'
---

# Contributing Instructions

Follow these conventions when editing `CONTRIBUTING.md`.

## Required Sections

- Project prerequisites (compiler toolchain, libsodium, cmocka, GNU Make)
- Issue reporting and feature request guidance
- Branching and pull request workflow
- Commit message conventions
- Testing requirements (`make test`, `make test-sanitize`, `make coverage-check`)
- Lint/static analysis requirements (`make lint`)
- Docs/man validation requirements (`make docs`, `make man`)
- Full quality gate (`make ci`)

## Content Rules

- Write for first-time contributors.
- Keep commands copy-pasteable.
- Ensure all documented make targets exist.
- Keep CI and local workflows aligned.
- Link to README, AGENTS.md, and SECURITY.md when present.
