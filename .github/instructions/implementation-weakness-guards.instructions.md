---
description: 'CWE-702/CWE-658 implementation weakness guards for C source files and headers'
applyTo: '**/src/**/*.c,**/include/**/*.h'
---

# Implementation Weakness Guards (CWE-702 / CWE-658)

Apply these rules when writing or reviewing any C source or header file in this
repository. The rule set is grounded in CWE-702 (implementation weaknesses),
CWE-658 (C language weakness space), and CERT C guidance per CWE-1154 (current)
and CWE-734 (legacy/obsolete).

These rules are **additive** with `c-memory-safety.instructions.md`. When both
files apply to the same change, follow the memory-safety patterns first and use
these CWE-tagged guards as a complementary checklist.

## 1. Bounds Checks (CWE-119, CWE-125, CWE-787 -- ARR38-C, STR31-C)

- Validate destination buffer capacity against the source data length **before**
  any `memcpy`, `memmove`, `sodium_base642bin`, or structure-field write.
- When length is derived from external input, reject it before use:
  ```c
  if (src_len > dst_capacity) { return SALT_ERR_INPUT; }
  memcpy(dst, src, src_len);
  ```
- Never rely on implicit null-termination from a prior write; confirm space for
  the terminator explicitly before adding it.
- For off-by-one safety, use `dst_capacity - 1` as the upper copy bound when
  reserving the final byte for `\0`.

## 2. Integer Overflow (CWE-190, CWE-680 -- INT30-C, INT32-C)

- Guard all addition or multiplication on user-controlled values used as buffer
  sizes or array indices:
  ```c
  if (plaintext_len > SIZE_MAX - crypto_box_SEALBYTES) { return SALT_ERR_INPUT; }
  size_t need = plaintext_len + crypto_box_SEALBYTES;
  ```
- Use `size_t` for buffer-size arithmetic; never cast a `size_t` result to a
  narrower type without an explicit range check first.
- Guard unsigned subtraction: if `a < b` then `a - b` wraps to a large value.
  Always check `if (a < b) { return SALT_ERR_INPUT; }` before the subtraction.
- Reject zero-length inputs that could produce a zero-size allocation followed
  by a write.

## 3. Pointer Lifetime and Null Checks (CWE-476, CWE-416 -- EXP34-C, MEM30-C)

- Check every `malloc` / `calloc` / `sodium_malloc` return for `NULL`
  immediately after the call:
  ```c
  buf = malloc(need);
  if (buf == NULL) { return SALT_ERR_INTERNAL; }
  ```
- Zero pointers after `free` or `sodium_free` to prevent use-after-free on
  re-entry or in error-path branches:
  ```c
  free(buf);
  buf = NULL;
  ```
- Do not return a raw pointer to a caller without documenting the null-means-
  error contract in the function header comment.
- Do not pass a pointer to a function that may free it and then use it again
  after the call.

## 4. Return-Value Checks (CWE-252 -- ERR33-C)

- Every function that can fail **must** have its return value checked at the
  call site. libsodium functions do not set `errno`; treat a non-zero return as
  fatal:
  ```c
  if (crypto_box_seal(c, m, mlen, pk) != 0) { return SALT_ERR_CRYPTO; }
  ```
- `(void)` cast is acceptable only for explicitly documented best-effort calls
  such as `sodium_mlock`, where failure is non-fatal and the wipe-on-release
  contract is the correctness guarantee.
- System calls in production `main()` that are best-effort (e.g., `setrlimit`,
  `prctl`) must carry an inline comment explaining why the return is ignored.

## 5. Sensitive Buffer Handling (CWE-14, CWE-312 -- MEM03-C)

- Any buffer that may hold plaintext, decoded key bytes, or crypto intermediates
  must be wiped on **every** exit path -- success and each error label:
  ```c
  /* error path */
  sodium_munlock(secret_buf, secret_len);   /* also zeroes the region */
  free(secret_buf);
  return SALT_ERR_CRYPTO;
  ```
- Do not include plaintext bytes, key bytes, or decoded values in error messages,
  `fprintf(stderr, ...)`, or any log output.
- Prefer `sodium_munlock` over a standalone `sodium_memzero` for heap secrets
  (unlock already zeroes); for stack-only secrets, use `sodium_memzero` directly.

## Decision Matrix

| Input kind | Required guard | CWE | CERT C |
|---|---|---|---|
| User-controlled length in allocation | Check `> SIZE_MAX - overhead` | CWE-190 | INT30-C |
| User-controlled length as copy bound | Check `<= dest_capacity` | CWE-119 | ARR38-C |
| `malloc` / `calloc` return value | Null check before first use | CWE-476 | MEM32-C |
| Pointer after `free` | Zero the pointer | CWE-416 | MEM30-C |
| Any fallible function return | Check immediately at call site | CWE-252 | ERR33-C |
| Buffer with secret content | Wipe on every exit path | CWE-14 | MEM03-C |
