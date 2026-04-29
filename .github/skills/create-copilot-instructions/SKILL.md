---
name: create-copilot-instructions
description: 'Create a new domain-specific instruction file when current coverage gaps require file-scoped guidance'
argument-hint: '[file type or workflow]'
---

# Create Copilot Instructions File

Use this skill only when adding a **new** instruction file for a file type or workflow that currently lacks coverage. For updates to existing files, edit directly.

## When to Use This Skill

✅ Use when:
- A new file type is introduced (e.g., first Python script in a C repo)
- A new workflow requires file-scoped rules (e.g., InSpec tests added)
- Existing instruction coverage has a measurable gap

❌ Do NOT use when:
- The file type already has an instruction file — edit it directly
- The rule applies project-wide — add to `copilot-instructions.md` instead
- Only 1-2 files of this type exist — avoid premature abstraction

## Workflow

1. **Check for existing coverage.**
   ```sh
   ls -1 .github/instructions/*.instructions.md
   ```
   Read `.github/instructions/copilot-customization.instructions.md` overlap precedence matrix.

2. **Survey the target files.**
   Identify patterns, conventions, and existing style in the files you're writing instructions for.

3. **Draft minimal frontmatter.**
   ```yaml
   ---
   description: 'Brief description of what these instructions cover'
   applyTo: 'glob/pattern/**/*.ext'
   ---
   ```
   Test the glob pattern matches intended files:
   ```sh
   # Bash glob test
   shopt -s globstar nullglob
   files=( glob/pattern/**/*.ext )
   printf '%s\n' "${files[@]}"
   ```

4. **Write actionable rules.**
   - Be concrete: "Use `sodium_memzero()` before `free()` for secret buffers"
   - Not vague: "Handle memory safely"
   - Include code examples for DO and DON'T patterns

5. **Validate against SSOT.**
   Confirm the new file follows `.github/instructions/copilot-customization.instructions.md` schema and doesn't conflict with existing overlap precedence rules.

6. **Run validation.**
   ```sh
   scripts/validate-copilot-config.sh
   ```

## Naming Convention

`<topic>.instructions.md` where `<topic>` is lowercase, hyphen-separated, and describes the file type or workflow.

Examples from this repository:
- `c-memory-safety.instructions.md`
- `cmocka-testing.instructions.md`
- `gnu-autotools.instructions.md`
- `sbom.instructions.md`

## Required Frontmatter

Refer to `.github/instructions/copilot-customization.instructions.md` for the authoritative schema. At minimum:
- `description:` (quoted string, one line)
- `applyTo:` (comma-separated glob patterns)

## Anti-Patterns

- Creating instruction files for domains not present in the repo
- Duplicating rules from existing instruction files
- Writing instructions for a single file (use inline comments instead)
- Including implementation code (instructions define conventions, not solutions)
