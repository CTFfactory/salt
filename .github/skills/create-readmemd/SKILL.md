---
name: create-readmemd
description: 'Create a comprehensive README.md for the salt C CLI project'
argument-hint: '[tool or monorepo context]'
---

# Create README.md

You are a code agent. Your task is to create a complete, accurate README.md that serves as the primary user-facing documentation for the project.

## What Makes a Good README

A README should answer three questions immediately: **what** the project does, **how** to install it, and **how** to use it. Everything else is secondary.

## Required Sections

### 1. Title and Badges

```markdown
# tool-name

[![Build](https://github.com/org/repo/actions/workflows/build.yml/badge.svg)](...)
[![Go Report Card](https://goreportcard.com/badge/github.com/org/repo)](...)
[![License](https://img.shields.io/badge/license-Unlicense-blue.svg)](...)

One-line description of what the tool does.
```

### 2. Overview

- 2-4 sentences describing the tool's purpose and primary use cases
- Target audience (sysadmins, developers, blue teamers, etc.)
- Key differentiator — why use this tool over alternatives

### 3. Installation

Document all installation methods available:

```markdown
## Installation

### From Release Binary

Download the latest release from [Releases](https://github.com/org/repo/releases).

### From Source

```bash
go install github.com/org/repo/cmd/tool@latest
```

### Package Manager

```bash
# Debian/Ubuntu
sudo dpkg -i tool_x.y.z_amd64.deb

# RPM
sudo rpm -i tool-x.y.z.x86_64.rpm
```
```

### 4. Quick Start

Show the minimal command to get started — the simplest useful invocation:

```markdown
## Quick Start

```bash
export PROXMOX_URL=https://proxmox:8006/api2/json
export PROXMOX_TOKEN_ID=user@pam!token
export PROXMOX_TOKEN_SECRET=secret

tool --list
```
```

### 5. Usage

- Document all commands and flags
- Show real-world examples for common workflows
- Group flags logically (connection, output, filtering)
- Include both environment variable and flag forms

### 6. Configuration

- List all environment variables with descriptions and defaults
- Document config file format and location (if applicable)
- Show precedence order: flags > env vars > config file > defaults

### 7. Output Formats

Document available output modes (`--human`, `--json`, `--csv`) with examples of each.

## Optional Sections

### Supported Platforms

Table showing OS/architecture matrix:

```markdown
| OS      | amd64 |
|---------|-------|
| Linux   | ✅    |
| Windows | ✅    |
```

### Contributing

Link to CONTRIBUTING.md or provide brief guidelines.

### Security

Link to SECURITY.md. Note vulnerability reporting process.

### License

State the license and link to the full text.

### See Also

Cross-reference related tools in the suite.

## Content Rules

- Write for users, not developers — save internals for AGENTS.md
- Use concrete examples with realistic (but sanitized) values
- Keep examples copy-pasteable — don't use placeholder values that won't work
- Include expected output where it helps understanding
- Don't document internal architecture — that belongs in AGENTS.md
- Don't include development instructions — that belongs in CONTRIBUTING.md
- Use consistent heading levels (# Title, ## Section, ### Subsection)
- Don't use emoji unless the project already uses them consistently

## Monorepo README

For a monorepo with multiple tools, the root README should:

1. Describe the suite/project as a whole
2. List all tools with one-line descriptions and links to their docs
3. Show a unified installation method
4. Provide a quick start that demonstrates the most common tool
5. Link to per-tool documentation in `docs/<tool>/README.md`

## Implementation Steps

1. **Read the codebase** — examine `cmd/*/main.go`, flag definitions, and existing docs
2. **Run the tool** — execute `tool --help` and all subcommands to capture actual output
3. **Check man pages** — align README content with man page documentation
4. **Extract flags** — list all flags from Cobra command definitions
5. **Identify env vars** — grep for `os.Getenv` and Viper bindings
6. **Write the README** — follow the section order above
7. **Validate links** — ensure all internal and external links work

## Repository Description Guidelines

- Description for the GitHub repo must not exceed 350 characters.
