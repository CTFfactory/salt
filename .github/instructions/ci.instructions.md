---
description: 'Conventions for GitHub Actions workflows for the salt C CLI project'
applyTo: '**/.github/workflows/*.yml,**/.github/workflows/*.yaml'
---

# CI Workflow Instructions

Follow these conventions when editing GitHub Actions workflow files.

## Required Gates

- Use GitHub Actions as the CI/CD platform.
- Keep at least two make-driven CI entry points:
  - `make ci-fast` for quick PR feedback
  - `make ci` for full quality gates
- Ensure `make docs` and `make man` are validated in CI.

## Build and Test Expectations

- Use `make` targets instead of raw compiler/test commands.
- Full CI should include lint, test, sanitizer test, coverage check, docs, and man page checks.
- Make coverage threshold enforcement explicit via `make coverage-check`.

## Workflow Structure

- Use least-privilege `permissions` at workflow level.
- Set `timeout-minutes` for each job.
- Pin third-party actions to immutable SHAs with version comments.
- Keep jobs deterministic and avoid hidden side effects.

## Security and Logging

- Never print secrets or key material to logs.
- Keep stdout/stderr behavior predictable and parseable.
- Use explicit shell settings (`set -euo pipefail`) in multi-command steps.

## Storage and Retention

- Keep artifacts minimal; do not upload binaries unless operationally required.
- Set explicit artifact retention and keep it short by default.
- Explain any storage-increasing change in the PR summary.

## Release Workflow

- Trigger release jobs from semver tag pushes (`v*`).
- Release should run only after required quality gates are green.
- Validate docs and man page generation before publishing release artifacts.
