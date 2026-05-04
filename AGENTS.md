# AGENTS Guide

This guide documents the `salt` repository for coding agents, including the C
CLI, its validation workflow, and the GitHub Copilot customizations kept under
`.github/`.

---

## Project Overview

This repository builds **salt**, a C CLI that encrypts plaintext as a libsodium
sealed box using a recipient public key and emits either base64 ciphertext or a
JSON envelope suitable for GitHub Actions Secrets API workflows.

Primary scope:
- Maintain the `salt` CLI and public C entrypoints in `include/salt.h`.
- Maintain deterministic tests, fuzzing, benchmarks, docs, and release assets.
- Maintain Copilot customizations for this codebase under `.github/`.

---

## Repository Layout

```text
.github/
  agents/                 Custom Copilot agent definitions
  instructions/           Auto-applied file-scoped guidance
  prompts/                Slash-invoked reusable prompts
  skills/                 Reusable skills (subdir/SKILL.md)
  workflows/              CI, release, and analysis workflows
  copilot-instructions.md Repository routing guidance
src/                      Production CLI + library implementation
include/                  Public and internal headers
tests/                    cmocka unit tests
fuzz/                     libFuzzer harnesses, corpora, and dictionaries
bench/                    Benchmark harnesses and committed baseline
examples/                 GitHub Actions secret-upload helper scripts
scripts/                  QA, fuzz orchestration, and validation helpers
docker/ubuntu/            Per-release Ubuntu compile-and-test Dockerfiles
docs/                     Man page source and supporting docs
AGENTS.md                 Repository guidance for coding agents
```

---

## Technology Stack

| Component | Technology | Notes |
|---|---|---|
| Language | C11 | Production CLI and library entrypoints |
| Crypto | libsodium 1.0.18+ | `crypto_box_seal`, base64 helpers, secure-memory helpers |
| Unit tests | cmocka | Deterministic success and failure-path coverage |
| Fuzzing | libFuzzer, AFL++ | Parser, CLI, output, boundary, and roundtrip harnesses |
| Benchmarking | Custom C harnesses | Regression gate backed by `bench/baseline.json` |
| Build system | GNU Autotools | Canonical GNU flow is `./bootstrap` for maintainers, then `./configure && make && make check && make install` from an Autotools build directory |
| CI/CD | GitHub Actions | Fast gate, full gate, release, CodeQL, fuzz smoke |
| Customization files | Markdown + YAML frontmatter | `.github/*` Copilot config files |

---

## Canonical GNU Build Flow

- Maintainers regenerating GNU build files: `./bootstrap`
- Users/contributors building from a prepared tree: `./configure && make && make check && make install`
- Recommended repo-local variant: run that flow out of tree under `build/autotools` via `../../configure --enable-tests`

## Advanced Make Targets

After the canonical GNU flow, all targets work from the GNU Autotools build
directory (`build/autotools`). All advanced targets are implemented natively in
`Makefile.am` and available from the generated Makefile.

| Goal | Command |
|---|---|
| Build CLI | `make build` or `make` |
| Run unit tests | `make test` or `make check` |
| Run clang gate | `make test-clang` |
| Run sanitizer tests | `make test-sanitize` |
| Run valgrind tests | `make test-valgrind` |
| Validate Ubuntu container matrix | `make test-docker` |
| Replay fuzz regression corpus | `make fuzz-regression` |
| Run benchmark regression gate | `make bench-check` |
| Lint and static analysis | `make lint` |
| Validate docs and man page | `make docs && make man` |
| Fast CI subset | `make ci-fast` |
| Full CI gate | `make ci` |
| Validate Copilot config | `make validate-copilot` |

---

## Build and Test

Use these exact commands when touching the corresponding surfaces:

```sh
cd build/autotools  # Always work from the Autotools build directory
make ci-fast        # Fast smoke gate (lint + build + test)
make ci             # Full release gate (all quality checks)
make test           # cmocka unit tests
make test-sanitize  # ASan/UBSan tests
make test-valgrind  # Valgrind memcheck
make fuzz-regression  # Replay committed fuzz corpus
make coverage-check   # Enforce coverage threshold
make bench-check      # Benchmark regression gate
make lint             # All static analysis and formatting checks
make docs && make man # Validate documentation and man page
```

Canonical build/test/install guidance for contributors is GNU standard:
maintainers run `./bootstrap` when needed, then users/contributors run
`./configure && make && make check && make install` from the `build/autotools`
directory.

### GNU Autotools (canonical)

```sh
./bootstrap
mkdir -p build/autotools
cd build/autotools
../../configure --enable-tests
make              # Build
make check        # Unit tests
make test-sanitize
make test-valgrind
make lint         # Static analysis
make docs         # Validate docs
make man          # Validate man page
make ci           # Full CI gate
```

For cross-version container verification, run:

```sh
cd build/autotools
make test-docker
```

---

## Code Conventions

- `salt_seal_base64()` performs the required libsodium initialization; do not
  add duplicate `sodium_init()` calls in higher-level CLI code.
- Validate every externally derived length before allocation or copy, especially
  plaintext, decoded key, and output-buffer sizes.
- Use libsodium helpers for encoding/decoding (`sodium_base642bin`,
  `sodium_bin2base64`) and check every return value immediately.
- Treat plaintext, decoded key bytes, and related intermediates as secrets:
  lock buffers with `sodium_mlock()` where practical and release them with
  `sodium_munlock()` / `sodium_memzero()` according to the buffer type.
- Keep stderr deterministic and parseable; stdout is reserved for the primary
  ciphertext or JSON result.
- Keep README, man page, helper scripts, and Make targets in sync when flags,
  workflows, or outputs change.

---

## Architecture Notes

- `src/salt.c` owns the sealed-box encryption wrapper and base64 output logic.
- `src/cli/parse.c` owns key parsing and strict JSON-key-object handling.
- `src/cli/output.c` owns output serialization (text and JSON modes) with UTF-8
  validation to ensure RFC 8259 compliant JSON output.
- `src/main.c` owns option parsing, stdin/argv handling, and production
  hardening such as core-dump suppression and argv scrubbing.
- `include/salt.h` is the public API surface; internal CLI helpers live in
  `src/cli/internal.h` and `src/cli/output.h`.
- `tests/` covers library, CLI, and production-`main()` behavior.
- `fuzz/` includes five harnesses: parse, cli, output, boundary, and roundtrip.
- `examples/` must stay aligned with the GitHub Actions secrets guidance in
  `README.md` and validate secret names and sizes before upload.
- Source of truth for Copilot customization schema and precedence:
  `.github/instructions/copilot-customization.instructions.md`.
- Entry routing for this repo lives in `.github/copilot-instructions.md`.

---

## Audit-to-Plan Workflow

Many specialist agents in this repository have corresponding audit skills that systematically scan their domain for weaknesses, issues, or configuration gaps. These agents follow a unified **audit-to-plan workflow** when invoked:

### Workflow Stages

1. **Audit Discovery** — Agent invokes its domain audit skill (e.g., `audit-implementation-weaknesses` for Implementation Secure C Expert).
2. **Finding Prioritization** — Findings are grouped by severity (CRITICAL → HIGH → MEDIUM → LOW) and by root cause or category.
3. **Plan Generation** — Agent creates a structured implementation plan with prioritized todos, each including:
   - Finding reference (CWE ID, file, line)
   - Proposed fix or remediation
   - Validation gates (exact make commands to run)
   - Expected outcome
4. **User Review** — Agent exits plan mode to show audit results and plan for user approval before implementation begins.
5. **Execution Tracking** — Agent maintains SQL-backed todos with dependencies; updates todo status as work progresses.

### Agents with Audit Capabilities

The following agents have audit skills and follow this workflow:

| Agent | Audit Skill | Scope | Domain |
|---|---|---|---|
| Implementation Secure C Expert | `audit-implementation-weaknesses` | `src/` | CWE-702/658 implementation weaknesses |
| Design Threat Model Expert | `audit-design-weaknesses` | `include/` | CWE-701 API design flaws |
| SBOM Expert | `audit-sbom` | `.` | Release SBOM metadata parity |
| GNU Autotools Expert | `audit-gnu-autotools` | `.` | GNU Autotools portability and CI parity |
| Libsodium Expert | `audit-libsodium-lowlevel-safety` | `src/` | Low-level Ed25519 API misuse (CVE-2025-69277) |
| Valgrind Expert | `audit-c-memory-safety` | `src/` | Memory-safety and leak patterns |

### Audit Result Storage

All audit results are stored in a standardized JSON format defined by `.github/schemas/audit-report.schema.json`. The format includes:
- Agent name and skill name
- ISO 8601 timestamp
- Audit scope (file, directory, or module)
- Findings array with severity, CWE/rule, file/line, evidence, proposed fix, and validation gates
- Summary statistics (count by severity)

### Implementation Example

When invoking the **Implementation Secure C Expert** to address a concern about null-pointer checks in `src/cli/parse.c`:

```bash
# User request: "audit memory safety in src/cli/parse.c"
#
# Agent response (5-phase flow):
# 1. Audit runs: audit-implementation-weaknesses on src/cli/parse.c
# 2. Findings prioritized: HIGH-severity null-check finding at line 42
# 3. Plan generated: 1 todo "cwe-476-parse-malloc-null-check" with fix steps
# 4. Plan shown for approval: user sees finding + todo list
# 5. After approval: agent implements fix, validates with make test-sanitize
```

### Related Documentation

- **Workflow details:** `.github/instructions/audit-to-plan.instructions.md`
- **Audit result schema:** `.github/schemas/audit-report.schema.json`
- **Routing guide:** `.github/copilot-instructions.md`
- **Individual audit skills:** `.github/skills/audit-*/SKILL.md`

---

- `.github/workflows/ci.yml` runs the Copilot config gate, fast gate, GCC
  matrix, clang gate, full gate, bench gate, and fuzz smoke jobs.
- `.github/workflows/release.yml` runs the full CI gate, builds the dynamic and
  static Linux amd64 release archives, and publishes `SHA256SUMS` on semver tags.
- `.github/workflows/codeql.yml` provides additional code analysis coverage.
- `docker/ubuntu/{20,22,24,26}/Dockerfile` doubles as the supported Ubuntu
  compile-and-test matrix.
- All CI jobs work from the GNU Autotools build directory (`build/autotools`).
- Before finalizing customization changes, run `/copilot-config-review` and
  `/validate-copilot-config`.
