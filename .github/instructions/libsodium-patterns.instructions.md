---
description: 'Conventions for libsodium usage in C source files'
applyTo: '**/src/**/*.c,**/include/**/*.h'
---

# libsodium Patterns

Use these patterns when implementing sealed-box encryption flows and any
other libsodium-backed operation in this repository.

Authoritative references (re-read when changing crypto or memory paths):

- Internals: <https://libsodium.gitbook.io/doc/internals>
- Secure memory: <https://libsodium.gitbook.io/doc/memory_management>
- Helpers: <https://libsodium.gitbook.io/doc/helpers>
- Padding: <https://libsodium.gitbook.io/doc/padding>
- Generating random data: <https://libsodium.gitbook.io/doc/generating_random_data>

## Initialization and threading

- Call `sodium_init()` once before any crypto primitive, base64 helper, or
  guarded-heap allocation. The CLI does this lazily inside
  `salt_seal_base64`; do not duplicate it elsewhere.
- libsodium is documented as thread-safe on POSIX. Treat the salt CLI's
  process-scoped state (signal handlers, `sodium_mlock` reservations,
  guarded heap pages) as the actual concurrency boundary.

## Required sealed-box call flow

1. `sodium_init()` once before any crypto operation.
2. Decode the public key with `sodium_base642bin`. Verify both the libsodium
   return code and the decoded length equal `crypto_box_PUBLICKEYBYTES`.
3. Allocate a ciphertext buffer of exactly
   `crypto_box_SEALBYTES + plaintext_len` (validate the addition does not
   overflow `SIZE_MAX`).
4. Call `crypto_box_seal` and check the return code.
5. Encode with `sodium_bin2base64` into a caller-sized buffer; size it via
   `sodium_base64_ENCODED_LEN` (the result already includes the trailing
   `\0`).

## Good pattern: strict decode and seal

```c
unsigned char public_key[crypto_box_PUBLICKEYBYTES];
size_t decoded = 0;

if (sodium_init() < 0) {
    return SALT_ERR_CRYPTO;
}

if (sodium_base642bin(public_key,
                      sizeof(public_key),
                      key_b64,
                      key_b64_len,
                      NULL,
                      &decoded,
                      NULL,
                      sodium_base64_VARIANT_ORIGINAL) != 0 ||
    decoded != crypto_box_PUBLICKEYBYTES) {
    return SALT_ERR_INPUT;
}

if (crypto_box_seal(ciphertext, plaintext, plaintext_len, public_key) != 0) {
    return SALT_ERR_CRYPTO;
}
```

## Bad pattern: permissive decode and unchecked crypto

```c
sodium_base642bin(public_key, sizeof(public_key), key_b64, key_b64_len,
                  NULL, &decoded, NULL, sodium_base64_VARIANT_ORIGINAL);
crypto_box_seal(ciphertext, plaintext, plaintext_len, public_key);
sodium_bin2base64(out, out_len, ciphertext, ciphertext_len,
                  sodium_base64_VARIANT_ORIGINAL);
```

## Sensitive memory hygiene

The CLI runs inside GitHub Actions runners (public or private) where
plaintext secrets and key material must never leak via swap, core dumps,
hibernation, debuggers, or `/proc/<pid>/cmdline`. Apply these rules to any
buffer that can transit plaintext, decoded keys, or related intermediates:

- Wipe with `sodium_memzero(ptr, len)` rather than `memset` — `memset` may
  be elided by the optimizer.
- Lock pages with `sodium_mlock(ptr, len)` immediately after allocation (or
  just after declaring fixed-size stack buffers) to keep them out of swap.
- Release with `sodium_munlock(ptr, len)`. **`sodium_munlock` already zeros
  the region before unlocking** — do not pair it with a redundant
  `sodium_memzero` of the same range.
- Treat `sodium_mlock` failures as non-fatal. They commonly fail under low
  `RLIMIT_MEMLOCK` (containers, sanitizer/valgrind runs). Cast the return
  value to `(void)` and continue; the wipe-on-release contract is what we
  rely on for correctness.
- After a logical batch of secret-touching work completes, call
  `sodium_stackzero(len)` once before returning to wipe stack space used by
  callees that have already returned. Pick `len` large enough to cover the
  deepest call frame the batch produced (a few KB is typical).
- Never log, print, or include in `error[]` strings any plaintext bytes,
  decoded key bytes, raw pointer values, or partial ciphertext.

### Guarded heap allocations (`sodium_malloc`)

`sodium_malloc` / `sodium_allocarray` / `sodium_free` provide canary-protected,
mlock'd, zero-on-free pages with a guard page on each side. Use them when
**all** of the following hold:

- The allocation is for a small, single-purpose secret region (decoded keys,
  short-lived plaintext copies).
- Allocation frequency is low; each call costs 3–4 extra pages of virtual
  memory.
- Alignment requirements are simple — `sodium_malloc` returns an address at
  the end of a page boundary, so packed or variable-length structures must
  round their requested size up to the alignment they need.

Do **not** use `sodium_malloc` for:

- The base64 output buffer (it is not a secret; it is the value the caller
  is about to print).
- Per-iteration buffers in benchmarks or fuzzers (page churn drowns the
  measurement and hides regressions).
- Buffers larger than a few KB where ordinary `malloc` + `sodium_mlock` +
  `sodium_munlock` already gives equivalent protection at lower overhead.

When `sodium_malloc` is used, free with `sodium_free` (which checks the
canary and zeros the region) — **never** mix it with `free`.

## Constant-time and helper functions

- Compare any secret-derived bytes (authentication tags, decoded keys,
  per-message MACs) with `sodium_memcmp` or `crypto_verify_*`. Plain
  `memcmp` leaks information through timing.
- For non-secret data (JSON field names, CLI option strings, format
  selectors), use `strcmp`/`memcmp` — `sodium_memcmp` is not a
  general-purpose lexicographic comparator and adds no security value here.
- Use `sodium_bin2base64` / `sodium_base642bin` for all base64 conversion
  with an explicit `sodium_base64_VARIANT_*`. Size output buffers via
  `sodium_base64_ENCODED_LEN(bin_len, variant)`; the macro already accounts
  for the trailing NUL.
- Use `sodium_bin2hex` / `sodium_hex2bin` for hex; both run in constant
  time for a given length.

## Padding (`sodium_pad` / `sodium_unpad`)

We deliberately do **not** use libsodium padding in this CLI:

- Sealed-box ciphertext length is `plaintext_len + crypto_box_SEALBYTES`,
  so the CLI's length-disclosure surface is exactly the user's plaintext
  length — padding to a block boundary would not hide that from anyone who
  observes the CLI invocation.
- The downstream consumer (the GitHub Secrets API) does not pad, so adding
  padding here would only widen the wire format and confuse decryption.

If a future operation transmits unbounded-length material over a channel
where length leakage matters, apply `sodium_pad` *before* encryption and
`sodium_unpad` *after* decryption, using the ISO/IEC 7816-4 algorithm
libsodium implements.

## Random data (`randombytes_*`)

We deliberately do **not** call `randombytes_buf` directly. `crypto_box_seal`
generates its own ephemeral keypair internally using libsodium's CSPRNG, so
the CLI never needs to supply randomness. If a future code path needs
randomness:

- Use `randombytes_buf(buf, size)` for raw bytes and `randombytes_uniform`
  for an unbiased integer in `[0, upper_bound)`.
- Do not call `randombytes_random() % n` — it is biased.
- Do not call `randombytes_close()` or `randombytes_stir()` from the CLI;
  the defaults are safe across `fork()` with the bundled PRG.

## Error-text rules

- Report deterministic CLI-safe errors such as
  `invalid public key encoding or length`.
- Do not print raw key material, decoded buffers, plaintext bytes, internal
  pointer values, or `errno` strings — libsodium does not set `errno`, so
  `strerror(errno)` after a libsodium call is meaningless and noisy.

## Low-level Ed25519 API guardrails (CVE-2025-69277)

This section applies only if code is added that calls low-level Ed25519
primitives. The existing `crypto_box_seal` sealed-box flow is **not affected**.

### Affected vs unaffected usage

| Unaffected — no action required | Affected — requires explicit review |
|---|---|
| `crypto_box_seal` / `crypto_box_open_easy` | `crypto_core_ed25519_is_valid_point` on external input |
| `crypto_sign_open` / `crypto_sign_ed25519_open` | `crypto_scalarmult_ed25519` on externally supplied points |
| `crypto_secretbox_*`, `crypto_aead_*` | `crypto_sign_ed25519_pk_to_curve25519` feeding back into crypto ops |
| Points generated internally by `crypto_sign_keypair` | Any scenario where untrusted bytes are treated as a group element |

### Guardrail: two-step point validation

`crypto_core_ed25519_is_valid_point` confirms curve membership and canonical
encoding but **does not reject torsion (low-order) components**. When an
externally supplied point is used in scalar multiplication, also verify
torsion-freeness by multiplying the point by the group order and checking the
result equals the neutral element. See
`.github/instructions/libsodium-lowlevel-vuln-guards.instructions.md` for the
full safe call sequence and code examples.

### Guardrail: prefer Ristretto255 for new designs

Any new protocol that involves externally supplied group elements should use
`crypto_scalarmult_ristretto255_*`. Ristretto255 encoding enforces prime-order
group membership at decode time, eliminating the need for a separate torsion
check.

### Annotation requirement

Every call to `crypto_core_ed25519_*` or `crypto_scalarmult_ed25519` must carry
an inline comment explaining why the input point is safe (e.g., internally
generated, Ristretto used, or torsion check applied).

