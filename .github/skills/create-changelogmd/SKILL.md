---
name: create-changelogmd
description: 'Create or update a CHANGELOG.md following Keep a Changelog format and Semantic Versioning'
argument-hint: '[version or blank for Unreleased]'
---

# Create CHANGELOG.md

You are a code agent. Your task is to create or update a CHANGELOG.md file following the [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format and [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## File Structure

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- New features go here during development

## [X.Y.Z] - YYYY-MM-DD

### Added
- Feature description

### Fixed
- Bug fix description
```

## Section Types

Use only these section headers, in this order when present:

1. `### Added` — new features
2. `### Changed` — changes to existing functionality
3. `### Deprecated` — features that will be removed
4. `### Removed` — features that have been removed
5. `### Fixed` — bug fixes
6. `### Security` — vulnerability fixes

## Entry Format

- Each entry is a bullet point starting with `- `
- In monorepos, prefix tool-specific changes with the tool name in backticks: `` - `proxmv` — added rename mode ``
- Use past tense: "Added", "Fixed", "Removed"
- Be specific and concise — describe the user-visible effect, not implementation details
- Do not include commit hashes, PR numbers, or author names

## Version Rules

### Semantic Versioning

- **MAJOR** (X.0.0): breaking changes to CLI flags, environment variables, config file format, or output structure
- **MINOR** (0.Y.0): new commands, flags, output formats, or tools
- **PATCH** (0.0.Z): bug fixes, performance improvements, documentation fixes

### Release Workflow

1. During development, add entries under `[Unreleased]`
2. When tagging a release, rename `[Unreleased]` to `[X.Y.Z] - YYYY-MM-DD`
3. Create a fresh empty `[Unreleased]` section above the new release
4. Date format: `YYYY-MM-DD` (ISO 8601)

## Implementation Steps

1. **Check for existing CHANGELOG** — if one exists, validate formatting and fix issues
2. **Gather history** — review git log, PRs, and existing documentation for notable changes
3. **Create/update the file** — follow the structure above
4. **Populate entries** — add meaningful entries for each version
5. **Validate** — ensure `[Unreleased]` section exists, dates are ISO 8601, sections are in correct order

## Formatting Rules

- Use `---` horizontal rule between the preamble and the first version section
- Do not add blank lines between entries within a section
- Add one blank line between sections
- Do not use sub-headers beyond `###` inside a version block
- Do not use markdown tables or complex formatting inside entries

## Common Mistakes to Avoid

- Do not remove the `[Unreleased]` section after a release — recreate it empty
- Do not backfill entries into already-released versions
- Do not use `### Breaking Changes` — use `### Changed` or `### Removed` and note the breaking nature
- Do not include commit hashes or PR numbers (the git tag provides traceability)
- Do not use future tense ("Will add...") — use past tense
- Do not leave the `[Unreleased]` section empty without at least a placeholder comment
