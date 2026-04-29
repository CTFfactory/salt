---
description: 'Conventions for GitHub Copilot customization files: agents, instructions, prompts, and skills'
applyTo: '**/.github/agents/*.agent.md,**/.github/instructions/*.instructions.md,**/.github/prompts/*.prompt.md,**/.github/skills/*/SKILL.md,**/.github/copilot-instructions.md'
---

# Copilot Customization File Instructions

Follow these conventions when creating or editing GitHub Copilot customization files in any project.

## File Types and Frontmatter

### Agent Files — `.github/agents/<name>.agent.md`

**Required frontmatter:**
```yaml
---
name: 'Display Name'
description: 'One-line description of the agent role and focus area'
model: Claude Sonnet 4.5
tools: ['codebase', 'search', 'runCommands', 'edit/editFiles', 'problems']
---
```

**Optional frontmatter fields:**
- `argument-hint` — hint text shown in chat input field
- `agents` — list of subagent names (use `'*'` for all, `[]` for none)
- `user-invocable` — boolean, show in agents dropdown (default: true)
- `disable-model-invocation` — boolean, prevent subagent invocation (default: false)
- `target` — `'vscode'` or `'github-copilot'`
- `handoffs` — list of workflow transitions to other agents
- `hooks` — scoped hook commands (Preview)
- `mcp-servers` — MCP server configs for GitHub Copilot agents

**Rules:**
- `name` — human-readable, title case, quoted string
- `description` — one sentence, quoted string, ends without period
- `model` — must be a confirmed valid model; can be a string or prioritized array
- `tools` — YAML list of confirmed valid tool names (see below)
- `infer` is **deprecated** — use `user-invocable` and `disable-model-invocation` instead
- File name: `kebab-case.agent.md` matching the agent's slug

**Sub-agent convention (hybrid policy):**
- Mandatory for orchestrator and cross-domain agents: include at least one of `agents` or `handoffs`
- Mandatory orchestrator set in this repository: `copilot-config-expert`, `principal-software-engineer`, `prompt-engineer`, `repo-architect`, `cicd-pipeline-architect`, `release-automation-expert`, `devops-expert`
- Optional for specialist agents; if omitted, explain routing in a `Related Agents` section
- Prefer `handoffs` when the next step is sequential workflow guidance; prefer `agents` for selectable delegation menus

**Body structure:**
1. H1 heading matching or describing the agent role
2. "You are..." opening paragraph defining expertise
3. Bullet lists of specific responsibilities and focus areas
4. Optional sections for domain rules, output format, constraints

### Instruction Files — `.github/instructions/<topic>.instructions.md`

**Required frontmatter:**
```yaml
---
description: 'What these instructions cover and when they apply'
applyTo: '**/glob/pattern/*.ext'
---
```

**Optional frontmatter fields:**
- `name` — display name shown in the UI (defaults to filename)
- `excludeAgent` — `'code-review'` or `'coding-agent'` to prevent use by specific agents

**Rules:**
- `applyTo` — comma-separated glob patterns that match actual files in the repo
- Content should be concrete, actionable rules — not vague guidance
- Keep under 200 lines to fit in Copilot context windows
- When `applyTo` is omitted, the instructions are not applied automatically but can be attached manually
- VS Code also uses `description` for semantic matching to auto-apply to relevant tasks

## Instruction Overlap Precedence Matrix

When multiple instruction files match the same file, apply rules in this order:

| File class | Order (first to last) |
|------|------|
| GitHub workflows (`.github/workflows/*.yml`) | `dependency-pinning.instructions.md` -> `actionlint.instructions.md` -> `ci.instructions.md` -> `sbom.instructions.md` -> `vhs-tape.instructions.md` |
| Makefiles (`**/Makefile*`, `**/*.mk`) | `dependency-pinning.instructions.md` -> `makefile.instructions.md` -> `sbom.instructions.md` -> `vhs-tape.instructions.md` |
| READMEs (`**/README.md`) | `readme.instructions.md` -> `github-secrets-integration.instructions.md` -> `sbom.instructions.md` |
| Changelogs (`**/CHANGELOG.md`) | `changelog.instructions.md` -> `sbom.instructions.md` |
| C sources (`**/src/**/*.c`) | `c-memory-safety.instructions.md` -> `libsodium-patterns.instructions.md` -> `code-smells.instructions.md` |
| C headers (`**/include/**/*.h`) | `libsodium-patterns.instructions.md` -> `code-smells.instructions.md` |
| C tests (`**/tests/**/*.c`) | `cmocka-testing.instructions.md` -> `code-smells.instructions.md` |

Conflict rule: if two instructions disagree, follow the first item listed in the matrix row (highest precedence) and add a comment in the change summary noting the override.

### Prompt Files — `.github/prompts/<name>.prompt.md`

**Required frontmatter:**
```yaml
---
description: 'What this review or analysis prompt does'
---
```

**Optional frontmatter fields:**
- `name` — display name used after typing `/` in chat (defaults to filename)
- `argument-hint` — hint text shown in chat input field
- `agent` — which agent to use: `'ask'`, `'agent'`, `'plan'`, or a custom agent name
- `model` — model override for this prompt
- `tools` — list of tools available when running this prompt (overrides agent tools)

**Rules:**
- No `applyTo` field — prompts are invoked manually via `/` slash commands
- Simplest frontmatter of all four types (only `description` required)
- Body should be a complete review/analysis template
- Use structured output sections (tables, checklists, severity ratings)
- When `tools` is specified, those tools take precedence over the agent's tool list

### Skill Files — `.github/skills/<name>/SKILL.md`

**Required frontmatter:**
```yaml
---
name: kebab-case-name
description: 'What this skill does and when to invoke it'
---
```

**Optional frontmatter fields:**
- `argument-hint` — hint for slash command input (e.g., `'[test file] [options]'`)
- `user-invocable` — boolean, show as `/` slash command (default: true)
- `disable-model-invocation` — boolean, prevent auto-loading by the model (default: false)

**Rules:**
- `name` must be kebab-case, match the parent directory name, max 64 characters
- `description` max 1024 characters
- Body contains complete step-by-step workflow instructions
- Include code blocks, examples, and expected outputs
- Skills are self-contained — agent should be able to follow without external context
- Skill directories can include scripts, examples, and other resources alongside `SKILL.md`
- Skills follow the [Agent Skills open standard](https://agentskills.io/) and are portable across AI agents

## Confirmed Valid Tool Names

Only use these tool names in agent `tools:` lists:

| Tool | Purpose |
|------|---------|
| `codebase` | Workspace-wide code understanding |
| `search` | Text and regex search across files |
| `edit/editFiles` | Create and edit files |
| `runCommands` | Execute terminal commands |
| `problems` | Read editor diagnostics and errors |
| `testFailure` | Access test failure details |
| `usages` | Find references and symbol usages |
| `fetch` | Fetch web content |
| `web/fetch` | Fetch web content (namespaced) |
| `changes` | View pending file changes |
| `search/codebase` | Semantic code search |
| `extensions` | Query installed extensions |
| `githubRepo` | Access GitHub repository metadata |
| `new` | Create new workspaces/files |
| `runTasks` | Execute VS Code tasks |
| `vscodeAPI` | Interact with VS Code extension API |
| `searchResults` | Access search result views |
| `runCommands/terminalLastCommand` | Access last terminal command output |
| `runCommands/terminalSelection` | Access terminal selection |

**Never use** (invalid — will cause VS Code errors):
- ~~`terminalCommand`~~ → use `runCommands`
- ~~`findTestFiles`~~ → use `search`
- ~~`runTests`~~ → use `runCommands`
- ~~`openSimpleBrowser`~~ → use `fetch` or `web/fetch`
- ~~`github`~~ → use `githubRepo`

## Confirmed Valid Model Names

### Model Lifecycle

| Status | Action | Validator Severity |
|--------|--------|--------------------|
| **Approved** | Use freely | OK |
| **Retiring** | Plan migration to listed target before retirement date | WARNING (advisory) |
| **Retired** | Replace immediately with migration target | ERROR (blocking) |

| Model | Status | Notes / Migration Target |
|-------|--------|-------------------------|
| `Claude Sonnet 4.5` | Approved | Most common in this project. GA. 1× multiplier. |
| `Claude Sonnet 4.6` | Approved | Latest Sonnet. GA. 1× multiplier. |
| `GPT-5.2` | Approved | Deep reasoning. GA. 1× multiplier. Superseded by GPT-5.4 for new agents. |
| `GPT-5.4` | Approved | Latest OpenAI flagship. Used for principal/security/E2E agents. GA. 1× multiplier. |
| `GPT-5.3-Codex` | Approved | Agentic tasks. GA. 1× multiplier. |
| `Gemini 3.1 Pro` | Approved | Edit-test loops. Public preview. 1× multiplier. |
| `Claude Haiku 4.5` | Approved | Fast/cheap tasks. GA. 0.33× multiplier. |
| `GPT-5.4 mini` | Approved | Codebase exploration. GA. 0.33× multiplier. |
| `GPT-5.1` | Retired (2026-04-01) | Migrate to `GPT-5.4`. |
| `GPT-5.1-Codex` | Retired (2026-04-01) | Migrate to `GPT-5.3-Codex`. |
| `GPT-5.1-Codex-Max` | Retired (2026-04-01) | Migrate to `GPT-5.3-Codex`. |
| `GPT-5.1-Codex-Mini` | Retired (2026-04-01) | Migrate to `GPT-5.4 mini`. |
| `GPT-5` | Retired | Migrate to `GPT-5.4`. |
| `GPT-5-Codex` | Retired | Migrate to `GPT-5.3-Codex`. |
| `Gemini 3 Pro` | Retired | Migrate to `Gemini 3.1 Pro`. |
| `Claude Opus 4.1` | Retired | Migrate to `Claude Sonnet 4.6`. |
| `Claude Opus 4` | Retired | Migrate to `Claude Sonnet 4.6`. |
| `Claude Sonnet 3.7` | Retired | Migrate to `Claude Sonnet 4.5`. |
| `Claude Sonnet 3.5` | Retired | Migrate to `Claude Sonnet 4.5`. |
| `o1-mini` | Retired | Migrate to `GPT-5.4 mini`. |
| `o3` | Retired | Migrate to `GPT-5.4`. |
| `o3-mini` | Retired | Migrate to `GPT-5.4 mini`. |
| `o4-mini` | Retired | Migrate to `GPT-5.4 mini`. |

Cadence: review this table quarterly, or immediately when a vendor announces a retirement date.

## Naming Conventions

| Type | File pattern | Example |
|------|-------------|---------|
| Agent | `kebab-case.agent.md` | `golang-expert.agent.md` |
| Instruction | `kebab-case.instructions.md` | `batch-engine.instructions.md` |
| Prompt | `kebab-case.prompt.md` | `code-review.prompt.md` |
| Skill | `kebab-case/SKILL.md` | `create-benchmarks/SKILL.md` |

**Skill suffix policy:** the `-skill` directory suffix is permitted but optional. New skills should omit the suffix unless the unsuffixed name would clash with an existing directory. Existing suffixed directories may remain to preserve link stability.

## Common Mistakes

- Quoting model names (`model: 'Claude Sonnet 4.5'` works but is unnecessary)
- Forgetting that instruction `applyTo` patterns are comma-separated, not YAML lists
- Adding `applyTo` to prompt files (prompts don't have automatic triggers)
- Using `name:` in instruction files (optional since VS Code supports it, but filename is the primary ID)
- Tool names are case-sensitive — `RunCommands` is invalid, `runCommands` is correct
- Using deprecated `infer` field instead of `user-invocable` / `disable-model-invocation`
- Using old `.chatmode.md` extension instead of `.agent.md`
- Forgetting that `tools` in prompt files override the agent's tool list
- Adding `tools` or `model` fields to instruction files (not supported)

## Machine-Readable Policy

This block is the single source of truth consumed by `scripts/validate-copilot-config.sh`.
Do **not** duplicate these lists into prompts, skills, or other instruction files; reference this
section by name instead. Keep the YAML between the BEGIN/END markers below valid and do not
remove the markers — the validator parses by exact match.

<!-- BEGIN COPILOT POLICY -->
```yaml
allowed_tools:
  - codebase
  - search
  - edit/editFiles
  - runCommands
  - problems
  - testFailure
  - usages
  - fetch
  - web/fetch
  - changes
  - search/codebase
  - extensions
  - githubRepo
  - new
  - runTasks
  - vscodeAPI
  - searchResults
  - runCommands/terminalLastCommand
  - runCommands/terminalSelection
invalid_tools:
  terminalCommand: runCommands
  findTestFiles: search
  runTests: runCommands
  openSimpleBrowser: fetch
  github: githubRepo
approved_models:
  - Claude Sonnet 4.5
  - Claude Sonnet 4.6
  - GPT-5.2
  - GPT-5.4
  - GPT-5.3-Codex
  - Gemini 3.1 Pro
  - Claude Haiku 4.5
  - GPT-5.4 mini
retiring_models:
retired_models:
  GPT-5.1: GPT-5.4
  GPT-5.1-Codex: GPT-5.3-Codex
  GPT-5.1-Codex-Max: GPT-5.3-Codex
  GPT-5.1-Codex-Mini: GPT-5.4 mini
  GPT-5: GPT-5.4
  GPT-5-Codex: GPT-5.3-Codex
  Gemini 3 Pro: Gemini 3.1 Pro
  Claude Opus 4.1: Claude Sonnet 4.6
  Claude Opus 4: Claude Sonnet 4.6
  Claude Sonnet 3.7: Claude Sonnet 4.5
  Claude Sonnet 3.5: Claude Sonnet 4.5
  o1-mini: GPT-5.4 mini
  o3: GPT-5.4
  o3-mini: GPT-5.4 mini
  o4-mini: GPT-5.4 mini
```
<!-- END COPILOT POLICY -->
