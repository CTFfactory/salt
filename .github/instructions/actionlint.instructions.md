---
description: 'Rules for keeping GitHub Actions workflow files clean and valid using actionlint'
applyTo: '**/.github/workflows/*.yml,**/.github/workflows/*.yaml'
---

# actionlint Instructions

Follow these rules when editing GitHub Actions workflow files to keep them `actionlint`-clean.

## Running actionlint

```sh
# Install (via Makefile — preferred)
make install-actionlint

# Run via Makefile (auto-installs, includes embedded shellcheck)
make actionlint

# Lint all workflows (standalone)
actionlint

# Lint a single file
actionlint .github/workflows/ci.yml

# Output in JSON for programmatic use
actionlint -format '{{json .}}'
```

actionlint combines GitHub Actions schema validation, expression type checking, and embedded shellcheck/pyflakes passes in a single command. **All workflows must pass `actionlint` before merging.**

## Expression Rules `[expression]`

- **Context availability** — not every context is valid everywhere. Reference the [context availability table](https://docs.github.com/en/actions/learn-github-actions/contexts#context-availability).
  - `runner.*` is **not available** in `jobs.<id>.env` — only in step-level `env:` and `run:` scripts.
  - `secrets.*` is not available in `if:` on `jobs.<id>` — only on steps.
  - `needs.*` is not available in steps — only in job-level fields.
- Set multi-step environment values via `$GITHUB_ENV` when the expression requires a context unavailable at job level:
  ```yaml
  - name: Set derived env
    run: echo "ASSET_DIR=${RUNNER_TEMP}/assets/${{ github.run_id }}" >> "$GITHUB_ENV"
  ```
- Use `${{ github.event_name == 'workflow_dispatch' && inputs.tag || github.event.release.tag_name }}` pattern for ternary — valid actionlint-safe expression pattern.

## Shell Script Rules `[shellcheck]`

Actionlint passes embedded `run:` blocks through shellcheck. Fix all reported issues:

| Code | Severity | Fix |
|------|----------|-----|
| SC2086 | info | Double-quote all variable expansions: `"$VAR"` not `$VAR` |
| SC2129 | style | Combine consecutive redirects to the same file with `{ cmd1; cmd2; } >> file` |
| SC2046 | warning | Quote command substitution: `"$(cmd)"` not `$(cmd)` |
| SC2006 | warning | Replace backticks with `$()` |
| SC2164 | warning | `cd` failures: use `cd dir || exit 1` |
| SC2155 | warning | Declare and assign separately to avoid masking return values |

**SC2129 block-redirect pattern** (preferred for `$GITHUB_STEP_SUMMARY`):
```yaml
        run: |
          {
            echo "## Summary"
            echo ""
            echo "Result: ${RESULT}"
          } >> "$GITHUB_STEP_SUMMARY"
```
Always quote `"$GITHUB_STEP_SUMMARY"` and `"$GITHUB_ENV"` — they are shell variables subject to SC2086.

## Action Version Rules `[action]`

- Pin third-party actions to full commit SHAs with a `# vX.Y.Z` comment:
  ```yaml
  uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd  # v6.0.2
  ```
- Never pin by mutable tag alone (`@main`, `@v4`) — actionlint warns and it is a supply-chain risk.
- First-party actions (`actions/*`, `github/*`) may use `@v4`-style tags when SHA pinning is impractical.

## Input/Output Type Rules `[workflow-call]`

- Declare `inputs:` types explicitly in reusable workflows: `type: string | boolean | number`.
- Use `required: true` for mandatory inputs and provide `default:` for optional ones.
- Never access `${{ inputs.name }}` in a reusable workflow where the input is undeclared.

## Permission Rules

- Set `permissions:` at the workflow level with least privilege.
- Override per job only when a job needs elevated permissions.
- Required minimum for `GITHUB_TOKEN` write usage: declare write scope explicitly.

## Common Mistakes

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| `context "runner" is not allowed here` | `runner.*` in job-level `env:` | Move to step-level env or set via `$GITHUB_ENV` |
| `SC2086: double quote to prevent globbing` | Unquoted `$VAR` in run: block | Add `"$VAR"` quotes |
| `SC2129: consider using { } >> file` | Many individual `>> file` redirects | Group with `{ ...; } >> "$FILE"` |
| `undefined input` | `${{ inputs.x }}` without declaration | Add `inputs:` block or use env var |
| Untrusted input injection | `${{ github.event.*.body }}` in `run:` | Assign to env var first, then use shell var |

## Untrusted Input Handling

Never interpolate untrusted GitHub event data directly in `run:` scripts — this enables script injection:

```yaml
# ❌ UNSAFE
- run: echo "${{ github.event.pull_request.title }}"

# ✅ SAFE — inject into env var, access as shell variable
- env:
    PR_TITLE: ${{ github.event.pull_request.title }}
  run: echo "$PR_TITLE"
```

Untrusted sources: `github.event.*.body`, `github.event.*.title`, `github.event.*.name`, `github.head_ref`, `github.event.inputs.*`.
