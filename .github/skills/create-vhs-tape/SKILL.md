---
name: create-vhs-tape
description: 'Create and refine deterministic VHS tape demos for the salt CLI, including pacing, waits, and release-ready output assets'
argument-hint: '[demo scenario or tape path]'
---

# Create VHS Tape

Create or update a Charmbracelet VHS tape for this repository with deterministic behavior and clean presentation.

## Inputs to Gather

- Demo objective (feature walk-through, quickstart, release demo, troubleshooting)
- Target outputs (GIF, MP4, or both)
- Required commands and expected terminal output
- Any constraints (max runtime, CI compatibility, branding style)

## Workflow

1. Inspect existing tape files, companion scripts, and Makefile/workflow targets that invoke VHS.
2. Draft a `.tape` script with explicit timing and deterministic waits.
3. Include presentation settings appropriate for release assets.
4. Add output directives for requested formats.
5. Add or update a companion shell script when orchestration is needed.
6. Validate determinism by running the tape at least twice.
7. Ensure no secrets, host-specific paths, or unstable prompts appear in recordings.

## Quality Checklist

- [ ] Every command completion uses deterministic wait patterns
- [ ] No plaintext secrets or tokens are shown
- [ ] Output assets are reproducible across runs
- [ ] Script remains readable and maintainable
- [ ] Any CI or Makefile integration changes are documented

## Output Format

Return edited files, a concise determinism summary, and any follow-up actions needed for CI or release integration.
