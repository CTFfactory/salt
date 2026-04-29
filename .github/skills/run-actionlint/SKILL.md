---
name: run-actionlint
description: 'Run actionlint against all GitHub Actions workflows, interpret every error category, and apply the correct fixes — shellcheck issues, invalid expression contexts, action version pins, and untrusted input injection'
argument-hint: '[workflow file or blank for all]'
---

# Run actionlint Skill

## Purpose

`actionlint` is a static analysis tool for GitHub Actions workflow files. It catches:
- Invalid expression syntax and unavailable context references
- Shellcheck issues in embedded `run:` blocks
- Untrusted user-input injection vectors
- Undefined secrets/inputs in reusable workflows
- Action version pin problems

## Workflow

### Step 1 — Ensure actionlint is installed

```sh
which actionlint || go install github.com/rhysd/actionlint/cmd/actionlint@latest
```

Verify the installed version:
```sh
actionlint --version
```

### Step 2 — Run against all workflows

```sh
cd <repo-root>
actionlint 2>&1
```

To lint a specific file:
```sh
actionlint .github/workflows/ci.yml
```

### Step 3 — Interpret and triage errors

Actionlint error output format:
```
<file>:<line>:<col>: <message> [<rule>]
```

| Rule category | Examples | Common fix |
|---------------|---------|-----------|
| `[expression]` | `context "runner" is not allowed here` | Move expr to a step or set via `$GITHUB_ENV` |
| `[shellcheck]` | `SC2086: double quote`, `SC2129: use {} >> file` | Quote vars; group redirects with block syntax |
| `[action]` | Unknown input, action not found | Check action docs; pin to SHA |
| `[workflow-call]` | Undefined input/output | Declare `inputs:` or `outputs:` in reusable workflow |
| `[credentials]` | Hard-coded token | Move to `${{ secrets.NAME }}` |

### Step 4 — Fix each error category

#### `[expression]` — context unavailability

`runner.*` is NOT available in `jobs.<id>.env`. It IS available in step-level `env:` and `run:` blocks.

```yaml
# ❌ BEFORE (actionlint error)
jobs:
  build:
    env:
      ASSET_DIR: ${{ runner.temp }}/assets

# ✅ AFTER — set via $GITHUB_ENV in a step
jobs:
  build:
    steps:
      - name: Set asset directory
        run: echo "ASSET_DIR=${RUNNER_TEMP}/assets" >> "$GITHUB_ENV"
```

#### `[shellcheck]` SC2086 — unquoted variable

```bash
# ❌ BEFORE
echo "result" >> $GITHUB_STEP_SUMMARY

# ✅ AFTER
echo "result" >> "$GITHUB_STEP_SUMMARY"
```

#### `[shellcheck]` SC2129 — individual redirects

```bash
# ❌ BEFORE
echo "## Title" >> "$GITHUB_STEP_SUMMARY"
echo "" >> "$GITHUB_STEP_SUMMARY"
echo "Body text" >> "$GITHUB_STEP_SUMMARY"

# ✅ AFTER — block redirect
{
  echo "## Title"
  echo ""
  echo "Body text"
} >> "$GITHUB_STEP_SUMMARY"
```

#### `[expression]` — untrusted input injection

Never interpolate `github.event.*` user-controlled fields directly in `run:` scripts.

```yaml
# ❌ UNSAFE — script injection vector
- run: echo "PR: ${{ github.event.pull_request.title }}"

# ✅ SAFE — use env var intermediary
- env:
    PR_TITLE: ${{ github.event.pull_request.title }}
  run: echo "PR: ${PR_TITLE}"
```

#### Action SHA pinning

```yaml
# ❌ Mutable tag
uses: actions/checkout@v4

# ✅ SHA-pinned with version comment
uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd  # v6.0.2
```

### Step 5 — Verify fixes

Re-run actionlint after every round of fixes:
```sh
actionlint 2>&1
```

A clean run exits with code 0 and produces no output.

### Step 6 — Add to CI / Makefile (optional)

To gate merges on actionlint in the Makefile:

```makefile
.PHONY: actionlint
actionlint: ## Lint GitHub Actions workflow files with actionlint
	@command -v actionlint >/dev/null 2>&1 || go install github.com/rhysd/actionlint/cmd/actionlint@latest
	actionlint
```

To add actionlint to a GitHub Actions workflow (self-linting):
```yaml
      - name: Lint workflows with actionlint
        run: |
          go install github.com/rhysd/actionlint/cmd/actionlint@latest
          actionlint
```

## Error Reference

| Error message | Rule | Fix summary |
|---------------|------|-------------|
| `context "runner" is not allowed here` | `[expression]` | Move to step-level env or `$GITHUB_ENV` |
| `context "secrets" is not allowed here` | `[expression]` | Only use `secrets.*` in step-level env or `if:` on steps |
| `SC2086: double quote to prevent globbing` | `[shellcheck]` | Quote `"$VAR"` |
| `SC2129: consider using { cmd1; cmd2; } >> file` | `[shellcheck]` | Group with block redirect |
| `SC2046: quote this to prevent word splitting` | `[shellcheck]` | Quote `"$(cmd)"` command substitutions |
| `undefined input "x"` | `[workflow-call]` | Declare under `inputs:` in the reusable workflow |
| `input "x" is required` | `[workflow-call]` | Provide the input or add `default:` |
| `unknown step output "x"` | `[expression]` | Check step `id:` and `outputs:` map |

## Notes

- actionlint respects `# actionlint-ignore` inline comments to suppress specific rules.
- shellcheck rules can be suppressed with `# shellcheck disable=SC2086` inside the run block.
- Prefer fixing over suppressing unless the rule is a false positive.
- The `.github/instructions/actionlint.instructions.md` file in this repo documents project-specific conventions.
