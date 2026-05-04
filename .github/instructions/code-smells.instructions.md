---
description: 'Code-smell catalog tailored to the salt C/libsodium/cmocka project; rules to avoid common implementation, test, and CLI smells'
applyTo: '**/src/**/*.c,**/tests/**/*.c,**/include/**/*.h,**/fuzz/**/*.c,**/bench/**/*.c'
---

# Code Smell Conventions

Use these rules to detect and prevent code smells when editing C, header, test, fuzz, and benchmark files. This file is complementary to:

- `c-memory-safety.instructions.md` — memory-safety specifics (precedence: memory-safety > smells when they overlap).
- `libsodium-patterns.instructions.md` — crypto API specifics.
- `cmocka-testing.instructions.md` — test conventions.

If a smell rule conflicts with one of those files, the more specific instruction wins.

## Bloater smells

- **Long function** — flag any function over ~60 lines or more than 3 levels of nesting. Split into named helpers with single responsibilities.
- **Long parameter list** — more than 5 parameters indicates a missing struct. Group related arguments (buffer + length, key + key length) into a typed struct or pass via the existing `salt_*` types.
- **Primitive obsession** — repeated raw `unsigned char *` + `size_t` pairs for keys, ciphertexts, or base64 buffers should be wrapped in a typed view (e.g. `salt_bytes_view`) when the same pair appears in 3+ signatures.
- **Data clumps** — the same set of arguments traveling together across multiple call sites is a clump; extract a struct.
- **Large translation unit** — `.c` files growing past ~500 lines should be split along clear seams (parsing vs. crypto vs. I/O).

## Dispensable smells

- **Dead code** — remove unreachable branches, commented-out blocks, and unused statics. Do not leave `#if 0` blocks; rely on git history.
- **Duplicate code** — identical or near-identical blocks appearing twice are a warning; three times is a defect. Extract a helper or inline the duplication only when it is genuinely incidental.
- **Speculative generality** — do not add parameters, hooks, or abstraction layers "for future use". Add them when the second concrete caller appears.
- **Comment as deodorant** — a comment explaining *what* obscure code does is a smell; rename, restructure, or extract until the code is self-explanatory. Comments should explain *why* (invariants, threat-model assumptions, libsodium quirks).
- **Magic numbers** — replace numeric literals (key sizes, sentinel exit codes, buffer multipliers) with named constants from `include/salt.h` or a local `static const`.

## Coupler / change-preventer smells

- **Shotgun surgery** — if a single behavior change requires edits in many unrelated files, the responsibility is scattered. Consolidate behind a single function or header.
- **Inappropriate intimacy / leaky internals** — public headers in `include/` must not expose helpers meant for `src/` only. Internal helpers belong in `src/cli/internal.h` (compatibility shim: `include/cli/internal.h`).
- **Feature envy** — a function that mostly manipulates fields of another module's struct should move to that module.
- **Header bloat** — every public header symbol expands the ABI surface. Default to `static` in `.c` files; promote only when a second caller needs it.

## CLI and error-handling smells

- **Silent failure** — every error path must produce a deterministic stderr line *and* a stable non-zero exit code. Returning early without diagnostics is a smell.
- **Ambiguous exit codes** — reusing the same non-zero exit code for unrelated failure classes hides bugs. Map each failure category (input, crypto, internal) to a documented code defined in headers.
- **Mixed stdout/stderr** — anything that is not the program's primary output (base64 ciphertext) belongs on stderr. Diagnostic prints to stdout corrupt downstream pipelines.
- **errno snooping after libsodium** — libsodium does not set `errno`. Branch on the documented return code only.
- **Unchecked return code** — every libsodium, allocator, and stdio call must have its return inspected on the same logical line or the next statement.

## Test smells (apply to `tests/**/*.c`)

- **Assertion roulette** — multiple unrelated assertions in one test with no message; on failure the cause is unclear. Split into focused tests or add `_msg` variants.
- **Eager test** — one test exercising several behaviors. Each test should verify one observable behavior.
- **Mystery guest** — tests that depend on unnamed external files, environment variables, or the system clock. Inputs must be inline or built in setup fixtures.
- **Conditional test logic** — `if`/loops that gate assertions hide failures. Use parameterized cmocka tests or separate cases.
- **Fragile test** — assertions on incidental output (timestamps, addresses, error text wording that is not part of the contract). Assert on stable contract surface only.
- **Test interdependence** — tests that must run in a specific order share hidden state. Each test must pass in isolation.

## Fuzz and benchmark smells

- **Non-deterministic harness** — fuzz harnesses must not depend on `time()`, `getenv()`, or RNG outside of libsodium's controlled paths. Same input must reach the same code path.
- **Benchmark without baseline** — adding a benchmark without updating `bench/baseline.json` (or documenting why) hides regressions.
- **Slow microbenchmark setup** — heavy per-iteration setup makes timings meaningless. Hoist setup outside the measured loop.

## Decision rules

- If you cannot name a smell in this catalog, do not invent one to justify a refactor.
- When fixing a smell, do the minimum necessary edit; do not bundle unrelated cleanups.
- When a smell is intentional (e.g. a long function generated by table data), add a one-line comment explaining the exemption.

## Validation

- Run `make lint` after smell-driven edits.
- Run `make test` and `make test-sanitize` to confirm behavior is unchanged.
- For test-file changes, also run `make coverage-check` to verify coverage did not regress.
