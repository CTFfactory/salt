---
name: audit-c-memory-safety
description: 'Audit C code for allocation safety, bounds checks, sensitive data handling, and sanitizer compatibility'
argument-hint: '[path or module]'
---

# Audit C Memory Safety

Use this skill to audit source files for memory-safety risks.

## Input checklist

- Target path or module under `src/` and related headers in `include/`.
- Whether the change touches crypto buffers, allocation paths, or parser logic.
- Which validation commands are expected (`make test-sanitize`, `make coverage-check`).

## Steps

1. Check length validation before decode/copy/allocate operations.
2. Check return-value handling for allocations and crypto calls.
3. Check sensitive buffer zeroing strategy where applicable.
4. Check for undefined behavior patterns (signed overflow, invalid pointer usage).
5. Confirm sanitizer targets cover changed code paths.

## Example workflow

1. Inspect modified files with focus on untrusted-length flows.
2. Trace each allocation to ensure error-path cleanup and wipe policy are explicit.
3. Verify every libsodium and allocation call has immediate return checking.
4. Run `make test-sanitize` and map failures back to specific call sites.
5. Classify findings and propose minimal remediation patches.

## Sample output format

```text
Severity: HIGH
File: src/salt.c
Finding: ciphertext allocation size derived from unvalidated arithmetic.
Evidence: potential overflow when plaintext_len approaches SIZE_MAX.
Fix: guard arithmetic, reject impossible lengths, add regression test.

Severity: MEDIUM
File: src/main.c
Finding: error path frees sensitive buffer without wipe.
Evidence: cleanup label misses sodium_memzero before free.
Fix: wipe buffer length before release and assert via test hook.
```

## CWE / CERT C Mapping (CWE-658 / CWE-702)

Tag each finding with a CWE ID and CERT C rule to support downstream tracking
and cross-referencing with `audit-implementation-weaknesses`:

| Finding Category | CWE IDs | CERT C Rule |
|---|---|---|
| Length not validated before copy/decode | CWE-119, CWE-125, CWE-787 | ARR38-C, STR31-C |
| Integer overflow in size arithmetic | CWE-190, CWE-680 | INT30-C, INT32-C |
| Allocation return not null-checked | CWE-476 | MEM32-C |
| Pointer dereferenced without null check | CWE-476 | EXP34-C |
| Use-after-free risk | CWE-416 | MEM30-C |
| Return value of fallible call ignored | CWE-252 | ERR33-C |
| Sensitive buffer not wiped on error path | CWE-14, CWE-312 | MEM03-C |

These IDs are grounded in CWE-702 (implementation weaknesses) and CWE-658 (C
language weakness space). For a structured per-weakness audit workflow, invoke
`.github/skills/audit-implementation-weaknesses/SKILL.md` in parallel or as a
follow-up pass.

## Output

Report findings by severity with CWE ID, CERT C rule, and concrete remediation
actions.
