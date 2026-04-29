---
name: setup-sealed-box-scaffolding
description: 'Scaffold C source/header structure for libsodium sealed-box encryption and base64 output flow'
argument-hint: '[optional paths]'
---

# Setup Sealed Box Scaffolding

Use this skill to scaffold implementation files for the sealed-box workflow.

## Steps

1. Create or align headers under `include/` for public interfaces.
2. Create implementation units under `src/` for:
   - input validation
   - key decode helpers
   - seal + base64 pipeline
3. Keep CLI orchestration isolated in `src/main.c`.
4. Add deterministic error code mapping and message strategy.
5. Ensure all libsodium calls include return-value checks.
