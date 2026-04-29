---
name: update-dependency-pins
description: 'Audit and update pinned dependency versions (GitHub Actions SHAs and Makefile tool versions) to their latest stable releases.'
argument-hint: '[dependency scope or blank for all]'
---

# Update Dependency Pins Skill

## Context

Use this skill to perform a periodic audit and update of all pinned external dependencies in a repository. This covers GitHub Actions (pinned to commit SHAs) and Makefile Go tool installations (pinned to exact versions).

## Workflow

1. **Inventory current pins.**
   - Read all `.github/workflows/*.yml` files and extract `uses:` lines with SHA pins and version comments.
   - Read `Makefile` and extract tool version variables (`*_VERSION ?= X.Y.Z`) and checksum variables (`*_SHA256 ?= ...`).
   - Note any Docker base image references in `docker/ubuntu/*/Dockerfile` files.
   - Note package installation steps in workflow `apt-get install` lines.

2. **Check for updates.**
   - For each GitHub Action, fetch the releases page to find the latest stable release and its commit SHA.
   - For each Makefile tool binary (e.g., actionlint), check the latest release tag and published SHA256 on the tool's GitHub repository.
   - For Docker base images, check the latest digest for the pinned tag on the registry.
   - Compare current pins against latest available versions.

3. **Apply updates.**
   - Update SHA pins in all workflow files. An action may appear in multiple files and multiple jobs — update all occurrences.
   - Update Makefile version and checksum variables.
   - Update Docker base image digests in `docker/ubuntu/*/Dockerfile` files when new digests are available for the pinned tag.
   - Preserve the `# vX.Y.Z` comment format on all SHA pins.

4. **Validate.**
   - Run `make actionlint` or `make validate-copilot` to verify updated Makefile tool versions install and run correctly.
   - Visually inspect workflow YAML for syntax correctness.
   - Run the repository's fast CI gate target (`make ci-fast`) if available.

5. **Report.**
   - Produce a summary table of all changes:

   | Dependency | Type | Old Version | New Version | SHA (if Action) |
   |---|---|---|---|---|
   | actions/checkout | Action | v6.0.1 | v6.0.2 | de0fac2... |
   | actionlint | Makefile | 1.7.7 | 1.7.8 | be92c26... (SHA256) |

## Example Dependency Inventory (Customize)

### GitHub Actions
| Action | Workflow(s) | Occurrences |
|---|---|---|
| `actions/checkout` | ci.yml, release.yml, codeql.yml | varies |
| `actions/cache` | ci.yml | varies |
| `actions/upload-artifact` | ci.yml | varies |
| `softprops/action-gh-release` | release.yml | varies |
| `github/codeql-action/init` | codeql.yml | varies |
| `github/codeql-action/analyze` | codeql.yml | varies |

### Makefile Tools
| Tool | Version Variable | Checksum Variable |
|---|---|---|
| actionlint | `ACTIONLINT_VERSION` | `ACTIONLINT_SHA256` |

### Docker Base Images
| Image | Location |
|---|---|
| `ubuntu:20.04` | `docker/ubuntu/20/Dockerfile` |
| `ubuntu:22.04` | `docker/ubuntu/22/Dockerfile` |
| `ubuntu:24.04` | `docker/ubuntu/24/Dockerfile` |
| `ubuntu:26.04` | `docker/ubuntu/26/Dockerfile` |

## Pin Format Reference

### GitHub Actions
```yaml
- uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd  # v6.0.2
```

### Makefile (binary tool with checksum)
```makefile
ACTIONLINT_VERSION ?= 1.7.8
ACTIONLINT_SHA256  ?= be92c2652ab7b6d08425428797ceabeb16e31a781c07bc388456b4e592f3e36a
```

### Dockerfile base image
```dockerfile
FROM ubuntu:24.04@sha256:<full-sha256-digest>  # ubuntu:24.04 2025-xx-xx
```

## Common Issues

- Updating SHA in one workflow but missing it in another → inconsistent pins across workflows.
- Forgetting to update the version comment → misleading documentation.
- Major version bump without reviewing breaking changes → build failures.
- Updating `ACTIONLINT_VERSION` but not `ACTIONLINT_SHA256` → checksum verification failure at install time.
- Updating a Docker base image tag without updating the digest → mutable reference reintroduced.

## Recommended Cadence

Run this audit monthly or whenever a security advisory affects a pinned dependency. Prioritize security-sensitive tools (actionlint, cosign, codeql-action) for immediate updates.

Update the inventory tables above to match files and tools that exist in your repository.
