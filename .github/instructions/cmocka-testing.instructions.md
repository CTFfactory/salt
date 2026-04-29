---
name: cmocka-testing
description: 'Conventions for cmocka tests and the repository testing guide'
applyTo: '**/tests/**/*.c,**/test_*.c,**/TESTING.md'
---

# Testing Conventions

## Source of Truth

- `TESTING.md` is the repository test plan and source of truth for testing
  taxonomy, smoke gates, coverage policy, trust boundaries, and release-quality
  criteria.
- Update `TESTING.md` when adding, removing, or materially changing test types,
  fuzz harnesses, coverage thresholds, or CI quality gates.

## Testing Taxonomy

- **Smoke testing**: `make ci-fast`, `make fuzz-smoke`
- **Unit testing**: `make test`, `make test-clang`
- **Memory-safety testing**: `make test-sanitize`, `make test-valgrind`
- **System testing**: `make test-docker`
- **Coverage gate**: `make coverage-check` (current policy: `>99%` line,
  `>99%` function, `>99%` block, and `>99%` branch coverage)
- **Fuzz testing**: `make fuzz-regression`, `make fuzz`, `make fuzz-run`
- **Monkey testing**: long-budget `scripts/fuzz.sh` campaigns and AFL++ runs
  where more randomised exploration is useful
- **Quality control**: `make bench-check`, `make docs && make man`,
  `make validate-copilot`, `make ci`

## cmocka Rules

- Organize tests by behavior (input validation, encryption success, error paths).
- Use setup/teardown fixtures for reusable test state.
- Keep assertions explicit for return codes, output data, and error messages.
- Add negative tests for malformed key input and crypto failure handling.
- Ensure tests are deterministic and runnable in CI without external services.
- Keep sanitizer-compatible test code and avoid undefined behavior in test helpers.
- Keep one behavior focus per test function.
- Verify stdout/stderr separation and exit-code stability for CLI-facing changes.

## Quality Control

- Treat fuzz-discovered or sanitizer-discovered bugs as open until they are
  diagnosed and, when practical, represented by deterministic regressions.
- Keep the CI gates aligned with `TESTING.md`; CI jobs enforce the plan, but the
  plan itself lives in `TESTING.md`.
