---
description: 'Instructions for maintaining the CHANGELOG following Keep a Changelog and Semantic Versioning'
applyTo: '**/CHANGELOG.md'
---

# Changelog Instructions

Follow [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format and [Semantic Versioning](https://semver.org/spec/v2.0.0.html) when editing CHANGELOG.md.

## File Structure

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [X.Y.Z] - YYYY-MM-DD

### Added
- ...
```

## Rules

### Section headers

This project does **not** use the Keep-a-Changelog `### Added` / `### Changed` / `### Fixed` / `### Security` subsections. Both `[Unreleased]` and released version blocks are flat feature listings (see "[Unreleased] section" and "Released sections" below). The category vocabulary is retained only as informal classification when triaging which entries deserve a bullet.

### [Unreleased] section

- Optional during development
- **Same flat feature-listing format as released sections** (see "Released sections" below) — no `### Added` / `### Changed` / `### Fixed` / `### Security` subsections
- One bullet per user-visible feature, capability, or security guarantee
- Cull entries that do not deserve mention as a feature: internal refactors, follow-up fixes that are subsumed by the feature bullet they belong to, diagnostic-text polish, and pure code-hygiene changes
- On release, rename `[Unreleased]` to `[X.Y.Z] - YYYY-MM-DD`; no restructuring is needed because the format already matches

### Released sections

Released versions are a flat feature listing rather than a categorized add/change/fix breakdown. The release notes describe the *capabilities of the released artifact*, not the development history that produced it.

Rules for released versions:

- No `### Added` / `### Changed` / `### Fixed` / `### Security` subsections under the version header
- One bullet per user-visible feature, capability, or guarantee
- Phrase entries declaratively in the present tense (the feature *is*, not *was added*)
- Drop pure refactors, internal cleanups, and intermediate fixes that are subsumed by a later entry in the same release
- Keep security guarantees as standalone bullets so operators can scan them quickly

Example:

```markdown
## [1.2.0] - 2026-05-01

- `--json` output mode emits a REST-ready envelope with control-character escaping per RFC 8259.
- Plaintext, decoded key, and ciphertext buffers are `sodium_mlock`'d so they cannot be paged to swap.
```

### Entry format

- Each entry is a bullet point starting with `- `
- Start with the tool name in backticks when the change is tool-specific: `` - `proxmv` — added rename mode ``
- Use past tense: "Added", "Fixed", "Removed"
- Be specific and concise — describe the user-visible effect, not implementation details

### Versioning

- MAJOR: breaking changes to CLI flags, environment variables, config file format, or output structure
- MINOR: new commands, flags, output formats, or tools
- PATCH: bug fixes, performance improvements, documentation fixes

### Formatting

- Use `---` horizontal rule between the preamble and the first version section
- Date format: `YYYY-MM-DD` (ISO 8601)
- Do not add blank lines between entries within a section
- Add one blank line between sections
- Do not use sub-headers beyond `###` inside a version block (use bold text for grouping if needed)

## Common Mistakes to Avoid

- Do not leave a stale `[Unreleased]` section in release branches or tags
- Do not backfill entries into already-released versions
- Do not use `### Breaking Changes` — use `### Changed` or `### Removed` and note the breaking nature in the entry text
- Do not include commit hashes or PR numbers in entries (the git tag provides traceability)
