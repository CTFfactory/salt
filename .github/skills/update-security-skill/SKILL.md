---
name: update-security-skill
description: 'Update SECURITY.md to reflect current vulnerability reporting process, supported versions, and attack-surface coverage'
argument-hint: '[section or full sync]'
---

# Update SECURITY.md Skill

Use this skill to sync SECURITY.md with actual security posture when disclosure process, supported versions, or testing gates change.

## When to Use

- After a new release (update supported versions table)
- After changing security testing gates (`make test-sanitize`, coverage thresholds)
- After adding/removing attack surfaces (new Secrets API workflows, CI changes)
- When vulnerability disclosure process changes

## Workflow

1. **Identify drift.**
   Compare `SECURITY.md` against:
   - Latest release tags (supported versions)
   - `AGENTS.md` security rules section
   - `Makefile.am` sanitizer and coverage targets
   - `TESTING.md` trust boundary and security testing sections
   - `.github/workflows/ci.yml` security gates
   - `.github/instructions/security.instructions.md` required sections

2. **Update affected sections.**
   Required sections per `.github/instructions/security.instructions.md`:
   - Supported versions (which releases receive patches)
   - Reporting a vulnerability (private disclosure, not public issues)
   - Disclosure timeline (expected response/fix time)
   - Security contact (email or GitHub Security Advisories)
   - Security scanning (sanitizer/test/coverage gates)
   - Sensitive data handling (no-secrets-in-logs, deterministic errors)

3. **Verify attack-surface coverage.**
   Confirm `SECURITY.md` documents:
   - Public-runner trust model (GITHUB_TOKEN permissions, fork-PR boundaries, action pins)
   - GitHub Secrets API workflow risks (key_id freshness, token handling, 48 KB validation)
   - Trust boundaries from `TESTING.md` (CLI args, stdin, base64/JSON parsing)

4. **Cross-reference actual gates.**
   - Security checks → `make test-sanitize`, `make test-valgrind`, `make coverage-check`, `make ci`
   - Supported versions → `git tag -l 'v*'`
   - Libsodium usage → `.github/instructions/libsodium-patterns.instructions.md`
   - Memory safety → `.github/instructions/c-memory-safety.instructions.md`

## Sync Checklist

- [ ] Supported versions table includes recent releases
- [ ] Private disclosure path is explicit (not "file an issue")
- [ ] Security checks match actual Makefile targets
- [ ] Attack surfaces match repository workflows (runners, Secrets API)
- [ ] libsodium and memory-safety guidance aligns with instructions
- [ ] `TESTING.md` cross-reference for trust-boundary coverage

## Common Drift Patterns

| Documented State | Actual State | Fix |
|---|---|---|
| Supported: last 2 releases | 3 releases exist | Update table |
| No sanitizer mention | `make test-sanitize` exists | Add to security scanning section |
| Public issue reporting | Private disclosure required | Update reporting section |
| No Secrets API coverage | GitHub Secrets workflow present | Add attack-surface section |
