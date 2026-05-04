---
description: 'Build-process conventions for installation docs, Ubuntu Docker build images, and build orchestration scripts'
applyTo: '**/INSTALL.md,**/docker/ubuntu/**/Dockerfile,**/scripts/qa.sh,**/scripts/fuzz.sh,**/scripts/bench-compare.sh'
---

# Build Process Instructions

Use these rules when changing the documented or scripted build flows for this repository.

## Canonical Build Flow

- Keep the canonical GNU flow explicit: maintainers run `./bootstrap`, then contributors build out-of-tree with `../../configure --enable-tests`, `make`, and `make check` from `build/autotools`.
- Keep all scripted quality gates Make-driven (`make ci-fast`, `make ci`, `make test-sanitize`, `make test-valgrind`, `make fuzz-regression`, `make bench-check`), not ad-hoc compiler/test command blocks.
- Keep local and CI/release build flows aligned so the same Make targets enforce the same expectations.

## INSTALL.md Rules

- Treat `INSTALL.md` as the build/install source of truth; README should link to it instead of duplicating command sequences.
- Keep commands copy-pasteable and explicitly scoped to `build/autotools` when using advanced targets.
- When adding or changing build steps, update nearby sections that depend on the same flow (canonical build, release prep, and advanced targets) in one change.

## Docker Build Matrix Rules

- Keep Ubuntu image variants (`docker/ubuntu/22`, `24`, `26`) behaviorally aligned unless a version-specific exception is documented.
- Pin package installation behavior to deterministic `apt-get` flows (`apt-get update` + non-interactive install in one layer) and clean package lists to limit image drift.
- Ensure Dockerfiles compile and run the same GNU build/test sequence described in project docs.

## Build Script Rules (`scripts/qa.sh`, `scripts/fuzz.sh`, `scripts/bench-compare.sh`)

- Preserve strict shell mode for orchestrators (`set -euo pipefail` or stricter) and keep error exits explicit.
- Keep script output deterministic and automation-friendly: stable prefixes, machine-parsable summaries, and stderr for errors.
- Keep default paths and targets repository-relative and reproducible (`build/autotools`, `.fuzz/`, `bench/baseline.json`), avoiding host-specific assumptions.
- When introducing a new environment variable or CLI flag, document default behavior in the script header comments and maintain backwards-compatible defaults.

## DO / DON'T

```text
DO:  cd build/autotools && make ci
DO:  Keep INSTALL.md, Dockerfiles, and build scripts in sync when build gates change.
DON'T: Replace Make targets in CI/docs with raw compiler invocations.
DON'T: Add version-specific Docker behavior without documenting why only one Ubuntu version differs.
```
