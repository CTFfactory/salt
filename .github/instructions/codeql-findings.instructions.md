---
description: 'CodeQL findings triage and suppression policy for the salt C CLI'
applyTo: '.github/workflows/codeql.yml,.github/codeql-config.yml'
---

# CodeQL Findings Policy

This document defines how CodeQL findings are triaged, managed, and suppressed in the salt repository.

## Principles

1. **Zero-tolerance for true positives** — All genuine security/quality issues must be remediated, not suppressed
2. **Documented suppressions** — Every suppression must have clear justification; undocumented suppressions are unacceptable
3. **Automation over manual review** — Suppress at the tool level (CodeQL config) rather than closing alerts manually
4. **Keep signals high** — False positive suppression must be narrow and specific, never broad disabling of query classes

## Severity and Action Mapping

| CodeQL Severity | Recommended Action | Timeline |
|---|---|---|
| **Critical** | Block PR merge until remediated | Before merge |
| **High** | Require code review and remediation | Before next release |
| **Medium** | Track and schedule remediation | Next sprint |
| **Low/Note** | Triage; suppress if false positive or not applicable | Per policy |

## Alert Categories

### Category A: True Positives (Remediate)
These findings represent genuine security or code-quality issues and must be fixed.

**Examples:**
- Null pointer dereferences without validation
- Buffer overflows or out-of-bounds access
- Uncontrolled allocations
- Integer overflows leading to underflow
- Weak cryptographic choices (e.g., using deprecated algorithms)
- Incomplete input validation in CLI parsing

**Process:**
1. Create a GitHub issue with `type:security` label
2. Link to the CodeQL alert URL and severity
3. Implement a fix in a feature branch
4. Add regression test (cmocka or fuzz corpus update)
5. Close the alert upon PR merge

### Category B: False Positives (Suppress with Justification)
These findings are triggered by legitimate code patterns that CodeQL cannot fully understand.

**Examples:**
- Controlled allocations where size is bounded by prior validation
- Intentional null checks that CodeQL's taint flow analysis misses
- Architecture-specific code patterns unavoidable in C

**Process:**
1. Verify the finding is not a true positive by manual code review
2. Dismiss the alert via the GitHub Security tab UI.
3. Include clear justification in the dismissal comment (why CodeQL is wrong, how the code is safe)
4. Document the suppression in `.github/codeql-suppressions.md`
5. Do NOT use `// codeql[...]` inline comments; use config-level suppressions instead

### Category C: Not Applicable (Suppress with Rationale)
These findings may be technically correct but not applicable to salt's threat model or operational constraints.

**Examples:**
- Performance warnings in non-critical code paths
- Warnings about functions used in testing only
- Library features disabled in production build

**Process:**
1. Document in `.github/codeql-suppressions.md` with clear rationale
2. Link to threat model or architecture decision
3. Ensure the finding is recorded appropriately if tracking manually.
4. Review annually; upgrade suppression if context changes

## Query Suite Selection

### security-and-quality (Default, Enabled)
- Fast feedback loop suitable for PR validation
- Covers high-impact security flaws and obvious code smells
- Recommended for all code paths

### security-extended (Enabled)
- Deeper analysis, longer runtime
- Includes subtle security patterns and crypto-specific checks
- Run on main branch weekly; included in full gate

### Custom Query Packs (Future)
- Libsodium API misuse patterns (e.g., missing sodium_mlock for sensitive buffers)
- CLI argument validation completeness
- Cryptographic boundary violations

## Suppression Workflow

### Inline Suppressions (NOT Preferred)
Do not use inline `// codeql[...]` or `codeql-py: ignore=...` comments. These scatter suppression logic across the codebase and are harder to audit.

### CodeQL Alert Dismissal (Preferred)
Do not try to add suppression logic to `.github/codeql-config.yml` as it is not supported natively. Instead, dismiss alerts directly in the GitHub Security tab UI. 
When dismissing an alert, provide a clear, documented justification in the text box for audit purposes.
For programmatic or bulk suppressions, consider using advanced query filtering or SARIF post-processing.

### Documentation
Document every suppression in `.github/codeql-suppressions.md`:
```markdown
## Suppression: c/cpp/memory-never-freed (src/cli/parse.c)

**Rule:** c/cpp/memory-never-freed
**Severity:** Medium
**Justification:** This report is valid in production, but parse.c is also used in fuzzing harnesses.
Fuzz harnesses intentionally do not free memory; leaks are expected (libFuzzer provides cleanup).
The parser itself does free all allocations on error paths; the function-level leak is test-only.
**Scope:** src/cli/parse.c only
**Review Frequency:** Annual
**Opened:** [date]
**Expires:** [date + 1 year]
```

## Review and Escalation

- **Suppressions reviewed:** Quarterly (every 3 months)
- **Policy reviewed:** Annually
- **Escalation:** If >5 unfixed HIGH findings, escalate to `Principal Software Engineer`
- **Audit trigger:** Any suppression >1 year old triggers review for removal or renewal

## Integration with CI/CD

- **PR validation:** `security-and-quality` suite (fast, blocks on critical/high)
- **Main branch:** `security-extended` suite runs on push (informational, does not block)
- **Scheduled:** Full `security-extended` + custom queries weekly (Sunday 06:00 UTC)
- **Release:** Run full suite before publishing release artifacts; block release on critical/high findings

## Initial Baseline Run

When CodeQL custom queries are first deployed to production, a baseline triage is required to establish the initial finding inventory and establish what is "normal" signal.

### Baseline Triage Process

**Phase 1: Initial Scan (Day 1)**
1. Trigger CodeQL workflow on main branch: `gh workflow run codeql.yml --ref main`
2. Wait for completion (~15-20 minutes)
3. Export findings to CSV from GitHub Security tab
4. Note total count and severity breakdown

**Phase 2: Classification (Days 2-3)**
For each finding, classify as:
- **Category A (Fix):** Genuine security/quality bug; must be remediated
  - Action: Create GitHub issue with findings linked
  - Effort estimate and assign PR
- **Category B (Suppress):** False positive with known reason; suppress in config
  - Action: Dismiss alert in GitHub Security tab with justification.
  - Rationale: "Controlled by X" or "Expected in Y"
- **Category C (Defer):** True positive but low priority; track for future sprint
  - Action: Create GitHub issue with `priority:low` label
  - Timeline: Next sprint

**Phase 3: Documentation (Days 3-4)**
1. For all Category B suppressions: document in `.github/codeql-suppressions.md`
2. For all Category A findings: create linked GitHub issues
3. Run CodeQL again to verify suppressions applied
4. Document final baseline metrics (0 unknown/unclassified findings)

### Exit Criteria for Baseline Complete

- [ ] All findings (A/B/C) classified
- [ ] Category B suppressions documented with rationale
- [ ] Category A issues created and assigned
- [ ] No "unknown" or "unreviewed" findings remain
- [ ] GitHub Security tab shows zero "unknown" alerts
- [ ] Metrics recorded in repo wiki or README

**Typical Timeline:** 3-4 days (depends on finding volume; expect 20-50 findings on first run)

### Baseline Metrics Template

Add to `.github/codeql-suppressions.md` after first run:

```markdown
## First Baseline Run (Date: YYYY-MM-DD)

- **Total Findings:** XX
- **Critical:** X (all remediated)
- **High:** X (Y suppressed, Z fixed)
- **Medium:** X (Y suppressed, Z deferred)
- **Low/Note:** X (Y suppressed)
- **Suppressions Added:** X with documented rationale
- **Issues Created:** X Category A + Z Category C
- **Time to Triage:** X days
```

## Related Documentation

- `.github/codeql-config.yml` — Automated query and filter definitions
- `.github/codeql-suppressions.md` — Suppression inventory with rationales
- `.github/workflows/codeql.yml` — CodeQL workflow automation
- AGENTS.md / `.github/agents/codeql-c-expert.agent.md` — CodeQL specialist agent

## Questions or Changes

Open an issue with `label:codeql` for policy discussions or violations. All policy changes require PR review by `@Principal Software Engineer`.
