# Testing Guide

`TESTING.md` is the source of truth for testing in this repository. It defines the
test plan, the test taxonomy, the quality-control gates, and the trust
boundaries that matter for a CLI used to encrypt secrets for GitHub Actions and
Secrets API workflows.

## Goals

- Produce objective evidence that `salt` behaves correctly for valid inputs and
  fails closed for malformed or adversarial inputs.
- Prefer automated, repeatable tests so every change gets fast smoke coverage
  and deeper regression coverage in CI.
- Focus the strongest testing on trust boundaries: CLI arguments, stdin,
  base64/JSON key parsing, output encoding, shell integrations, and GitHub
  Secrets API request construction.
- Treat testing as risk reduction and quality control, not proof that the
  system is flaw-free.

## Test Plan

The repository follows a layered test strategy: fast smoke checks first,
deterministic unit and memory-safety checks next, then system-level testing and
trust-boundary fuzzing, with full CI acting as the quality-control release gate.

| Kind | Primary commands | Purpose | Notes |
|---|---|---|---|
| Smoke testing | `make ci-fast`, `make fuzz-smoke` | Fast build-verification checks for every change | Catch obvious breakage before deeper gates |
| Unit testing | `make check`, `make test` (compat alias), `make test-clang` | Deterministic cmocka coverage for success and failure paths | Validate CLI contract, parsing, and crypto wrapper behavior |
| Memory-safety testing | `make test-sanitize`, `make test-valgrind`, `make test-valgrind-cli` | Catch leaks, use-after-free, UB, and runtime memory regressions | `test-valgrind` covers the deterministic cmocka surface; `test-valgrind-cli` exercises real `main()` CLI process paths |
| System testing | `make test-qa`, `make test-docker` | Verify the compiled CLI end to end and across supported Ubuntu releases | `make test-qa` is the black-box exit-code and stdout/stderr contract gate |
| Coverage gate | `make coverage-check` | Enforce minimum coverage | Policy is **>99% line/function/block/branch** coverage for the enforced source scope (`src/main.c`, `src/salt.c`) |
| Fuzz testing | `make fuzz-build`, `make fuzz-run`, `make fuzz`, `make fuzz-regression` | Drive attacker-controlled and malformed inputs through trust boundaries | Harnesses: `parse`, `cli`, `output`, `boundary`, `roundtrip` |
| Monkey testing | long-budget `scripts/fuzz.sh` runs and extended `make fuzz-run` sessions | Explore random or less-structured inputs beyond curated seed corpora | Findings should be minimized and turned into regressions when practical |
| Security testing | `make test-sanitize`, `make test-valgrind`, `make fuzz-regression`, `make actionlint`, `make shellcheck` | Validate confidentiality, integrity, deterministic failure behavior, and workflow hardening | Cross-reference `SECURITY.md` for attack surfaces |
| Quality control | `make ci`, `make bench-check`, `make docs && make man`, `make validate-copilot` | Prevent functional, performance, docs, and configuration regressions from shipping | `make ci` is the full functional gate; benchmark checks run as a separate performance gate |

Canonical GNU flow for this repository is: maintainers run `./bootstrap` when
GNU build inputs change, then users/contributors run `./configure && make &&
make check && make install`. The recommended repo-local variant is the
out-of-tree Autotools build under `build/autotools` with `../../configure
--enable-tests`. All advanced targets (lint, docs, man, ci, ci-fast, fuzz-*,
coverage-check, bench-check, release, sbom, test-docker, package-deb-*) are
implemented natively in `Makefile.am` and available from the generated Makefile.

For complete build and installation instructions, platform-specific guidance, container builds, static binaries, package generation, and troubleshooting, see **[INSTALL.md](INSTALL.md)**.

## Coverage

Testing coverage in this repository is organized around the major features and
trust boundaries:

- **CLI input handling**: option parsing, stdin handling, argv scrubbing, and
  deterministic exit codes.
- **Key parsing**: raw base64 keys, strict JSON key objects, length checks, and
  malformed input rejection.
- **Encryption and encoding**: libsodium initialization, `crypto_box_seal`
  behavior, and base64 output generation.
- **Output contracts**: text vs JSON output, stderr/stdout separation, and
  stable error messaging for scripts.
- **Workflow integrations**: GitHub Secrets API helper flows, secret-name and
  secret-size validation, and request-shape correctness.
- **Operational regressions**: coverage thresholds, benchmark baselines, docs
  consistency, and Copilot customization validity.

Coverage is enforced and reported with these commands:

```sh
cd build/autotools
make coverage-check
make coverage-html
```

- `make coverage-check` is the policy gate and must remain above enforced line,
  function, block, and branch thresholds.
- `make coverage-check` currently enforces coverage over `src/main.c` and
  `src/salt.c` (the core CLI entry/encryption wrapper scope).
- `make coverage-html` generates themed HTML reports at
  `build/autotools/coverage/light/index.html` and
  `build/autotools/coverage/dark/index.html` (plus their detail pages/assets).
- Release tarball templates include the full `coverage/light/` and
  `coverage/dark/` report trees so release consumers can inspect coverage output
  without rebuilding locally.

### Enterprise coverage toolchain: `grcov`, `llvm-cov`, and `lcov`

The enforced repository gate remains `make coverage-check` (gcovr + gcov JSON
for block checks), because it is deterministic, fast, and already wired into
`make ci`. For enterprise reporting, audit, and interoperability workflows,
`grcov`, `llvm-cov`, and `lcov` are complementary tools rather than mutually
exclusive replacements.

| Tool | Strengths | Best fit in this repository |
|---|---|---|
| `grcov` | Fast aggregation; multi-format output (`html`, `lcov`, `cobertura`); good filtering (`--keep-only`, `--ignore`) and path remap controls | Cross-CI normalization and exporting `lcov.info`/Cobertura artifacts for external systems |
| `llvm-cov` | Native Clang source-based coverage (`show`, `report`, `export`); rich branch/region details; optional MC/DC support in modern LLVM flows | High-fidelity compiler-native analysis, especially on Clang-focused hardening and debugging paths |
| `lcov` + `genhtml` | Mature enterprise reporting; differential coverage, baseline/diff categorization, owner/date binning, and broad ecosystem compatibility | Governance-heavy reporting, diff-aware review gates, and archival-quality HTML reports |

#### Canonical interoperability patterns

1. **Current repo policy gate (required):**
   ```sh
   cd build/autotools
   make coverage-check
   ```
2. **Optional `lcov` capture/report for broader consumers:**
   ```sh
   cd build/autotools
   mkdir -p coverage
   lcov --capture --directory . --output-file coverage/salt.info --rc branch_coverage=1
   lcov --remove coverage/salt.info '/usr/*' '*/tests/*' --output-file coverage/salt.filtered.info --rc branch_coverage=1
   genhtml coverage/salt.filtered.info --output-directory coverage/lcov-html --branch-coverage --function-coverage --show-details --legend
   ```
3. **Optional `grcov` export from coverage artifacts:**
   ```sh
   cd build/autotools
   grcov . -s ../../ --keep-only '*/src/*' --ignore '*/tests/*' --branch --ignore-not-existing -t lcov -o coverage/grcov.info
   ```
4. **Optional Clang source-based path with `llvm-cov export`:**
   - Build/tests must use Clang source-based coverage flags (`-fprofile-instr-generate -fcoverage-mapping`), then merge profiles with `llvm-profdata merge`.
   - Use `llvm-cov report` for summaries and `llvm-cov export -format=lcov` (or JSON text format) for downstream systems.
   - Use `-path-equivalence` / `-compilation-dir` controls when build and report paths differ across hosts.

#### Enterprise robustness requirements

- Keep one authoritative gate (`make coverage-check`) and treat alternate tools
  as reporting/export layers to avoid metric drift between CI and release
  documentation.
- Normalize source paths explicitly (`--source-dir`, `--prefix-dir`,
  `-path-equivalence`, `--substitute`) in distributed CI to prevent false
  "missing source" noise and broken per-line annotations.
- Preserve branch coverage in every export path (`grcov --branch`,
  `lcov --rc branch_coverage=1`, `llvm-cov` branch summaries enabled) so
  dashboard values match security-focused testing expectations.
- Record exact tool versions in CI and release notes when changing coverage
  pipelines, because parser behavior and summary math can vary by version.
- For differential reporting at scale, prefer `lcov`/`genhtml` baseline+diff
  workflows while retaining the repo threshold gate for pass/fail control.

## Test Methods

### Smoke testing

Use the smoke gates for quick feedback before heavier checks:

```sh
cd build/autotools
make ci-fast
make fuzz-smoke
```

`make ci-fast` is the main build-verification test for day-to-day changes.
`make fuzz-smoke` adds a quick adversarial-input pass for every fuzz harness.

### Unit and memory-safety testing

Deterministic cmocka tests are the primary mechanism for verifying expected
behavior and failure paths:

```sh
cd build/autotools
make check
make test              # Compatibility alias for 'make check'
make test-clang
make test-sanitize
make test-valgrind
make test-valgrind-cli
```

When a bug is fixed, prefer adding or extending a deterministic cmocka
regression so the failure stays reproducible outside a fuzz harness.

The CLI cmocka surface is organized by behavior so changes stay localized:
`tests/cli/test_contract.c` covers top-level CLI contract and option handling,
`tests/cli/test_io.c` covers stdin/stdout and allocation paths,
`tests/cli/test_json.c` covers strict JSON key parsing, JSON output
encoding, and UTF-8 validation for RFC 8259 compliance, and
`tests/cli/test_runtime.c` covers signal, resource, and performance-smoke paths.
Shared fixtures live in `tests/cli/test_support.c`.

Fixture and mock infrastructure:

- Use `cli_test_setup` / `cli_test_teardown` for CLI tests that need isolated
  stream hooks, allocator hooks, signal state, or deterministic I/O fixtures.
- Use `salt_test_setup` / `salt_test_teardown` for lower-level library tests
  that override libsodium, allocator, encryption, or base64 hooks.
- Prefer shared helpers such as `stream_from_text()`,
  `stream_with_write_fail_after()`, `cli_failing_alloc`, and
  `cli_fail_on_nth_alloc` over local one-off fixtures.
- Keep failure injection narrow and reset hooks in teardown so tests remain
  order-independent.

### System testing

System testing in this repository focuses on the complete CLI and supported
build environments:

```sh
cd build/autotools
make test-qa
make test-docker
```

`make test-qa` exercises the compiled `bin/salt` binary as a black-box process
and is the authoritative end-to-end gate for exit-code behavior, stdout/stderr
separation, and shell-facing contract checks. `make test-docker` verifies the
supported Ubuntu matrix and catches compiler, toolchain, and runtime
differences that unit tests alone will miss.

### Packaging testing

Debian package generation is a first-class release artifact for Ubuntu
deployments. Use these commands to build and verify packages:

```sh
cd build/autotools
make package-deb-local SALT_PACKAGE_VERSION=0.0.0
make package-deb-docker SALT_PACKAGE_VERSION=0.0.0
```

`make package-deb-local` builds a local `.deb` using nFPM and verifies the
installed file structure matches expectations. `make package-deb-docker` builds
Ubuntu 22/24/26 packages in their respective Docker environments to ensure
deterministic dependency resolution and runtime linkage. Package artifacts are
validated for proper dependency declarations, man page installation, and binary
execution before release. See the `package-deb-docker` target in `Makefile` and
the `deb` stages in `docker/ubuntu/*/Dockerfile` for the build and validation
flow.

### Fuzzing and monkey testing

Trust-boundary fuzzing is a core security-testing technique for `salt` because
the tool processes attacker-influenced inputs. Use the standard harnesses for
structured fuzzing:

```sh
make fuzz-build
make fuzz-run
make fuzz-regression
```

For longer exploratory runs or more randomised input exploration, use:

```sh
./scripts/fuzz.sh
make fuzz-run FUZZ_MAX_TOTAL_TIME=600
```

Treat `./scripts/fuzz.sh` here as the repo-local long-budget mutation helper.
Remotely managed or sink-backed fuzzing campaign procedures and artifact sync
should be maintained externally to this repository.

For unattended overnight campaigns with external corpus storage (e.g., GCS, S3):

```sh
FUZZ_EXTERNAL_CORPUS_RESTORE_CMD='gsutil -m cp -r gs://mybucket/fuzz/* .' \
FUZZ_EXTERNAL_CORPUS_SYNC_CMD='gsutil -m cp -r .fuzz/* gs://mybucket/fuzz/' \
TOTAL_BUDGET_SECONDS=86400 HARNESS_SET=all ./scripts/fuzz.sh
```

(Replace `gsutil` commands with `aws s3 sync` or equivalent for your storage backend.)

**Harness development**: Use `/salt/fuzz/fuzz_common.h` as the template for new
harnesses. It provides `fuzz_copy_cstr()` and `fuzz_seed_keypair()` helpers that
enforce safe bounds and deterministic initialization. Review existing harnesses
(e.g., `fuzz_parse.c`) for initialization pattern and `LLVMFuzzerTestOneInput()`
structure.

Quick start:

```sh
cd build/autotools
make fuzz-build
make fuzz-regression
make fuzz-run FUZZ_MAX_TOTAL_TIME=120
```

Fuzz cheat sheet:

| If you want to... | Run this | Notes |
|---|---|---|
| replay the stable regression set | `make fuzz-regression` | Uses committed `fuzz/corpus/<harness>/` seeds plus persisted crash reproducers |
| run the fast smoke pass used in CI | `make fuzz-smoke` | Short mutation pass for every harness |
| do a short local exploratory run | `make fuzz-run FUZZ_MAX_TOTAL_TIME=120` | Grows the mutable `.fuzz/<harness>/corpus/` working set |
| promote minimized working inputs into committed seeds | `make fuzz-minimize` | Curate the result; do not check in the entire working corpus |

Repository policy:

- Keep committed seed corpora under `fuzz/corpus/<harness>/`.
- Keep working corpora, crashes, and logs under `.fuzz/`.
- Keep CI, `make fuzz-smoke`, `make fuzz-regression`, and release flows pinned
  to the minimal curated committed seeds plus persisted crash reproducers.
- Minimize interesting external campaign inputs before promoting them into the
  committed regression corpus.
- Do not check in the entire working corpus wholesale; promote only curated,
  high-signal minimized inputs.
- Keep committed corpora small enough for fast replay. As a rule of thumb, a
  harness corpus that grows past roughly 500 seeds should be reviewed, minimized,
  and pruned before promotion unless the new seeds cover distinct regressions or
  coverage edges.
- Keep fuzz findings open until the root cause is understood and a deterministic
  regression exists when practical.

### Security testing

Security testing here combines multiple techniques because no single gate is
enough for a secrets-handling CLI:

- unit tests for input validation and deterministic failures
- sanitizers and Valgrind for memory correctness
- fuzzing for malformed and adversarial trust-boundary inputs
- workflow and shell linting for GitHub Actions and helper scripts
- documentation review against `SECURITY.md` attack surfaces

Security testing reduces risk but does not prove the absence of vulnerabilities.

### Clang-specific and sanitizer testing

Use the standard Clang and sanitizer gates for release-relevant changes:

```sh
cd build/autotools
make test-clang
make test-sanitize
make scan-build
make tidy
```

These are the release-relevant Clang and sanitizer targets currently available
in the repository build system.

## Responsibilities

- **`TESTING.md`** must be updated when test taxonomy, coverage policy, fuzz
  harnesses, smoke gates, or release-quality criteria change.
- **C source changes** should add or update cmocka tests where behavior changes
  are deterministic and user-visible.
- **Fuzz or monkey-test findings** should be minimized, diagnosed, and turned
  into deterministic regressions when practical.
- **GitHub Secrets API workflow changes** must keep `README.md`, `SECURITY.md`,
  example scripts, and this file aligned.
- **CI changes** must preserve the distinction between smoke gates and the full
  quality-control gate.

## Quality-Control Gates

Use these commands as the authoritative quality-control checkpoints:

```sh
cd build/autotools
make coverage-check
make bench-check
make bench-check-advisory
make docs
make man
make validate-copilot
make ci
```

Quality-control policy:

- Coverage must stay strictly above the enforced `99%` line, `99%` function,
  `99%` block, and `99%` branch thresholds.
- `make ci-fast` must stay fast and trustworthy enough to act as a smoke gate.
- `make ci` is the full release gate and should remain the place where deeper
  regression, QA, coverage, docs, and symbol checks converge.
- Local release cutovers should run `make ci` before `make release`,
  `make coverage-html`, `make sbom`, and `make package-deb-docker` so the
  documented maintainer flow matches the GitHub `v*` release workflow.
- `make bench-check` is the separate **stable microbenchmark** regression gate.
- `make bench-check-advisory` runs process-spawn/cold-start checks in warn-only
  mode, because those measurements are runner-noise sensitive unless a stable
  performance environment is available.
- Documentation drift is treated as a quality issue, not optional cleanup.

## Trust Boundaries

Testing priorities should follow the repository's attack surfaces:

- **CLI and stdin inputs**: plaintext length, empty input, malformed flags,
  malformed JSON, truncated or invalid base64 keys.
- **Process surfaces**: `/proc/<pid>/cmdline`, `/proc/<pid>/environ`, stdout,
  stderr, crash dumps, and debug attach behavior.
- **Shell automation**: example scripts, curl/jq pipelines, error propagation,
  and masking behavior.
- **GitHub Actions runners**: fork PRs, untrusted workflow inputs, action
  pinning, log injection, artifact reuse, and least-privilege `GITHUB_TOKEN`
  permissions.
- **GitHub Secrets API flows**: public-key retrieval, `key_id` freshness,
  encryption request construction, size/naming limits, and structured-secret
  masking pitfalls.

`SECURITY.md` documents those attack surfaces in policy form; this file explains
how they map to tests and quality gates.

## Artifacts and Records

- HTML coverage artifacts are generated under `build/autotools/coverage/`.
- Text coverage summaries from `make coverage-check` are generated at
  `build/autotools/gcovr.txt` and `build/autotools/gcov_blocks.txt`.
- Fuzz working state lives under `.fuzz/`; committed regression seeds live under
  `fuzz/corpus/`.
- Benchmark baselines live under `bench/`.
- Benchmark baseline updates should include review notes describing the runner,
  compiler, libsodium package, and rationale for accepting changed medians or
  p95 values.
- CI logs and GitHub Actions job results are the primary records for smoke and
  quality-control execution.

## Out of Scope

- Live GitHub API integration tests are not run in CI; the repository instead
  validates local encryption, request shape, error handling, and script safety.
- Passing all tests does not prove the absence of exploitable flaws, especially
  in runner, network, or third-party service behavior outside this repository's
  control.
