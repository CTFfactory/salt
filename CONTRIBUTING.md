# Contributing

Thank you for contributing to salt - Sodium Asymmetric/Anonymous Locking Tool.

## Prerequisites

- C compiler with C11 support
- GNU Make
- Autotools (for building from Git)
- libsodium development package
- cmocka development package (for tests)
- man-db (for man page validation)

For complete build and installation instructions, see **[INSTALL.md](INSTALL.md)**.

## Local Setup

For contributor setup and installation flow, use **[INSTALL.md](INSTALL.md)**.
For test and fuzz validation guidance, use **[TESTING.md](TESTING.md)**.

## Issues and Feature Requests

- Use GitHub issues at <https://github.com/CTFfactory/salt/issues> for bugs,
  regressions, and feature requests.
- Include the platform, compiler, and the exact command you ran when reporting a problem.
- For security reports, do **not** open a public issue; follow `SECURITY.md`.

## Branching and Pull Request Workflow

1. Create a branch from `main`.
2. Implement focused changes with tests.
3. Run local quality gates before opening a PR.
4. Update affected documentation (`README.md`, `man/salt.1`, `CHANGELOG.md`, and supporting docs) in the same branch when behavior changes.
5. Open a pull request with a clear summary, rationale, and any risk notes.

## Commit Messages

Use concise, imperative commit messages that describe user-visible intent.

Examples:

- `Add malformed key test coverage`
- `Harden stderr parsing for CLI errors`
- `Update release workflow action pins`

## Required Validation Before PR

Run local quality gates before opening a PR:

```sh
cd build/autotools
make ci-fast           # Fast smoke gate (recommended default)
make ci                # Full CI gate (before requesting review)
```

**`make ci-fast`** (2-5 min): Runs `validate-copilot`, `actionlint`, `lint-suppressions`, `lint`, `test`, and `build`.
**`make ci`** (10-30 min): Full gate with sanitizers, valgrind, coverage, QA, fuzz regression, docs, and symbol audit.

For explicit local validation requirements, run these targets (from
`build/autotools`) before requesting review:

```sh
make lint
make test
make test-sanitize
make coverage-check
make docs
make man
make ci
```

For complete testing documentation including test taxonomy, fuzzing workflows, coverage policy, and quality-control gates, see **[TESTING.md](TESTING.md)**.

## Lint Suppression Policy

Suppressions are allowed only when a direct fix is not feasible in the same PR.

- Prefer code fixes over suppressions
- Keep scope narrow (line/file over global)
- Provide technical reasoning with risk acceptance
- Define removal trigger and review date

Active suppressions are tracked in `docs/lint-suppressions.md`.

## Release Process

Use this checklist when preparing a release tag:

1. Confirm release scope and SemVer bump type (major, minor, patch).
2. Update `CHANGELOG.md` with a new version section using date format `YYYY-MM-DD`.
3. Run full local functional gate:

```sh
cd build/autotools
make ci
```

4. Run release-specific gates and artifact builds:

```sh
cd build/autotools
make test-docker
make bench-check
make release SALT_VERSION=X.Y.Z
make coverage-html
make sbom SBOM_VERSION=X.Y.Z SBOM_SOURCE_REF=refs/tags/vX.Y.Z
make package-deb-docker SALT_PACKAGE_VERSION=X.Y.Z
```

5. Commit release-prep changes.
6. Create annotated tag:

```sh
git tag -a vX.Y.Z -m "Release vX.Y.Z"
```

7. Push branch and tag:

```sh
git push origin <branch>
git push origin vX.Y.Z
```

8. Verify GitHub release workflow publishes:
	- `salt-linux-amd64.tar.gz`
	- `salt-linux-amd64.spdx.json`
	- `salt-static-linux-amd64.spdx.json`
	- `salt-linux-amd64.cyclonedx.json`
	- `salt-static-linux-amd64.cyclonedx.json`
	- `salt_*_amd64.deb`
	- `SHA256SUMS`
	- `SHA256SUMS.asc`
    (`salt-linux-amd64.tar.gz` is a template-layout archive rooted at `salt/` and includes docs, helper scripts, coverage HTML, man page, static `salt`, and dynamic `salt-dynamic` under `bin/`.)
9. Verify binary version output matches the tag via `salt -V`.

For build artifact generation details, see **[INSTALL.md](INSTALL.md)**.

## Documentation Requirements

If behavior changes, update all applicable docs in the same PR:

- **[README.md](README.md)** - User-facing overview, usage, and quick start
- **[INSTALL.md](INSTALL.md)** - Build and installation procedures
- **[TESTING.md](TESTING.md)** - Test plan, taxonomy, and quality gates
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and release notes
- **man/salt.1** - Man page and CLI option reference (update `.TH` date field to ISO 8601 `YYYY-MM-DD` format when content changes)
- **examples/** - GitHub Actions secret helper scripts and workflow integration patterns
- **[CONTRIBUTING.md](CONTRIBUTING.md) / [SECURITY.md](SECURITY.md) / [AGENTS.md](AGENTS.md)** - When contributor, security, or repository-operating guidance changes

When updating **examples/** helper scripts, verify that they remain aligned with GitHub REST API guidance in README.md and maintain secret-name validation, size-limit enforcement, and safe credential handling patterns.

## Security Reporting

Do not open public issues for vulnerabilities. Follow the private disclosure process in SECURITY.md.

## Related Guides

- **[README.md](README.md)** - Project overview and user guide
- **[INSTALL.md](INSTALL.md)** - Build and installation instructions
- **[TESTING.md](TESTING.md)** - Testing strategy and quality gates
- **[AGENTS.md](AGENTS.md)** - Repository agent and automation guidance
- **[SECURITY.md](SECURITY.md)** - Security policy and vulnerability disclosure
