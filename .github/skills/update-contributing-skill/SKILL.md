---
name: update-contributing-skill
description: 'Update CONTRIBUTING.md to align with current build targets, test gates, and quality thresholds'
argument-hint: '[section or full sync]'
---

# Update CONTRIBUTING.md Skill

Use this skill to sync CONTRIBUTING.md with actual repository state when build targets, quality gates, or contribution workflows change.

## When to Use

- After adding/removing Makefile targets referenced in contributor docs
- After changing coverage thresholds or quality gates
- After updating toolchain requirements (compiler, libsodium, cmocka versions)
- When PR workflow or commit conventions change

## Workflow

1. **Identify drift.**
   Compare documented targets/thresholds in `CONTRIBUTING.md` against:
   - `Makefile.am` target definitions
   - `.github/workflows/ci.yml` quality gates
   - `AGENTS.md` authoritative conventions
   - `configure.ac` dependency checks

2. **Update affected sections.**
   Common drift points:
   - Prerequisites (toolchain versions)
   - Make targets (`make test`, `make ci`, `make lint`)
   - Coverage thresholds (line/function/branch/block %)
   - Test commands (`make test-sanitize`, `make test-valgrind`)
   - Build flow (GNU Autotools: `./bootstrap`, `./configure`, `make`)

3. **Validate commands.**
   Test every documented command:
   ```sh
   cd build/autotools
   make ci-fast
   make test
   make coverage-check
   make lint
   ```

4. **Cross-reference authoritative sources.**
   - Build commands → `AGENTS.md` "Build and Test" section
   - Coverage thresholds → `Makefile.am` COVERAGE variables
   - Quality gates → `.github/workflows/ci.yml`
   - Commit format → existing `CHANGELOG.md` entries

## Sync Checklist

- [ ] Prerequisites match `configure.ac` `PKG_CHECK_MODULES` and CI apt installs
- [ ] Make targets match `Makefile.am` `.PHONY` declarations
- [ ] Coverage threshold matches `COVERAGE ?= 99` from Makefile
- [ ] Test commands include sanitizer and valgrind paths
- [ ] GNU build flow references `./bootstrap` for maintainers
- [ ] Links to AGENTS.md, SECURITY.md, README.md are valid

## Common Drift Patterns

| Documented Value | Actual State | Fix |
|---|---|---|
| Coverage 95% | `COVERAGE ?= 99` | Update to 99% |
| `make build` | Target removed | Reference `make` or `make all` |
| No sanitizer mention | `make test-sanitize` exists | Add to test checklist |
| Handcrafted Makefile flow | GNU Autotools used | Document `./bootstrap` and `./configure` |
