---
description: 'Guards against low-level libsodium Ed25519 point-validation and subgroup misuse (CVE-2025-69277)'
applyTo: '**/src/**/*.c,**/include/**/*.h'
---

# Libsodium Low-Level Vulnerability Guards

Apply these guardrails to any C source or header that calls low-level libsodium
Ed25519 or group-arithmetic primitives. They are complementary to
`libsodium-patterns.instructions.md`, which covers the primary sealed-box
workflow.

## CVE-2025-69277 scope

**Affected:** code that passes externally supplied bytes to low-level Ed25519
group functions without a torsion check (`crypto_core_ed25519_*`,
`crypto_scalarmult_ed25519`, `crypto_sign_ed25519_sk_to_curve25519`).

**Not affected:**
- `crypto_box_seal` / sealed-box callers — uses X25519 internally.
- `crypto_sign_open` / `crypto_sign_ed25519_open` — high-level API validates
  internally.
- `crypto_secretbox_*`, `crypto_aead_*` — symmetric primitives, unaffected.

Do **not** treat CVE-2025-69277 as a sealed-box regression. The `salt_seal_base64`
code path in this repository is **not vulnerable**.

## Rule 1 — Prefer high-level APIs

Use high-level APIs unless the protocol requires custom group arithmetic:

| Task | Preferred API |
|---|---|
| Encrypt to recipient public key | `crypto_box_seal` |
| Sign / verify a message | `crypto_sign` / `crypto_sign_open` |
| Symmetric authenticated encryption | `crypto_secretbox_easy` / `crypto_aead_*` |

Reach for `crypto_core_ed25519_*` or `crypto_scalarmult_ed25519` only when the
protocol genuinely requires low-level group operations.

## Rule 2 — Validate externally supplied points before use

`crypto_core_ed25519_is_valid_point` checks curve membership and canonical
encoding, but does **not** reject low-order (torsion) components. Passing its
return value alone is insufficient when input points arrive from an untrusted
source.

**Minimum safe sequence for an externally supplied point:**

```c
/* 1. Reject off-curve or non-canonical encodings */
if (crypto_core_ed25519_is_valid_point(p) != 1) {
    return SALT_ERR_INVALID_POINT;
}

/* 2. Reject torsion components — multiply by group order l and expect identity.
 *    A torsion-free point yields the neutral element; a low-order point does not. */
unsigned char check[crypto_core_ed25519_BYTES];
if (crypto_scalarmult_ed25519_noclamp(check, group_order_l, p) != 0 ||
    sodium_memcmp(check, ed25519_neutral, sizeof(check)) != 0) {
    return SALT_ERR_INVALID_POINT;
}
```

Where `group_order_l` is the little-endian encoding of the Ed25519 group order
`l` and `ed25519_neutral` is the encoding of the neutral element `(0, 1)`.

## Rule 3 — Prefer Ristretto255 for new protocol implementations

When designing a new protocol that requires externally supplied group elements,
use **Ristretto255** instead of raw Ed25519 points:

- The Ristretto encoding enforces prime-order group membership at decode time.
- No separate torsion check is needed after a successful decode.
- API: `crypto_core_ristretto255_*`, `crypto_scalarmult_ristretto255_*`.

```c
/* Good: Ristretto255 decode rejects any non-prime-order encoding */
unsigned char rp[crypto_core_ristretto255_BYTES];
if (crypto_core_ristretto255_is_valid_point(rp) != 1) {
    return SALT_ERR_INVALID_POINT;
}
/* rp is guaranteed to be in the prime-order subgroup */
crypto_scalarmult_ristretto255(out, scalar, rp);
```

## Rule 4 — Annotate every low-level Ed25519 call site

Every call to `crypto_core_ed25519_*` or `crypto_scalarmult_ed25519` must
carry an inline comment that states why the point is safe to use:

```c
/* SAFE: keypair generated internally by crypto_sign_keypair — no external input */
crypto_scalarmult_ed25519_base(pk, sk);

/* SAFE: torsion check passed above (see is_valid_point + group-order multiply) */
crypto_scalarmult_ed25519(shared, my_scalar, peer_point);
```

Call sites that lack such a comment are considered incomplete and must be
flagged during code review.

## Rule 5 — Do not weaken existing validation on the sealed-box path

The sealed-box path validates that decoded public key length equals
`crypto_box_PUBLICKEYBYTES` (32 bytes, a Curve25519 key). Do not:

- Accept raw Ed25519 public keys (32 bytes but different curve domain) on this
  path without explicit conversion via `crypto_sign_ed25519_pk_to_curve25519`.
- Skip the length check on the basis of a prior is_valid_point call (they test
  different things).

## Bad patterns (flag and reject)

```c
/* BAD: is_valid_point alone does not exclude low-order torsion subgroup */
if (crypto_core_ed25519_is_valid_point(external_point)) {
    crypto_scalarmult_ed25519(out, scalar, external_point); /* CVE-2025-69277 */
}

/* BAD: raw Ed25519 point passed directly to crypto_box_seal without conversion */
crypto_box_seal(ct, pt, pt_len, raw_ed25519_pk);

/* BAD: silent pass — no validation at all before scalar multiplication */
crypto_scalarmult_ed25519_noclamp(out, scalar, wire_point);
```

## Validation commands

```sh
# Scan for low-level Ed25519 call sites that may need audit
grep -rn 'crypto_core_ed25519_\|crypto_scalarmult_ed25519\|crypto_sign_ed25519_.*_to_' src/ include/

# Confirm no sealed-box callers were modified to accept Ed25519 points
grep -n 'crypto_box_seal' src/ include/
```
