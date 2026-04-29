---
description: 'Design-phase security review rules for C public API headers, derived from CWE-701 design weakness taxonomy, CWE-1400 categorization, and CERT-C design rules (CWE-1154; CWE-734 is the retired 2008 edition)'
applyTo: 'include/**/*.h'
---

# Design-Weakness Review: C Public API Headers

These rules apply when creating or editing any C header file under `include/`.
CWE-701 groups weaknesses introduced at design time; the rules below translate
relevant CWE categories and CERT-C design rules into concrete, enforceable checks.
CWE-1154 is the current CERT-C view; CWE-734 is its legacy 2008 predecessor
and must not be cited as authoritative.

---

## 1 — Resource Lifetime and Ownership (CWE-664; CERT-C MEM-series)

- Every function that returns a heap pointer **must** document in its declaration
  comment whether the caller or the library owns the result and which function
  frees it (e.g., `free()` or a library-specific destructor).
- Every function that accepts a pointer parameter **must** document whether it
  takes ownership, borrows the pointer, or transfers ownership to the callee.
- Functions that may return `NULL` on allocation failure **must** state that
  explicitly; callers must not be required to infer it from convention.
- Paired alloc/free functions **must** be declared in the same header and
  cross-referenced in both declaration comments.

## 2 — Trust Boundary Documentation (CWE-693; CWE-345)

- Every parameter that originates from an untrusted caller (user input, network,
  or file) **must** be labelled with a comment identifying it as untrusted and
  stating which validation the function performs vs. which the caller must
  perform before the call.
- Size and length parameters **must** document valid ranges and the behaviour
  when an out-of-range value is passed (truncate, return error, or abort).
- Do not design APIs that silently accept invalid sizes and clip internally
  without returning an error; the caller must know when a constraint was violated.

## 3 — Error Return Consistency (CWE-701; CERT-C ERR-series)

- Every fallible public function **must** return a type capable of representing
  all failure modes. `void` return is permitted only for functions that are
  guaranteed by design to be infallible.
- The return-value convention (0 = success, negative = error) **must** be
  consistent across the entire public API; do not mix conventions in the same
  header.
- Every non-zero error value **must** be documented in the declaration comment
  with a one-line description of the condition it represents.
- Do not expose errno-style global error state as the sole error channel;
  functions must return an error value directly.

## 4 — Access Control at the Interface (CWE-284)

- Do not expose internal state structs or fields in public headers unless the
  caller has a documented, intentional reason to read or modify them.
- Opaque pointer types (`typedef struct foo_s foo_t;`) are preferred for any
  object whose internal layout is an implementation detail.
- Functions that grant elevated capabilities (key operations, buffer access)
  **must** document the precondition under which the caller is permitted to
  invoke them.

## 5 — Sensitive Data Isolation (CWE-226; CWE-693)

- Sensitive parameters (key material, plaintext, tokens) **must** be separated
  from non-sensitive parameters in the function signature where practical; avoid
  mixing sensitive and diagnostic data in the same parameter.
- The zeroing contract **must** be documented: state which party is responsible
  for wiping sensitive buffers (caller or library) and under which conditions
  (error path, success path, or both).
- Do not design APIs where sensitive data is returned via output parameters that
  the caller might silently ignore; prefer explicit return values that force
  acknowledgement of the sensitive result.

## 6 — Concurrency Model (CWE-362)

- Every header that exposes module-level state **must** include a comment
  stating the thread-safety guarantee: not thread-safe, thread-safe, or re-entrant.
- Functions that modify shared static state **must** not claim thread-safety
  without documenting the synchronization mechanism.
- If no concurrency model applies, state "single-threaded use only" explicitly
  to prevent future misuse.

## 7 — Comparison and Equality Semantics (CWE-697)

- Functions that compare sensitive data (keys, MACs, hashes) **must** state in
  the declaration comment that comparison is constant-time and cite the
  underlying mechanism (e.g., `sodium_memcmp`).
- Do not design APIs where the caller is expected to compare returned values
  with `==` or `memcmp` for security-sensitive equality checks; provide a
  dedicated constant-time comparison helper or document the constraint.

## 8 — General Design Hygiene

- Keep the public API surface minimal: do not expose helper functions that are
  only needed internally.
- Every macro that performs computation **must** parenthesize all arguments and
  the entire expansion to prevent operator-precedence surprises (CERT-C PRE01-C).
- Integer parameters used as sizes or indices **must** use `size_t`; do not use
  signed integer types for values that can never be negative.
- Avoid `#define` constants that shadow or conflict with standard library names.
