---
description: 'Conventions for writing and maintaining AGENTS.md files'
applyTo: '**/AGENTS.md'
---

# AGENTS.md Instructions

Follow these conventions when editing AGENTS.md files in any project. AGENTS.md is a "README for coding agents" — it provides instructions that AI tools need to work effectively on the codebase.

## Purpose

AGENTS.md complements README.md: README is for human users, AGENTS.md is for AI coding agents (Copilot, Cursor, Codex, Claude, Gemini, etc.). It contains technical context that agents need but would clutter user documentation.

## File Locations

- `AGENTS.md` at the repository root — covers the full monorepo
- `docs/<tool>/AGENTS.md` — tool-specific instructions (closest file takes precedence)

## Required Sections

### Root AGENTS.md

1. **Project Overview** — what the project is, all tools listed with descriptions
2. **Repository Layout** — directory tree with annotations
3. **Technology Stack** — table of languages, frameworks, and tools with versions
4. **Essential Make Targets** — table of key `make` targets
5. **Build and Test** — exact commands for building and testing
6. **Code Conventions** — naming, error handling, testing patterns
7. **Architecture Notes** — shared packages, dependency rules, import patterns
8. **CI/CD** — workflows, release process, required checks

### Per-Tool AGENTS.md

1. **Project Overview** — what this specific tool does
2. **Repository Layout** — tool-specific directory structure
3. **Technology Stack** — same format as root
4. **Essential Make Targets** — tool-relevant targets
5. **Command Structure** — subcommands and flags
6. **API Interaction** — endpoints used, auth patterns
7. **Testing** — tool-specific test patterns

## Content Rules

- Include exact, runnable commands — not vague descriptions
- Use code blocks with language identifiers
- List the primary language standard, compiler requirements, and key library versions
- Document all `make` targets in a table format
- Include toolchain and key dependency version requirements (e.g., C17, libsodium ≥ 1.0.18)
- Describe shared modules and their responsibilities
- Note architecture rules (e.g., "all library logic in `src/`, public API in `include/`")
- Keep technical — this is for agents, not marketing

## Formatting

- Use standard Markdown
- Separate major sections with `---` horizontal rules
- Use tables for structured data (targets, tools, env vars)
- Use fenced code blocks for directory trees and commands
- Keep sections focused — one concern per section

## Updating

Update AGENTS.md when:

- Adding a new tool to the monorepo
- Changing the directory structure
- Adding or modifying Make targets
- Changing build or test commands
- Updating toolchain or key library version requirements
- Adding new shared source modules

## Common Mistakes

- Mixing user documentation (belongs in README) with agent instructions
- Forgetting to update per-tool AGENTS.md when the tool's structure changes
- Using outdated package paths or directory layouts
- Not listing all Make targets
- Omitting the build system entry point and primary toolchain version requirements
