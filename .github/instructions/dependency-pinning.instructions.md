---
description: 'Rules for pinning GitHub Actions and Makefile tool versions to immutable references'
applyTo: '**/.github/workflows/*.yml,**/.github/workflows/*.yaml,**/Makefile*,**/*.mk'
---

# Dependency Pinning Instructions

Follow these conventions when editing GitHub Actions workflows or Makefile tool installation targets.

## GitHub Actions

- **Always pin to full commit SHAs**, never mutable version tags (`@v6`, `@main`, `@latest`).
- **Include a version comment** after the SHA: `uses: org/action@<SHA>  # vX.Y.Z`.
- Pin to the **latest patch release** of the major version in use.
- When updating, verify the SHA corresponds to a **signed release tag** on the action's repository.

```yaml
# CORRECT — immutable SHA with version comment
- uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd  # v6.0.2

# WRONG — mutable tag can change without notice
- uses: actions/checkout@v6
```

## Makefile Tool Versions

- Define tool versions as **overridable variables** using `?=`.
- Pin to **exact versions**, never ranges or `latest`.
- For tools downloaded as pre-built binaries (e.g., actionlint), also pin the `SHA256` checksum and verify it after download.
- Keep tool versions consistent between Makefile variables and any explicit version references in workflow `apt-get install` steps.

```makefile
# CORRECT — exact version and digest, overridable
ACTIONLINT_VERSION ?= 1.7.8
ACTIONLINT_SHA256  ?= be92c2652ab7b6d08425428797ceabeb16e31a781c07bc388456b4e592f3e36a

# WRONG — floating version
ACTIONLINT_VERSION ?= latest
```

## Docker Image Digest Pinning

- Pin Docker base images to **immutable digest references**, not mutable tags.
- Include a tag comment alongside the digest for human readability.

```dockerfile
# CORRECT — immutable digest with tag comment
FROM ubuntu:24.04@sha256:<full-sha256-digest>  # ubuntu:24.04 2025-xx-xx

# WRONG — mutable tag can silently change
FROM ubuntu:latest
```

## APT Package Pinning

- When installing system packages via `apt-get`, prefer versioned package specs
  (`pkg=X.Y.Z-ubuntu1`) where the distribution's package manager supports it.
- For tools with published pre-built binaries, download the specific release
  archive and verify the SHA256 checksum before installing.
- Record significant APT package version changes in `CHANGELOG.md` when applicable.

## Update Process

### Automated Updates (GitHub Actions)

GitHub Actions are automatically monitored by Dependabot (see `.github/dependabot.yml`):

- **Dependabot preserves SHA pinning** when actions are already pinned to commit SHAs
- **Version comments are maintained** through updates (e.g., `# v6.0.2` → `# v6.0.3`)
- PRs are grouped for minor/patch updates to reduce review volume
- Review and merge Dependabot PRs after validating the changelog and release notes

### Manual Updates (Other Dependencies)

When manually updating dependency versions:

1. Check the upstream repository's releases page for the latest stable version.
2. For Actions: copy the full commit SHA from the release tag, not the tag name.
3. Update all occurrences across all workflow files (an action may appear in multiple workflows).
4. Update the version comment to match the new version.
5. For Makefile tools: update the version variable definition and run `make install-tools` to verify.
6. Ensure actionlint and other Makefile-installed tool versions are consistent across Makefile variables and any explicit version references in workflow `apt-get install` steps.

## Supply Chain Security

- Prefer Actions from official organizations (`actions/*`, `sigstore/*`, `docker/*`).
- Verify release signatures when available (look for "Verified" badge on GitHub release commits).
- Review changelogs before upgrading major versions.
- Do not use Actions from untrusted or low-star repositories without security review.
