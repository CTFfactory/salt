# Copilot Reusable Pack

This directory contains the implementation scaffold for making the current GitHub Copilot customization set reusable across different repositories.

## Architecture

Use a split-pack model:

1. Base pack: project-agnostic files that can be adopted as-is.
2. Parameterized pack: files that are reusable after placeholder substitution.
3. Optional overlays: domain-specific files that should be opt-in only.

## Goals

- Remove hardcoded project identity and organization assumptions from reusable files.
- Isolate domain-specific workflows (for example Proxmox or Nagios/Icinga2) so they do not affect unrelated repositories.
- Standardize placeholder tokens for project-specific values.
- Provide a repeatable validation checklist for downstream repositories.

## Current Files

- MANIFEST.md: file-by-file classification and migration status.
- VARIABLES.md: canonical placeholder token definitions.
- ADOPTION_CHECKLIST.md: validation and rollout checklist for consumers.
- OVERLAYS.md: opt-in overlay bundles and integration boundaries.

## Validation

Run the pack validation script from repository root:

```sh
bash .github/copilot-pack/scripts/validate-pack.sh
```

The script checks:

- unresolved placeholder tokens in customization files
- broken local markdown links in copilot-pack docs
- missing file paths referenced by MANIFEST.md

## Implementation Notes

- This scaffold is additive and non-breaking.
- Existing customization files remain in place while refactoring proceeds in phases.
- Future changes should update MANIFEST.md status markers as files are migrated.