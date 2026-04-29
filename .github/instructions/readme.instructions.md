---
description: 'Conventions for writing and maintaining README.md files'
applyTo: '**/README.md'
---

# README Instructions

Follow these conventions when editing README.md files in any project.

## Structure

README.md files must include these sections in order:

1. **Title and badges** — project name, CI status badge, license badge
2. **Overview** — 2-4 sentences describing purpose and target audience
3. **Installation** — all available installation methods (binary, source, package manager)
4. **Quick Start** — minimal working example (copy-pasteable)
5. **Usage** — commands, flags, and configuration
6. **Configuration** — environment variables, config files, precedence
7. **Output Formats** — if the tool supports `--json`, `--csv`, `--human`
8. **Supported Platforms** — OS/architecture matrix table
9. **Contributing** — link to CONTRIBUTING.md
10. **Security** — link to SECURITY.md
11. **License** — license name and link

Not all sections are required — omit those that don't apply.

## Content Rules

- Write for users (operators, sysadmins), not developers — save internals for AGENTS.md
- Use concrete examples with realistic but sanitized values
- Keep examples copy-pasteable — don't use placeholder values that won't execute
- Include expected output where it helps understanding
- Don't document internal architecture — that belongs in AGENTS.md
- Don't include development instructions — that belongs in CONTRIBUTING.md
- Avoid emoji unless the project uses them consistently
- Use consistent heading levels: `#` title, `##` sections, `###` subsections
- Description for the GitHub repo must not exceed 350 characters.

## Monorepo Root README

The root README should:

- Describe the tool suite as a whole
- List all tools with one-line descriptions
- Provide a unified installation method
- Show a quick start using the most common tool
- Link to per-tool documentation in `docs/<tool>/`

## Per-Tool README

Each tool's documentation in `docs/<tool>/README.md` should be self-contained:

- Full flag documentation
- All environment variables
- Multiple examples covering common workflows
- Output format samples

## Badges

Place badges on the first line after the title:

```markdown
# <project-name>

[![Build](https://github.com/<org>/<repo>/actions/workflows/build.yml/badge.svg)](https://github.com/<org>/<repo>/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-Unlicense-blue.svg)](LICENSE)
```

## Flag Documentation

Document flags in a table or definition list. Include:

- Flag name (long and short form)
- Type and default value
- Corresponding environment variable
- Brief description

```markdown
| Flag | Env Var | Default | Description |
|------|---------|---------|-------------|
| `--url` | `API_URL` | — | API endpoint |
| `--json` | — | `false` | Output in JSON format |
```

## Platform Matrix

```markdown
| OS      | amd64 |
|---------|-------|
| Linux   | ✅    |
| Windows | ✅    |
```

## Updating

When adding new features:

- Add the flag/command to the Usage section
- Add an example showing the new feature
- Update CHANGELOG.md concurrently
- Update the man page to match

When updating README.md, ensure it stays consistent with:

- Man page documentation
- CLI flag and option definitions in the C source (`main.c`, `cli_parse.c`)
- CHANGELOG.md entries
