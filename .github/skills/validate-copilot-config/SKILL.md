---
name: validate-copilot-config
description: 'Audit and fix all GitHub Copilot customization files: validate frontmatter, tool names, model names, and cross-file consistency'
argument-hint: '[scope path or blank for full repository]'
---

# Validate Copilot Configuration

You are a code agent. Your task is to audit all GitHub Copilot customization files in the repository and fix any issues found.

Use `.github/instructions/copilot-customization.instructions.md` as the authoritative source for schema, tool-name validity, model-name validity, overlap precedence, and deprecation policy. If this skill and that instruction file diverge, follow the instruction file.

## Inventory

First, list all customization files:

```bash
# Agents
ls -1 .github/agents/*.agent.md

# Instructions
ls -1 .github/instructions/*.instructions.md

# Prompts
ls -1 .github/prompts/*.prompt.md

# Skills
find .github/skills -name SKILL.md

# Project-wide
ls -1 .github/copilot-instructions.md
```

## Validation Rules

### Frontmatter Schema

| File Type | Required Fields | Optional Fields | Forbidden Fields |
|-----------|----------------|----------------|------------------|
| `.agent.md` | `name`, `description`, `tools` | `model`, `argument-hint`, `agents`, `user-invocable`, `disable-model-invocation`, `target`, `handoffs`, `hooks`, `mcp-servers` | `applyTo` |
| `.instructions.md` | `description`, `applyTo` | `name`, `excludeAgent` | `tools`, `model` |
| `.prompt.md` | `description` | `name`, `argument-hint`, `agent`, `model`, `tools` | `applyTo` |
| `SKILL.md` | `name`, `description` | `argument-hint`, `user-invocable`, `disable-model-invocation` | `applyTo`, `tools`, `model` |

### Tool and Model Policy

Do not maintain tool or model lists in this skill. Read them from the SSOT instead:

- Allowed and invalid tool names: `Confirmed Valid Tool Names` section plus the `allowed_tools` and `invalid_tools` keys in the `Machine-Readable Policy` block of `.github/instructions/copilot-customization.instructions.md`.
- Approved, retiring, and retired model lists with migration targets: `Confirmed Valid Model Names` section plus the `approved_models`, `retiring_models`, and `retired_models` keys in the same machine-readable block.

Severity mapping:

- Tool in `invalid_tools` or absent from `allowed_tools` -> **ERROR**.
- Model in `retired_models` or absent from `approved_models` -> **ERROR**.
- Model in `retiring_models` -> **WARNING** with migration target from the policy block.

The automated implementation lives in `scripts/validate-copilot-config.sh`; this skill describes the manual review that should produce the same findings.

## Audit Workflow

### Step 1: Parse and Validate Frontmatter

For each file, extract the YAML frontmatter between `---` delimiters and check:

1. All required fields present for the file type
2. No forbidden fields present
3. Field values are correctly typed (strings quoted, tools is a list)

### Step 2: Validate Tool Names

For every `tools:` list entry in agent files and prompt files:

1. Check against the confirmed valid list above
2. Flag any invalid tools as ERROR
3. Suggest the correct replacement

### Step 3: Validate Model Names

For every `model:` field in agent files and prompt files:

1. Check against the confirmed valid model list (GA + Preview)
2. Flag retired models as ERROR
3. Flag retiring models as WARNING with migration guidance
4. If `model` is an array, validate each entry in the list
5. Suggest replacement from recommended models

### Step 4: Check Cross-References

1. Every agent listed in `AGENTS.md` Custom Agent Selection Guide → verify `.agent.md` file exists
2. Every skill referenced in `copilot-instructions.md` → verify `SKILL.md` exists
3. Every instruction file referenced in `copilot-instructions.md` → verify `.instructions.md` exists
4. Skill `name:` field matches parent directory name (max 64 characters)
5. Check for deprecated `infer` field in agent files → suggest `user-invocable` / `disable-model-invocation`
6. Check for deprecated `.chatmode.md` file extensions → suggest renaming to `.agent.md`
7. Verify markdown file references inside customization files resolve to real repository files
8. Check for duplicate agent display names across `.agent.md` files

### Step 5: Check applyTo Coverage

For each instruction file's `applyTo` patterns:

1. Run glob match against workspace files
2. Flag patterns that match zero files as WARNING

### Step 6: Apply Fixes

For each ERROR finding:

1. Replace invalid tool names with confirmed replacements
2. Replace retired model names with recommended GA replacements
3. Add missing required frontmatter fields
4. Remove forbidden frontmatter fields
5. Replace deprecated `infer` with `user-invocable` / `disable-model-invocation`

### Step 7: Report

Produce a summary table:

| Severity | File | Issue | Status |
|----------|------|-------|--------|
| ERROR | file.agent.md | Invalid tool `terminalCommand` | FIXED → `runCommands` |
| ERROR | file.agent.md | Retired model `Gemini 3 Pro` | FIXED → `Gemini 3.1 Pro` |
| WARNING | file.agent.md | Retiring model `GPT-5.1` (2026-04-01) | FLAGGED |
| WARNING | file.instructions.md | `applyTo` matches 0 files | FLAGGED |
| OK | file.prompt.md | All checks passed | — |

## Post-Fix Verification

After applying fixes:

1. Run VS Code error checker on all modified files
2. Verify no new errors introduced
3. Confirm all frontmatter parses correctly
