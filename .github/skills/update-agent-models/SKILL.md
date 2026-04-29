---
name: update-agent-models
description: 'Weekly maintenance workflow: audit model lifecycle for all .github/agents/*.agent.md files, select approved replacements per repo policy, apply surgical edits, and run the GNU build-system validation gate'
argument-hint: '[agent glob or blank for all agents]'
user-invocable: true
---

# Update Agent Models

You are a code agent performing the weekly Copilot agent model maintenance cycle for this
repository. Your single source of truth is:

```
.github/instructions/copilot-customization.instructions.md
```

If this skill and that instruction file diverge on any policy detail, follow the instruction
file. Do **not** invent model names, tool names, or policy rules from memory.

---

## When to invoke

Invoke this skill (type `/update-agent-models` in Copilot chat) on a weekly cadence, or
immediately when:

- A vendor announces a model retirement date
- A new flagship model is released and added to the approved list
- An agent is added or its domain responsibility changes materially

---

## Step 1 — Extract the live policy block

Parse the machine-readable policy embedded in the SSOT instruction file. The block is
delimited by HTML comment markers:

```bash
awk '/<!-- BEGIN COPILOT POLICY -->/{flag=1;next}/<!-- END COPILOT POLICY -->/{flag=0}flag' \
  .github/instructions/copilot-customization.instructions.md
```

From this block extract four lists:
- `approved_models` — models that may be used freely
- `retiring_models` — models that need a planned migration (WARNING severity)
- `retired_models` — models that must be replaced immediately (ERROR severity)
- (ignore `allowed_tools` and `invalid_tools` — those are out of scope for this skill)

Do **not** hard-code model names. Always re-read the policy block at the start of each run
so that future policy edits are picked up automatically.

---

## Step 2 — Collect current model assignments

```bash
grep -n "^model:" .github/agents/*.agent.md
```

Build a table with three columns: `file`, `current model`, `policy status`.

Policy status is one of:
- `OK` — model is in `approved_models`
- `WARNING` — model is in `retiring_models` (plan migration)
- `ERROR` — model is in `retired_models` (replace immediately)
- `UNKNOWN` — model is not in any list (treat as ERROR)

---

## Step 3 — Determine required and optional changes

### Mandatory changes (ERROR / UNKNOWN)

Any agent whose `model:` has status `ERROR` or `UNKNOWN` **must** be updated before the
run is complete. Use the migration target from `retired_models` in the policy block.

### Advisory upgrades (cost/performance alignment)

Even when all models are `OK`, evaluate whether each agent's model matches its domain.
Apply the selection guide below. Only upgrade when the improvement is clear and the cost
increase is warranted — changes for their own sake are not acceptable.

#### Selection guide (ordered by domain)

| Agent domain | Preferred model | Rationale |
|---|---|---|
| Architecture, security analysis, orchestrators | `GPT-5.4` | Deep multi-step reasoning, code-architecture decisions |
| Code-generation heavy / agentic (compiler, test, fuzz) | `GPT-5.3-Codex` | Purpose-built for complex code tasks, edit-test loops |
| Edit-test loops with high tool precision | `Gemini 3.1 Pro` | Efficient tool use, scripting workflows |
| General-purpose coding, config, writing | `Claude Sonnet 4.5` or `Claude Sonnet 4.6` | Best balance of quality and cost at 1x multiplier |
| Fast / repetitive / cost-sensitive tasks | `Claude Haiku 4.5` (0.33x) or `GPT-5.4 mini` (0.33x) | Low-stakes tasks where speed matters more than depth |

#### Mandatory orchestrators (always warrant flagship reasoning)

The following agents are designated mandatory orchestrators in this repository and should
use `GPT-5.4` unless there is a specific reason documented in a comment:

```
copilot-config-expert   principal-software-engineer   prompt-engineer
repo-architect          cicd-pipeline-architect        release-automation-expert
devops-expert
```

> Note: `copilot-config-expert` uses `Claude Sonnet 4.6` by policy — its domain is
> schema/instruction validation where Sonnet 4.6 outperforms on structured reasoning.
> `prompt-engineer` focuses on NLP/instruction crafting where Sonnet 4.5 excels.
> These are deliberate exceptions; do not change them without documented justification.

---

## Step 4 — Apply changes

For each agent requiring a model update:

1. Open the agent file.
2. Locate the `model:` line in the frontmatter (between the `---` delimiters).
3. Replace **only** that line — do not touch surrounding frontmatter or body content.
4. Record the change: `file | old model -> new model | reason`.

Example surgical replacement:

```
# Before
model: Claude Sonnet 3.7

# After
model: Claude Sonnet 4.5
```

If the `model:` line is absent and the agent file has no model set, add it after the
`description:` line using the domain-appropriate model from the selection guide.

---

## Step 5 — Run the validation gate

```bash
cd build/autotools && make validate-copilot
```

The Makefile target calls `scripts/validate-copilot-config.sh` with the repository root.

**Exit code semantics:**

| Exit code | Meaning | Action |
|---|---|---|
| `0` | Clean — no errors, no warnings | Done |
| `1` | One or more ERRORs (blocking) | Fix each `ERROR:` line and re-run |
| `2` | Warnings only (advisory) | Review each `WARNING:` line; fix or document why acceptable |

If the validator exits `1`, iterate: fix the reported error, re-run, repeat until exit `0`.

---

## Step 6 — Produce a change summary

After a clean validation run, output a Markdown table documenting every decision made in
this cycle:

```markdown
## Agent Model Audit -- <ISO date>

| Agent file | Before | After | Reason |
|---|---|---|---|
| `clang-c-expert.agent.md` | `Claude Sonnet 4.5` | `GPT-5.3-Codex` | Code-heavy agentic: sanitizer workflows + static-analysis C fixes |
| `valgrind-expert.agent.md` | `Claude Sonnet 4.5` | `GPT-5.4` | Deep reasoning: subtle runtime memory diagnostics |
| `vhs-expert.agent.md` | `Claude Sonnet 4.5` | *(no change)* | Tape authoring; moderate complexity; cost-appropriate |
```

Include **all** agents in the table, not just changed ones. "No change" rows provide the
audit trail showing each agent was reviewed.

---

## Step 7 — Schedule the next review

After completing the cycle, note in the summary:

- Date of this run
- Any models currently in `retiring_models` and their planned retirement date
- Recommended date for next run (default: 7 days from today, or earlier if a retirement
  date falls within 14 days)

---

## Constraints and guardrails

- **Never use retired models.** The validator will catch them as blocking errors.
- **Never use model names not in `approved_models`.** Unknown models are treated as errors.
- **Do not add `model:` to instruction files** — that field is forbidden there.
- **Do not add `model:` to skill files** — that field is forbidden there.
- **One surgical line per agent** — change only the `model:` value; leave all other
  frontmatter and body content untouched.
- **Cost awareness** — upgrading to a higher-multiplier model requires justification.
  Moving from 1x to 3x (Opus) or 30x (Opus fast mode) is never acceptable for routine
  tasks. Prefer `GPT-5.4` (1x) over Opus unless the task genuinely demands it.
- **Array model syntax** is valid when fallback ordering is needed, e.g.
  `model: [Claude Sonnet 4.6, Claude Sonnet 4.5]`. Use it only when intentional.
