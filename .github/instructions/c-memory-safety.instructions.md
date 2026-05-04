---
description: 'Memory-safety conventions for C implementation files'
applyTo: '**/src/**/*.c'
---

# C Memory Safety

Use these patterns when editing C implementation files. The salt CLI is
designed to run on shared and untrusted GitHub Actions runners; treat any
plaintext message and decoded key material as adversarial-grade secrets that
must not leak via swap, core dumps, hibernation, debuggers, `ps`, or
`/proc/<pid>/cmdline`.

## Required checks

- Validate all untrusted lengths before allocation and copy operations.
  Reject sizes that would overflow `SIZE_MAX` after sealed-box overhead is
  added.
- Use bounded operations and explicit size calculations. Prefer
  `salt_required_base64_output_len` and the `crypto_box_*` constants over
  hand-rolled arithmetic.
- Check every allocation and every libsodium return value. libsodium does
  not set `errno`; do not branch on it after a libsodium call.
- Keep sanitizer compatibility (`-fsanitize=address,undefined`) and treat
  sanitizer failures as blocking.

## Sensitive-buffer lifecycle

For any buffer that may transit plaintext, decoded key bytes, or other
secret-derived intermediates:

1. Allocate (heap `malloc`/`calloc`, fixed-size stack array, or
   `sodium_malloc` when guarded pages are warranted).
2. Immediately `(void)sodium_mlock(ptr, len)` to keep pages out of swap.
   `sodium_mlock` failure is non-fatal (low `RLIMIT_MEMLOCK` is common in
   containers and sanitizer/valgrind runs); the wipe-on-release contract is
   what guarantees correctness.
3. On every exit path (success and failure):
   - Heap buffers: `sodium_munlock(ptr, len)` then `free(ptr)`.
   - Stack buffers: `sodium_munlock(ptr, sizeof(buf))`.
   - `sodium_munlock` already zeroes the region — do **not** add a
     redundant `sodium_memzero` of the same range.
4. Before the function `return`s, call `sodium_stackzero(len)` once to wipe
   stack space used by callees that have already returned. Pick `len`
   slightly larger than the deepest callee frame (a few KB is typical).

## Good pattern: bounded allocation and cleanup

```c
size_t need = salt_required_base64_output_len(msg_len);
if (need == 0 || need > SIZE_MAX - 1) {
    return SALT_ERR_INPUT;
}

char *out = malloc(need + 1);
if (out == NULL) {
    return SALT_ERR_INTERNAL;
}
(void)sodium_mlock(out, need + 1);

if (salt_seal_base64(msg, msg_len, key, out, need + 1, NULL,
                     err, err_size) != SALT_OK) {
    (void)sodium_munlock(out, need + 1);   /* zeroes before unlock */
    free(out);
    return SALT_ERR_CRYPTO;
}

(void)sodium_munlock(out, need + 1);
free(out);
sodium_stackzero(2048);
return SALT_OK;
```

## Bad pattern: unchecked sizes and missing return checks

```c
char *out = malloc(msg_len * 4);          /* unchecked multiplication */
salt_seal_base64(msg, msg_len, key, out, msg_len * 4); /* ignored rc */
printf("%s\n", out);                       /* may read invalid output */
free(out);                                 /* no sensitive wipe */
```

## Process-level hardening (production main only)

The production `main()` disables core dumps and ptrace attach so transient
secrets cannot be dumped to disk or read by an attaching debugger that
shares the UID:

- `setrlimit(RLIMIT_CORE, &(struct rlimit){0, 0})`.
- `prctl(PR_SET_DUMPABLE, 0, 0, 0, 0)` on Linux.

Both calls are best-effort and intentionally not invoked from
`SALT_NO_MAIN` test/fuzz/library builds. Library embedders are responsible
for their own equivalent hardening.

Argv-borne plaintext is also scrubbed in place by the production binary
(`sodium_memzero` over the original argv slot) after copying to an mlock'd
heap buffer; this is restricted to the production `main()` path because
test harnesses may pass argv slots backed by read-only string literals.

## Decision rules

- If a length is derived from external input, validate before arithmetic.
- If a buffer may contain plaintext, key bytes, or decoded ciphertext, lock
  it on allocation and unlock-zero it on every release.
- If an operation can fail, check and branch on the return code immediately.
- If a function handles a batch of secret-touching work, end with one
  `sodium_stackzero` call before the final return.
- Never include plaintext bytes, decoded key bytes, or pointer values in
  any `error[]` string, `fprintf(stderr, ...)`, or log line.

## CWE / CERT C Quick Reference (CWE-658 / CWE-702)

When reviewing `src/` changes alongside
`implementation-weakness-guards.instructions.md`, use these CWE IDs and CERT C
rule labels to annotate findings for downstream CVE triage:

| Pattern | CWE | CERT C Rule |
|---|---|---|
| Length from input used in allocation without overflow guard | CWE-190, CWE-191, CWE-680 | INT30-C |
| Destination capacity not validated before copy | CWE-119, CWE-120, CWE-125 | ARR38-C |
| `malloc` / `calloc` / buffer capacity improperly calculated | CWE-131 | MEM35-C |
| `malloc` / `calloc` return not null-checked | CWE-476 | MEM32-C |
| Pointer dereferenced without null check | CWE-476 | EXP34-C |
| Pointer used after `free` | CWE-416 | MEM30-C |
| Use of uninitialized pointer | CWE-824 | EXP33-C |
| Return value of fallible call ignored | CWE-252 | ERR33-C |
| Sensitive buffer not zeroed on error exit path | CWE-14, CWE-312 | MEM03-C |

This table reflects CWE-702 (implementation weaknesses) and CWE-658 (C language
weakness space including classic buffers CWE-119/120, integer faults CWE-190/191, and NULL derefs CWE-476), strictly aligned with the CERT C guidance in CWE-1154 (Weaknesses Addressed by the SEI CERT C Coding Standard).

