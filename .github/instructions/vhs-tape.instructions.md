---
description: 'Conventions for writing deterministic VHS tape demos and associated Makefile and CI integration'
applyTo: '**/*.tape,**/.github/workflows/*.yml,**/.github/workflows/*.yaml,**/Makefile'
---

# VHS Tape Instructions

Use these rules when creating or updating VHS tape files.

## Determinism

- Prefer `Wait@<duration> /regex/` to synchronize command completion.
- Do not use open-ended `Wait` without timeout and match conditions.
- Keep shell prompts and command output stable between runs.
- Avoid commands that depend on network state unless explicitly mocked.

## Presentation

- Use a readable terminal size and consistent margins.
- Keep pacing concise and intentional.
- Capture only story-relevant screenshots.
- Generate only requested output formats.

## Security and Hygiene

- Never record secrets, private keys, tokens, or internal hostnames.
- Do not leak local absolute paths unless required for the demo.

## Makefile and CI Integration

When adding Makefile or GitHub Actions integration for VHS tape generation:
- Keep it deterministic and optional by default.
- Use explicit dependency checks before running VHS steps.
- Follow `dependency-pinning.instructions.md` for VHS version pinning.
- Follow `ci.instructions.md` for workflow structure and gates.
