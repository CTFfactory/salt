# CodeQL Suppressions Inventory

This document tracks all active CodeQL suppressions with their rationale and review status.

## Format

For each suppression:
- **Rule:** CodeQL rule identifier
- **File Pattern:** Which files are affected
- **Severity:** CodeQL reported severity
- **Justification:** Why this is not actionable or is a false positive
- **Category:** True Positive Fix, False Positive, Not Applicable
- **Opened:** Date suppression was added
- **Expires:** Renewal/review date (1 year from opened)
- **Reviewer:** Who approved this suppression

---

## Active Suppressions

## Suppression: c/cpp/uncontrolled-allocation-size (src/cli/*.c)

**Rule:** c/cpp/uncontrolled-allocation-size
**File Pattern:** src/cli/*.c
**Severity:** High
**Justification:** Plain_text argument from stdin is limited by read_plaintext() to PLAINTEXT_MAX_SIZE (16 MB) before allocation. No uncontrolled allocation risk.
**Category:** False Positive
**Opened:** 2026-05-05
**Expires:** 2027-05-05
**Reviewer:** @copilot
**Status:** Active

## Suppression: c/cpp/memory-never-freed (fuzz/*.c)

**Rule:** c/cpp/memory-never-freed
**File Pattern:** fuzz/*.c
**Severity:** Medium
**Justification:** libFuzzer runtime is responsible for cleanup on process exit. Memory not freed in individual iterations is expected and correct behavior.
**Category:** False Positive
**Opened:** 2026-05-05
**Expires:** 2027-05-05
**Reviewer:** @copilot
**Status:** Active

## Suppression: c/cpp/potentially-dangerous-function (src/cli/signal.c)

**Rule:** c/cpp/potentially-dangerous-function
**File Pattern:** src/cli/signal.c
**Severity:** Medium
**Justification:** Functions like write() and signal() are used correctly in async-signal-safe context. No resource allocation or complex logic in signal handlers.
**Category:** False Positive
**Opened:** 2026-05-05
**Expires:** 2027-05-05
**Reviewer:** @copilot
**Status:** Active

### Template (Copy for new suppressions)

```markdown
## Suppression: [RULE_NAME] ([FILE_PATTERN])

**Rule:** c/cpp/[specific-rule]
**File Pattern:** [file pattern or "all"]
**Severity:** [Critical/High/Medium/Low]
**Justification:** [2-3 sentences explaining why this is not actionable]
**Category:** [True Positive Fix / False Positive / Not Applicable]
**Opened:** YYYY-MM-DD
**Expires:** YYYY-MM-DD
**Reviewer:** @[github-user]
**Status:** Active / Expired / Renewed

[Additional context or code examples if helpful]
```

---

## Suppression Hygiene Checklist

When adding a suppression, verify:
- [ ] Finding has been manually reviewed and deemed safe or false positive
- [ ] Justification is clear and specific (not generic)
- [ ] File pattern is as narrow as possible (not `.*`)
- [ ] Expiration date is set (typically 1 year)
- [ ] Entry added to this inventory with full details
- [ ] Corresponding rule added to `.github/codeql-config.yml`

---

## Policy References

- Full policy: `.github/instructions/codeql-findings.instructions.md`
- Configuration: `.github/codeql-config.yml`
- Workflow: `.github/workflows/codeql.yml`

---

## Review Log

| Date | Reviewer | Action | Count |
|---|---|---|---|
| 2026-05-04 | (initial) | Created empty inventory | 0 |
| 2026-05-05 | @copilot | Documented configured suppressions from codeql-config.yml | 3 |

