# Copilot Routing Guide

Use this file as the default routing map for Copilot tasks in this repository.

This repository uses specialist Copilot agents to keep changes consistent across C code, tests, CI workflows, and customization files. Each agent has a narrow domain so recommendations stay practical and aligned with project conventions.

When a task touches only one domain, route directly to the matching specialist. When a task spans domains, start with an orchestrator and follow the documented handoff sequence to avoid conflicting edits.

Use this guide together with `.github/instructions/*.instructions.md` and `.github/skills/*/SKILL.md`: routing decides who should do the work, instructions define file-level rules, and skills provide reusable workflows.

## Audit-to-Plan Workflow

Many specialist agents have corresponding audit skills that systematically scan their domain for weaknesses, code smells, or configuration issues. When these agents are invoked, they follow a unified audit-to-plan workflow:

1. **Audit Discovery** — Agent invokes its domain audit skill to identify current state
2. **Finding Prioritization** — Findings are grouped by severity and CWE/rule class
3. **Plan Generation** — Structured implementation plan is created from findings
4. **User Review** — Agent exits plan mode to show audit results and plan for approval
5. **Execution Tracking** — SQL-backed todos with validation gates track implementation

Agents with audit capabilities declare them in their frontmatter with:
- `audit-skill` — name of the audit skill to invoke
- `audit-scope` — default scope for audit (e.g., `src/`, `include/`)

For details on this workflow and which agents support audits, see `.github/instructions/audit-to-plan.instructions.md`.

## Task Routing

- Copilot customization files (`.github/agents`, `.github/instructions`, `.github/prompts`, `.github/skills`): use `Copilot Config Expert`
- Customization content quality and prompt writing: use `Prompt Engineer`
- Architecture and cross-domain engineering decisions: use `Principal Software Engineer`
- Repository structure and module boundaries: use `Repo Architect`
- CI workflows and Makefile automation: use `CI/CD Pipeline Architect`
- Release orchestration and changelog/versioning: use `Release Automation Expert`
- Release artifact signing, GPG key handling in GitHub Actions secrets, and build provenance attestation: use `Release Signing & Attestation Expert`
- Syft CLI usage, Syft configuration, cataloger behavior, Syft version/checksum installation, SPDX/CycloneDX SBOM generation, SBOM post-processing, release SBOM metadata, `make sbom`, and Snyk/Trivy SBOM-replacement tradeoff analysis: use `SBOM Expert`
- Snyk/Trivy security-scan integration in Make/CI (tokens, cache/update behavior, fail-on policy, and deterministic gate design): use `CI/CD Pipeline Architect` with `SBOM Expert`
- Operational deployment and infrastructure tooling: use `DevOps Expert`
- GNU Autotools inputs (`configure.ac`, `Makefile.am`, `bootstrap`), macro/conditional design, and GNU Build System workflows (`autoreconf`, `configure`, out-of-tree builds, `make check`, `make distcheck`): use `GNU Autotools Expert`
- Bash helper scripts, QA harnesses, fuzz orchestration scripts, benchmark comparison scripts, and GitHub Secrets example scripts: use `Bash Script Expert`
- CLI flags, help text, stdout/stderr behavior, exit codes, diagnostics, man page parity, and user-facing option design: use `CLI UX Designer`
- Black-box QA, `make test-qa`, Docker/system testing, release smoke checks, and live or dry-run GitHub Secrets workflows: use `E2E QA Engineer`
- libsodium sealed-box cryptography design and review: use `Libsodium Expert`
- Design-phase security weakness analysis, CWE-701 API contract review, and C header trust-boundary review: use `Design Threat Model Expert`
- CWE-702/CWE-658 implementation weakness audits, bounds-check hardening, and CERT-C remediation across `src/` and `include/`: use `Implementation Secure C Expert`
- libsodium CVE triage, low-level Ed25519 API misuse analysis, and crypto vulnerability response: use `Libsodium Vulnerability Response Expert`
- cmocka unit test strategy and mocking patterns: use `Cmocka Testing Expert`
- Testing strategy, test plan, and testing taxonomy work (`TESTING.md`): use `Principal Software Engineer` for scope decisions, `Cmocka Testing Expert` for implementation details, and `SE: Tech Writer` for document drafting
- GCC compiler diagnostics and standards-compliant C fixes: use `GCC C Expert`
- Clang diagnostics, sanitizers, and static-analysis friendly C fixes: use `Clang C Expert`
- CodeQL C/C++ workflow tuning, query selection, and alert triage: use `CodeQL C Expert`
- libFuzzer harnesses, corpus strategy, and crash triage: use `Fuzzing Expert`
- Valgrind memcheck diagnostics and leak analysis: use `Valgrind Expert`
- Benchmark harness changes and regression gate (`bench/`, `make bench-check`): use `Principal Software Engineer` for scope and `CI/CD Pipeline Architect` if Makefile or CI gates change
- Code-smell audits and structural refactor proposals (C, headers, tests, fuzz, bench): use `Principal Software Engineer` (run `/audit-code-smells`)
- Security testing, attack-surface review, and GitHub runner/Secrets API threat modeling: use `Principal Software Engineer` (run `/security-review`)
- VHS demos, `.tape` scripts, terminal recordings, and `vhs/` maintenance: use `VHS Expert`
- GitHub Actions SHA pins, Makefile tool-version variables (e.g., `ACTIONLINT_VERSION`), APT package version pinning, and Docker image digest pinning: use `Dependency Pinning Expert`
- GitHub Actions Secrets API workflows (including Bash key retrieval,
  encryption, and `PUT` requests): use `Bash Script Expert` for script safety, `CLI UX Designer` for command UX, `SE: Tech Writer` for docs, `Libsodium Expert` for crypto correctness, `E2E QA Engineer` for dry-run/live workflow validation, and `Principal Software Engineer` for cross-domain decisions

All `.github/agents/*.agent.md` files are intended to be selectable runtime agents. Keep `user-invocable: true` explicit in each agent frontmatter so dropdown/runtime discovery does not depend on default behavior.

## Validation Gate

Before finalizing customization changes, run these in sequence:

1. `/copilot-config-review`
2. `/validate-copilot-config`

If findings conflict, prioritize schema validity and overlap-precedence rules from `.github/instructions/copilot-customization.instructions.md`.

## Sub-Agent Convention

For orchestrator agents, prefer explicit `agents` and `handoffs` metadata in frontmatter.

- Use `agents` for selectable delegation paths.
- Use `handoffs` for sequential workflow transitions.

For specialist agents, metadata is optional when routing is clearly documented in body text.

## Multi-Domain Routing

Use these sequences when a request spans multiple domains.

- Cryptography plus tests: `Libsodium Expert` -> `Cmocka Testing Expert` -> `CI/CD Pipeline Architect` (if gates change)
- Design weakness review to implementation hardening: `Design Threat Model Expert` -> `Implementation Secure C Expert` -> `Clang C Expert` or `GCC C Expert` (apply fixes) -> `Cmocka Testing Expert` (tests) -> `CI/CD Pipeline Architect` (if gates change)
- libsodium CVE triage to regression coverage: `Libsodium Vulnerability Response Expert` -> `Libsodium Expert` (sealed-box review) -> `Cmocka Testing Expert` (regression tests) -> `CI/CD Pipeline Architect` (if gates change)
- Full security review spanning design, implementation, and crypto: `Design Threat Model Expert` -> `Implementation Secure C Expert` -> `Libsodium Vulnerability Response Expert` -> `CodeQL C Expert` (static-analysis confirmation)
- Compiler diagnostics plus runtime verification: `GCC C Expert` or `Clang C Expert` -> `Valgrind Expert` -> `Cmocka Testing Expert`
- CodeQL alert remediation plus compiler hardening: `CodeQL C Expert` -> `Clang C Expert` or `GCC C Expert` -> `Cmocka Testing Expert` -> `CI/CD Pipeline Architect` (if gates change)
- Fuzzing plus hardening: `Fuzzing Expert` -> `Clang C Expert` -> `Valgrind Expert` -> `CI/CD Pipeline Architect` (if gates change)
- Shell-facing CLI behavior: `CLI UX Designer` -> `Bash Script Expert` -> `E2E QA Engineer` (if black-box QA changes)
- Helper scripts plus GitHub Secrets flows: `Bash Script Expert` -> `Libsodium Expert` -> `E2E QA Engineer` -> `SE: Tech Writer` (if docs change)
- Autotools and build-system validation: `GNU Autotools Expert` -> `CI/CD Pipeline Architect` (if gates change)
- CI pipeline plus release: `CI/CD Pipeline Architect` -> `Dependency Pinning Expert` -> `Release Automation Expert`
- Release signing and provenance rollout: `Release Signing & Attestation Expert` -> `CI/CD Pipeline Architect` -> `Dependency Pinning Expert` -> `Release Automation Expert` -> `SE: Tech Writer`
- SBOM release metadata: `SBOM Expert` -> `Dependency Pinning Expert` (if Syft/Snyk/Trivy pins change) -> `Release Automation Expert` (if release assets or checksums change) -> `SE: Tech Writer` (if docs change)
- Snyk/Trivy adoption or replacement analysis: `SBOM Expert` -> `CI/CD Pipeline Architect` -> `Dependency Pinning Expert` -> `Release Automation Expert` (if release artifacts or checksums change)
- Copilot customization plus content quality: `Copilot Config Expert` -> `Prompt Engineer` -> `Principal Software Engineer` (for trade-offs)
- Code-smell audit plus remediation: `Principal Software Engineer` (run `/audit-code-smells`) -> `GCC C Expert` or `Clang C Expert` (apply fixes) -> `Cmocka Testing Expert` (test smells) -> `CI/CD Pipeline Architect` (if gates change)

When uncertain, start with `Principal Software Engineer` for triage and delegation.

## Failure Recovery

- If `/copilot-config-review` reports schema errors, route to `Copilot Config Expert` first.
- If model/tool guidance conflicts across files, treat `.github/instructions/copilot-customization.instructions.md` as authoritative.
- If validation fails in CI, fix blocking errors before continuing with feature work.
- If coverage drops below the enforced threshold or a test taxonomy change lands
  without a matching `TESTING.md` update, route to `Cmocka Testing Expert` or
  `Fuzzing Expert` before merging.
- If Syft invocation or cataloging behavior changes unexpectedly, route to `SBOM Expert` before changing post-processing.
- If SBOM validation fails, route to `SBOM Expert` before changing release assets.
- If Snyk or Trivy is proposed as a Syft replacement, route to `SBOM Expert` first and require explicit parity criteria before implementation.
- If a handoff target is unclear, fall back to `Principal Software Engineer` for routing.
- If CWE-701 design weaknesses are identified in `include/` headers, route to `Design Threat Model Expert` before making implementation changes.
- If CWE-702 or CWE-658 implementation weaknesses are found in `src/`, route to `Implementation Secure C Expert` for structured CERT-C-guided remediation.
- If a libsodium CVE is published or low-level Ed25519 API misuse is suspected, route to `Libsodium Vulnerability Response Expert` before modifying any crypto code path.

## Overlay Usage

Overlay agents, prompts, and skills should be used only when the target repository contains that domain.

- Infrastructure overlays (Ansible, InSpec, Nagios, Proxmox, Ubuntu) are opt-in by context.
- Do not apply overlays to generic repositories unless the domain is explicitly present.
