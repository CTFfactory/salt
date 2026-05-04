# Libsodium Usage Inventory

This document inventories **upstream libsodium objects** used by this repository.
It includes official libsodium functions and constants/macros, and excludes
project-local wrappers/hooks (for example `salt_*_fn`, `*_impl`, `*_hook`) and
non-libsodium variables with similar names.

## Section A: Production usage (`src/`, `include/`)

| Object | Type | Libsodium object description | How salt uses it | Representative references |
|---|---|---|---|---|
| `crypto_box_seal` | Function | Anonymous public-key "sealed box" encryption using recipient public key. | Core encryption primitive in `salt_seal_base64()` for ciphertext generation from plaintext and recipient key. | `src/salt.c`, `include/salt.h` |
| `sodium_base642bin` | Function | Decodes Base64 text to binary bytes using a selected variant. | Decodes incoming base64 public keys and enforces exact key length. | `src/salt.c` |
| `sodium_bin2base64` | Function | Encodes binary bytes to Base64 text with a selected variant. | Encodes sealed-box ciphertext to the CLI/library base64 output. | `src/salt.c` |
| `sodium_init` | Function | Initializes libsodium internals before cryptographic/helper calls. | Called before crypto/helper operations in the main seal path; initialization failures are surfaced as crypto errors. | `src/salt.c`, `include/salt.h` |
| `sodium_memzero` | Function | Secure memory wipe API intended to avoid compiler-elided clearing. | Used to scrub sensitive plaintext/key-adjacent buffers in CLI input and seal cleanup paths. | `src/cli/input.c`, `src/salt.c` |
| `sodium_mlock` | Function | Best-effort memory page locking to reduce swap exposure of sensitive data. | Applied to sensitive buffers (keys/plaintext/output/error/ciphertext) as part of secret-handling hardening. | `src/salt.c`, `src/cli/input.c`, `src/cli/prepare.c`, `src/cli/state.c`, `src/cli/state.h` |
| `sodium_munlock` | Function | Unlocks `mlock`'d memory and zeros it before release. | Used on cleanup paths to scrub and release sensitive regions deterministically. | `src/salt.c`, `src/main.c`, `src/cli/input.c`, `src/cli/state.c`, `src/cli/state.h` |
| `sodium_stackzero` | Function | Wipes a caller-selected amount of stack memory. | Used at end of critical paths to reduce residual stack exposure of sensitive intermediate data. | `src/salt.c`, `src/main.c` |
| `crypto_box_PUBLICKEYBYTES` | Constant/Macro | Byte length for Curve25519 public keys used by `crypto_box`. | Used for fixed-size key buffers and strict decoded-key length validation. | `src/salt.c`, `include/salt.h` |
| `crypto_box_SEALBYTES` | Constant/Macro | Sealed-box overhead added to plaintext length. | Used for ciphertext and output-size computations, including overflow-safe sizing logic. | `src/salt.c`, `include/salt.h` |
| `sodium_base64_ENCODED_LEN` | Constant/Macro (function-like) | Computes required Base64 output buffer length for an input length/variant. | Computes required output size for base64 encoded sealed-box ciphertext. | `src/salt.c` |
| `sodium_base64_VARIANT_ORIGINAL` | Constant/Macro | Standard libsodium Base64 variant (with padding). | Production encode/decode variant for key/ciphertext paths and output sizing. | `src/salt.c` |
| `sodium_base64_VARIANT_ORIGINAL_NO_PADDING` | Constant/Macro | Standard Base64 variant without `=` padding. | Accepted as fallback when decoding incoming public keys. | `src/salt.c` |

## Section B: Support usage (`tests/`, `fuzz/`, `bench/`, helper C under `scripts/`)

| Object | Type | Libsodium object description | How salt uses it | Representative references |
|---|---|---|---|---|
| `crypto_box_keypair` | Function | Generates random `crypto_box` public/secret keypair. | Used in tests/bench/helper code to generate keypairs for roundtrip and performance exercises. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `crypto_box_seal` | Function | Anonymous public-key sealed-box encryption. | Used in support code as an oracle/reference behavior alongside salt wrapper flows and fuzz setup. | `tests/test_salt.c`, `fuzz/fuzz_boundary.c` |
| `crypto_box_seal_open` | Function | Decrypts sealed-box ciphertext using recipient keypair. | Used for roundtrip verification of ciphertext produced by salt or harness-generated paths. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_roundtrip.c`, `scripts/verify-roundtrip-helper.c` |
| `crypto_box_seed_keypair` | Function | Deterministically derives keypair from fixed seed bytes. | Used by fuzz helpers to make deterministic key material generation reproducible. | `fuzz/fuzz_common.h` |
| `sodium_base642bin` | Function | Decodes Base64 text to binary bytes. | Used in tests/fuzz/helper code to decode keys/ciphertext and validate parse/decode behavior. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_roundtrip.c`, `scripts/verify-roundtrip-helper.c` |
| `sodium_bin2base64` | Function | Encodes binary bytes to Base64 text. | Used to prepare base64 fixtures and encoded keys/ciphertext in tests/fuzz/bench/helper programs. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_boundary.c`, `fuzz/fuzz_cli.c`, `fuzz/fuzz_roundtrip.c`, `bench/bench_base64.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `sodium_init` | Function | Initializes libsodium internals. | Called by tests/fuzz/bench/helper executables before using crypto/helper APIs. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_*.c`, `bench/bench_main.c`, `scripts/verify-roundtrip-helper.c` |
| `sodium_memcmp` | Function | Constant-time byte comparison helper. | Used by fuzz roundtrip validation when comparing decrypted/plaintext buffers. | `fuzz/fuzz_roundtrip.c` |
| `sodium_memzero` | Function | Secure memory wipe API. | Used in support programs to scrub key/plaintext/ciphertext/decryption buffers after checks. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_*.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `randombytes_buf_deterministic` | Function | Deterministic random-byte generator from fixed seed for reproducible data. | Used by benchmarks to build reproducible input corpora and stable measurements. | `bench/bench_base64.c`, `bench/bench_seal.c` |
| `crypto_box_PUBLICKEYBYTES` | Constant/Macro | Byte length of `crypto_box` public key. | Support buffer sizing for key material and related encoded fixture buffers. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_*.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `crypto_box_SEALBYTES` | Constant/Macro | Sealed-box overhead constant. | Support ciphertext sizing and roundtrip assertions in tests/fuzz/helper paths. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_roundtrip.c`, `scripts/verify-roundtrip-helper.c` |
| `crypto_box_SECRETKEYBYTES` | Constant/Macro | Byte length of `crypto_box` secret key. | Support key buffer sizing in tests/fuzz/bench/helper code. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_*.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `crypto_box_SEEDBYTES` | Constant/Macro | Seed length required by `crypto_box_seed_keypair`. | Used in deterministic fuzz key generation helpers. | `fuzz/fuzz_common.h` |
| `sodium_base64_ENCODED_LEN` | Constant/Macro (function-like) | Computes required Base64 output buffer length. | Used widely for fixture/output buffer sizing in tests/fuzz/bench/helper code. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_*.c`, `bench/bench_base64.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `sodium_base64_VARIANT_ORIGINAL` | Constant/Macro | Standard libsodium Base64 variant (with padding). | Shared support encode/decode variant for fixture generation and validation parity with production behavior. | `tests/test_salt.c`, `tests/test_fuzz_regressions.c`, `fuzz/fuzz_*.c`, `bench/bench_base64.c`, `bench/bench_seal.c`, `scripts/verify-roundtrip-helper.c` |
| `randombytes_SEEDBYTES` | Constant/Macro | Seed size required by deterministic randombytes APIs. | Used by benchmarks for seed buffers feeding `randombytes_buf_deterministic`. | `bench/bench_base64.c`, `bench/bench_seal.c` |

## Notes

- This inventory intentionally focuses on upstream libsodium objects directly
  used by this repository’s C code and headers.
- Non-upstream local symbol names (hooks/typedefs/wrappers) are intentionally
  excluded even when they mirror libsodium call signatures.
