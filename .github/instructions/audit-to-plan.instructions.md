---
description: 'Audit-to-Plan workflow for custom agents with domain audit skills'
applyTo: '.github/agents/*.agent.md'
---

# Audit-to-Plan Workflow

This guide documents the unified audit-to-plan workflow that custom agents follow when they have domain-specific audit capabilities.

## Overview

When a custom agent is invoked for a task in its domain, the agent should:

1. **Audit Discovery** — Run the domain audit to identify current state, weaknesses, or opportunities
2. **Finding Prioritization** — Group and prioritize findings by severity and impact
3. **Plan Generation** — Create a structured implementation plan from findings
4. **User Review** — Exit plan mode to request user approval before execution
5. **Execution Tracking** — Maintain SQL-backed todo list with validation gates

## Agent Frontmatter

Agents with audit capabilities must declare them in their `.agent.md` frontmatter:

```yaml
---
name: 'Implementation Secure C Expert'
description: 'Specialist in CWE-702/CWE-658 implementation weaknesses, CERT C guidance, and systematic C security hardening'
model: GPT-5.4
tools: ['codebase', 'search', 'runCommands', 'edit/editFiles', 'problems', 'usages']
user-invocable: true

# Audit capability metadata (if agent has an audit skill)
audit-skill: 'audit-implementation-weaknesses'
audit-scope: src/
audit-args: '[optional: cwe_focus, severity_floor, or other args]'
---
```

### Frontmatter Fields

- `audit-skill` (string) — Name of the audit skill to invoke (e.g., `audit-implementation-weaknesses`)
- `audit-scope` (string) — Default scope for audit (e.g., `src/`, `include/`, or entire repository)
- `audit-args` (string, optional) — Default or template for audit arguments (e.g., `cwe_focus=all`, `severity_floor=MEDIUM`)

## Audit Result Format

All audits must conform to the schema defined in `.github/schemas/audit-report.schema.json`. The audit result is a JSON object with:

```json
{
  "agent": "Implementation Secure C Expert",
  "audit_skill": "audit-implementation-weaknesses",
  "timestamp": "2026-05-03T10:08:15Z",
  "scope": ".",
  "findings": [
    {
      "severity": "HIGH",
      "cwe": "CWE-476",
      "cert_rule": "MEM32-C",
      "category": "Null Check Missing",
      "file": "cli/parse.c",
      "line": 42,
      "description": "Return value of malloc not checked before dereference",
      "evidence": "ptr = malloc(len); ptr[0] = 0;",
      "fix": "Add if (ptr == NULL) { return SALT_ERR_INTERNAL; } after allocation",
      "validation_gates": ["make test-sanitize", "make test"],
      "references": ["CWE-476", "MEM32-C"]
    }
  ],
  "summary": {
    "critical": 0,
    "high": 1,
    "medium": 0,
    "low": 0,
    "total": 1
  }
}
```

### Required Fields

- `agent` — agent name (from frontmatter)
- `audit_skill` — skill name (from frontmatter)
- `timestamp` — ISO 8601 timestamp
- `scope` — audit scope (user-specified or default)
- `findings` — array of finding objects (may be empty if no issues found)
- `summary` — count summary by severity

### Finding Object Fields

- `severity` — "CRITICAL", "HIGH", "MEDIUM", or "LOW"
- `file` — repository-relative path
- `description` — one-sentence human-readable summary
- `cwe` (optional) — CWE identifier if applicable
- `cert_rule` (optional) — CERT-C rule identifier if applicable
- `category` (optional) — high-level category (e.g., "Bounds Check", "Design Flaw")
- `line` (optional) — line number
- `evidence` (optional) — code snippet
- `fix` — proposed remediation or fix strategy
- `validation_gates` (optional) — list of validation commands to verify fix
- `references` (optional) — array of CWE/CERT/OWASP identifiers

## Audit-to-Plan Workflow

### Phase 1: Audit Discovery

When an agent is invoked with a request that falls within its domain:

1. Agent checks its frontmatter for `audit-skill` declaration
2. Agent determines audit scope:
   - Use scope from `audit-scope` frontmatter if available
   - Refine scope based on user request (e.g., "audit the cli subsystem" rather than all of the source)
   - If user request is ambiguous, ask for clarification
3. Agent invokes the audit skill with appropriate arguments
4. Audit runs to completion, returning a structured audit report (JSON)
5. Agent stores report in session workspace (e.g., `.copilot/session-state/audit-{agent-id}.json`)

### Phase 2: Finding Prioritization

1. Agent receives audit report with findings array
2. Agent sorts findings by severity: CRITICAL → HIGH → MEDIUM → LOW
3. Agent groups findings by:
   - CWE class (e.g., all CWE-476 findings together)
   - Component or file (e.g., all findings in the CLI module)
   - Dependency relationships (e.g., design fixes before implementation fixes)
4. Agent identifies any blocking findings (CRITICAL or HIGH) that must be addressed first
5. Agent estimates scope and complexity for each group

### Phase 3: Plan Generation

1. Agent creates a structured implementation plan (markdown + YAML frontmatter)
2. Plan document includes:

   **Header (YAML frontmatter)**
   ```yaml
   ---
   title: 'Implementation Plan: CWE-476 Null Check Remediation'
   agent: 'Implementation Secure C Expert'
   audit_skill: 'audit-implementation-weaknesses'
   audit_timestamp: '2026-05-03T10:08:15Z'
   audit_scope: src/
   findings_summary:
     critical: 0
     high: 1
     medium: 0
     low: 0
     total: 1
   ---
   ```

   **Body (markdown)**
   - Executive summary of audit findings
   - Statistics (total findings, breakdown by severity and CWE)
   - Prioritized list of todos
   - Dependencies between todos
   - Validation strategy for each todo
   - Rollback/revert guidance (if applicable)

3. Agent structures todos with:
   - **id** — kebab-case identifier (e.g., `cwe-476-parse-malloc-null-check`)
   - **title** — brief human-readable title
   - **description** — full context including:
     - Finding reference and severity
     - Affected file(s) and line(s)
     - Evidence from audit
     - Concrete fix steps
     - Validation gates (exact make commands)
     - Expected outcome
   - **status** — initially 'pending'

4. Agent inserts todos into SQL `todos` table:
   ```sql
   INSERT INTO todos (id, title, description, status) VALUES
     ('cwe-476-parse-malloc',
      'Add null check after malloc in parse.c',
      'Finding from audit: CWE-476 HIGH severity at parse.c:42. ...',
      'pending');
   ```

5. Agent inserts dependencies into SQL `todo_deps` table if needed:
   ```sql
   INSERT INTO todo_deps (todo_id, depends_on) VALUES
     ('cwe-476-impl-fix', 'design-fix-first');
   ```

### Phase 4: User Review & Approval

1. Agent exits plan mode (calls `exit_plan_mode` tool) to show the plan for user review
2. User sees:
   - Audit summary with finding statistics
   - Executive summary of findings
   - Proposed implementation plan with prioritized todos
   - Option to approve, request changes, or view full plan details
3. If user approves, agent proceeds to execution phase
4. If user requests changes:
   - Agent clarifies with user via `ask_user` tool
   - Agent updates plan based on feedback
   - Agent calls `exit_plan_mode` again for re-review
5. If user declines, plan is abandoned

### Phase 5: Execution Tracking

After user approval:

1. Agent begins work on highest-priority todo
2. Before starting, agent updates todo status:
   ```sql
   UPDATE todos SET status = 'in_progress' WHERE id = 'cwe-476-parse-malloc';
   ```
3. As agent implements fix:
   - Run validation gates locally (e.g., `make test-sanitize`, `make test`)
   - Verify findings are resolved by re-running audit if needed
4. After completing, agent updates todo status:
   ```sql
   UPDATE todos SET status = 'done' WHERE id = 'cwe-476-parse-malloc';
   ```
5. Agent moves to next dependent todo
6. Agent generates summary report when all todos are complete or blocked

## Example: Implementation Secure C Expert

### Agent Frontmatter

```yaml
---
name: 'Implementation Secure C Expert'
description: 'Specialist in CWE-702/CWE-658 implementation weaknesses, CERT C guidance, and systematic C security hardening'
model: GPT-5.4
tools: ['codebase', 'search', 'runCommands', 'edit/editFiles', 'problems', 'usages']
user-invocable: true

# Audit capability
audit-skill: 'audit-implementation-weaknesses'
audit-scope: src/
audit-args: 'cwe_focus=all severity_floor=MEDIUM'
---
```

### Audit Workflow Section in Agent Body

```markdown
## Audit Workflow

This agent follows the unified audit-to-plan workflow:

1. **Audit Discovery** — Invokes `audit-implementation-weaknesses` to scan target scope for CWE-702 and CWE-658 weaknesses.
2. **Finding Prioritization** — Groups findings by severity and CWE class; identifies blocking issues.
3. **Plan Generation** — Creates prioritized todo list with validation gates for each fix.
4. **User Review** — Exits plan mode to request approval before implementation.
5. **Execution Tracking** — Maintains SQL-backed todos; validates fixes against `make test-sanitize`, `make lint`, `make test`.

The agent stores audit results in the session workspace for reproducibility and audit trail.
```

## Validation Gates

All proposed fixes must pass the following validation before being marked complete:

- `make test-sanitize` — AddressSanitizer and UndefinedBehaviorSanitizer must run clean
- `make lint` — Static analysis and code formatting gates
- `make test` — All unit tests must pass
- Domain-specific gates (e.g., `make ci-fast`, `make bench-check`)

Each todo description must include:
- Exact validation commands to run
- Expected outcome (e.g., "all tests pass, no new compiler warnings")
- How to verify the finding is resolved

## SQL Tracking

All todos derived from audit findings use consistent SQL structure:

```sql
-- Create todo from finding
INSERT INTO todos (id, title, description) VALUES
  ('cwe-{id}-{component}',
   '{Brief title}',
   'Audit Finding: CWE-{id} [{severity}] in {file}:{line}.\n{finding description}\nFix: {proposed fix}\nValidation: {gates}\nExpected outcome: {result}');

-- Link dependencies
INSERT INTO todo_deps (todo_id, depends_on) VALUES
  ('cwe-476-impl', 'design-fix-first');

-- Update status as work progresses
UPDATE todos SET status = 'in_progress' WHERE id = 'cwe-476-impl';
UPDATE todos SET status = 'done' WHERE id = 'cwe-476-impl';
```

## Special Cases

### No Findings

If an audit returns zero findings (clean scan):

1. Agent reports "Audit complete — no findings in scope {scope}"
2. Agent offers:
   - Broader scope audit (e.g., expand scope if a subdirectory was scanned)
   - Review of adjacent components or patterns
   - Documentation that this area is in good standing
3. No plan is generated if there is nothing to fix

### Findings with Unclear Fixes

If an audit identifies a weakness but the fix is ambiguous or requires design discussion:

1. Finding is marked as "blocked" with a note explaining the blocker
2. Agent presents findings to user and asks for guidance
3. Plan may include a "design decision" todo before implementation todos

### Cross-Domain Findings

If audit findings span multiple domains (e.g., libsodium API misuse requires both `Libsodium Expert` and `Implementation Secure C Expert`):

1. First agent identifies the cross-domain nature
2. First agent coordinates handoff to second agent per documented routing in `.github/copilot-instructions.md`
3. Second agent runs its audit and adds todos to the same plan
4. User reviews combined plan before any work begins

## Related Files

- Schema: `.github/schemas/audit-report.schema.json`
- Routing guide: `.github/copilot-instructions.md`
- Audit skills: `.github/skills/audit-*/SKILL.md`
- Agent examples: `.github/agents/*.agent.md`
- Customization rules: `.github/instructions/copilot-customization.instructions.md`

