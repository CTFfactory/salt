---
name: audit-gnu-autotools
description: 'Audit GNU Autotools and GNU Build System files for portability, dependency checks, and CI parity'
argument-hint: '[path scope or blank for full audit]'
---

# Audit GNU Autotools

Use this skill to review Autotools build definitions and report high-signal issues with concrete fixes.

## Audit Targets

- `configure.ac`
- `Makefile.am`
- `bootstrap`

## Checklist

1. Validate required dependencies fail during configure with actionable diagnostics.
2. Validate `AC_SUBST` and `AM_CONDITIONAL` values are consumed consistently in `Makefile.am`.
3. Validate test gating behavior (`ENABLE_TESTS`) matches repository expectations.
4. Validate bootstrap remains deterministic (`autoreconf -fi`) and does not hide failures.
5. Validate custom test targets keep explicit exit behavior and deterministic stderr.
6. Validate CI-relevant interfaces (`make build`, `make test`, sanitizer/valgrind targets) remain reachable.

## Validation Commands

```bash
./bootstrap
mkdir -p build/autotools-audit && cd build/autotools-audit
../../configure
make -j"$(nproc)"
make check
```

## Output Format

Report findings in a severity table:

| Severity | File | Finding | Recommended Fix |
|---|---|---|---|
| ERROR | configure.ac | Missing required dependency failure path | Add `AC_MSG_ERROR` guarded check |
| WARNING | Makefile.am | Conditional variable defined but unused | Remove dead conditional or wire into target |
| OK | bootstrap | Deterministic autoreconf entrypoint | No change |
