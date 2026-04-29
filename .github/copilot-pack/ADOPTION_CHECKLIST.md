# Adoption Checklist

Use this checklist when applying the reusable copilot pack to another repository.

## Pre-merge Checks

1. Confirm all adopted files use valid frontmatter fields for their file type.
2. Confirm all tools in tools lists are valid for current VS Code Copilot.
3. Confirm all model names are currently available or provide fallback arrays.
4. Confirm applyTo patterns match intended files in the target repository.
5. Confirm there are no unresolved placeholders in adopted files.
6. Confirm prompts and skills do not reference missing agents or files.

## Base Pack Validation

1. Run a full search for hardcoded source-project terms and remove any unintended leftovers.
2. Verify generic instruction files do not require optional overlay directories.
3. Verify references to AGENTS.md or copilot-instructions.md are conditional when those files are absent.

## Overlay Validation

1. Add overlays only when the target repository uses that domain.
2. Validate overlay-specific environment variable names and auth examples.
3. Validate overlay applyTo patterns do not capture unrelated files.
4. Validate base behavior remains unchanged when overlay files are removed.

## Recommended Rollout

1. Pilot on one small repository.
2. Pilot on one larger repository with different structure.
3. Compare validation results and update templates.
4. Tag a baseline reusable-pack version after both pilots pass.