# Lint Suppressions

This document records every active lint suppression in the repository.

Policy:
- Suppressions are a last resort.
- Fixes are preferred over suppressions whenever practical.
- Scope must be as narrow as possible.
- Every suppression must include rationale, risk, owner, review date, and removal trigger.

## Current Baseline

Validation run on 2026-04-25:
- `make actionlint`: pass
- `make tidy`: pass (no user-code diagnostics emitted; tool reports aggregate warning counts including suppressed non-user-code noise)
- `make format`: pass
- `make cppcheck`: pass
- `make shellcheck`: pass
- `make codespell`: pass
- `make lint`: pass

## Active Suppression Ledger

| Tool | Location | Mechanism | Scope | Rationale | Risk if kept | Review by | Removal trigger | Owner |
|---|---|---|---|---|---|---|---|---|
| clang-tidy | `.clang-tidy` | Disabled checks: `-bugprone-easily-swappable-parameters` | Global (all checked files) | C APIs in this project intentionally use small, same-type parameter sets where semantic names and tests provide the primary safety net. This check generated repetitive low-signal findings during development and obscured higher-severity diagnostics. | Parameter-order mistakes can still happen if review quality drops. | 2026-07-22 | Re-enable when wrapper types or stronger signature distinctions are introduced for repeated same-type parameters. | Maintainers |
| clang-tidy | `.clang-tidy` | Disabled checks: `-misc-include-cleaner` | Global | Include usage is intentionally explicit for clarity across GCC/Clang and fuzz/test targets; this check produced churn recommendations that risked portability regressions (in particular it flags every libsodium facade-include and every POSIX feature-test pulled symbol). | Header hygiene drift if includes grow organically. | 2026-10-01 | Re-enable after compile_commands-based include audit and cross-compiler verification. | Maintainers |
| clang-tidy | `.clang-tidy` | Disabled checks: `-readability-function-cognitive-complexity`, `-readability-identifier-length`, `-readability-magic-numbers` | Global | Readability checks are intentionally de-prioritized relative to correctness/security checks in this low-level C CLI. These generate style-only noise that masks higher-priority findings. | Reduced pressure for style consistency and refactor hygiene. | 2026-10-01 | Re-enable selectively after a dedicated readability cleanup pass. | Maintainers |
| clang-tidy | `src/cli/signal.c`, `tests/cli/test_support.c`, `tests/cli/test_contract.c`, `tests/cli/test_io.c`, `tests/cli/test_json.c`, `tests/cli/test_runtime.c`, `tests/cli/test_characterization.c`, `tests/test_fuzz_regressions.c` | Per-site `/* NOLINTNEXTLINE(bugprone-reserved-identifier) */` | 9 sites | POSIX feature-test macro `_POSIX_C_SOURCE` must be named exactly to enable conforming POSIX APIs; the reserved-identifier check has no other live diagnostics in this repository. | None — narrowly scoped to the documented POSIX usage. | 2026-10-01 | Remove if/when the project drops POSIX feature-test macros. | Maintainers |
| clang-tidy | `src/cli/parse.c`, `src/cli/output.c`, `src/cli/stream.c`, `src/salt.c`, `tests/cli/test_contract.c`, `tests/cli/test_helpers.c`, `tests/cli/test_io.c`, `tests/cli/test_json.c`, `tests/cli/test_runtime.c`, `tests/cli/test_support.c`, `tests/test_fuzz_regressions.c`, `tests/test_salt.c` | Per-site `/* NOLINT*(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */` | 33 sites | Suppressions now remain only where libc interfaces are required (varargs formatting and selected C library calls) and analyzer false positives persist despite bounded lengths and explicit checks. Refactors removed prior `memset`/`memmove`/`memcpy` suppressions in shared code paths. | Remaining suppressions can mask newly introduced unsafe call sites if reviewers stop enforcing bounded-length patterns. | 2026-10-01 | Continue reducing by centralizing formatted-error helpers and retiring per-site suppressions as analyzer support improves. | Maintainers |
| clang-tidy | `src/cli/parse.c`, `src/cli/output.c`, `src/cli/stream.c`, `src/salt.c` | Per-site `/* NOLINTBEGIN/END(clang-analyzer-valist.Uninitialized) */` | 6 sites | The `va_list` is initialized by the immediately preceding `va_start`; the analyzer cannot prove the path-flow link in current toolchains. | None — the flow is local and obvious. | 2026-10-01 | Remove if a future analyzer release tracks the dependency. | Maintainers |
| clang-tidy | `tests/cli/test_support.c`, `tests/test_salt.c` | Per-site `/* NOLINT*(readability-non-const-parameter) */` | 5 stubs | Test stubs (`failing_crypto_box_seal`, `failing_bin2base64`, `cli_fake_error`, `cli_fake_zero_output`, `cli_fake_success`) must match the libsodium and `salt_seal_base64` ABI verbatim; adding `const` would break the function-pointer assignments. | None — narrowly scoped to fixed external ABI. | 2026-10-01 | Remove if/when test indirection wraps the stubs behind a typedef that allows `const` qualification. | Maintainers |
| cppcheck | `Makefile.am` | `--suppress=missingIncludeSystem` | Global cppcheck invocation | System-header availability differs across local/CI/distroless environments; this suppression removes non-actionable missing-system-header noise so cppcheck failures represent repository-owned code issues. | Could hide genuine system-header path misconfiguration. | 2026-10-01 | Remove when cppcheck environment is fully containerized and deterministic across all gates. | Maintainers |
| cppcheck | `Makefile.am` | `--inline-suppr` | Enables local, line/file suppressions if needed | Keeps suppression scope narrow by allowing per-site exceptions instead of broad global disables. This is intentionally enabled but must be governed by documentation and comments where used. | Undocumented inline suppressions could accumulate over time. | 2026-06-30 | Keep enabled only if every inline suppression is documented in this file and reviewed in PRs. | Maintainers |
| scan-build | `Makefile.am` | `-disable-checker security.insecureAPI.DeprecatedOrUnsafeBufferHandling` | Global scan-build invocation | This checker reports nearly every bounded stdlib formatting/memory usage in this codebase and duplicates clang-tidy findings, obscuring higher-signal security diagnostics already enforced in the lint gate. | A genuine insecure API call could be missed in scan-build output if clang-tidy coverage is reduced. | 2026-10-01 | Re-enable once helper-level refactors reduce false positives enough for scan-build signal to stay actionable. | Maintainers |
| codespell | `Makefile.am` | `--ignore-words-list=bu,crasher,crashers` | Global codespell invocation | `crasher/crashers` are valid fuzzing terms; `bu` appears in legitimate short tokens and generated contexts. Ignoring these prevents recurrent false positives without reducing typo detection meaningfully. | True typo on those exact tokens may be missed. | 2026-07-22 | Remove individual ignored terms once no longer used in code/docs/corpus naming. | Maintainers |
| codespell | `Makefile.am` | `--skip=build,bin,.git,fuzz/corpus,terraform,autom4te.cache,build-aux` | Path-local skip | Excludes generated artifacts, VCS internals, and transient build-system outputs where spelling findings are non-actionable and unstable between runs. | Typos in skipped/generated outputs are not reported. | 2026-12-31 | Keep unless skipped paths become source-of-truth inputs for release artifacts. | Maintainers |

## Requirements For New Suppressions

Any new suppression must include:
- Exact location and mechanism.
- Why a direct fix is not currently feasible.
- Why the suppression scope is the narrowest safe option.
- Risk assessment and explicit removal trigger.
- A review-by date and accountable owner.

Any PR that adds or changes a suppression must update this file in the same change.
The repository enforces this via `make lint-suppressions` (backed by `scripts/validate-lint-suppressions.sh`).
