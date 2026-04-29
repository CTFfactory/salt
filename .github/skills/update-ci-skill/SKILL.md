---
name: update-ci-skill
description: 'Update or generate GitHub Actions workflows and Makefile CI targets for the salt C CLI project'
argument-hint: '[workflow name or CI objective]'
---

# Update CI Skill

Use this skill to create, update, or troubleshoot GitHub Actions workflows and Makefile CI targets.

## Workflow

1. Read all files in `.github/workflows/`.
2. Read `Makefile` and identify `ci-fast`, `ci`, `test`, `test-sanitize`, `coverage-check`, `docs`, and `man`.
3. Ensure CI jobs call `make` targets rather than raw compiler/test commands.
4. Ensure docs and man-page validation is present in CI.
5. Ensure pinned action SHAs, least-privilege permissions, and job timeouts.
6. Verify local parity by running `make ci-fast` then `make ci`.

## Common Issues

- Workflow uses raw commands and drifts from Makefile behavior.
- Sanitizer test path is missing in CI.
- Coverage threshold is not enforced.
- Docs/man checks are absent, causing release regressions.
