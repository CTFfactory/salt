# Copilot Customization Manifest

Auto-curated inventory of customization assets present in this repository.
Status labels:

- READY: in active use as-is.
- TEMPLATE: in use, but contains placeholder language that future repos may need to substitute.

This manifest does not list overlay assets that are not present in this repository.
Re-run the audit (or `scripts/validate-copilot-config.sh`) after adding or removing files.

## Agents

- READY: .github/agents/bash-script-expert.agent.md
- READY: .github/agents/cicd-pipeline-architect.agent.md
- READY: .github/agents/clang-c-expert.agent.md
- READY: .github/agents/cli-ux-designer.agent.md
- READY: .github/agents/cmocka-testing-expert.agent.md
- READY: .github/agents/codeql-c-expert.agent.md
- READY: .github/agents/copilot-config-expert.agent.md
- READY: .github/agents/dependency-pinning-expert.agent.md
- READY: .github/agents/design-threat-model-expert.agent.md
- READY: .github/agents/devops-expert.agent.md
- READY: .github/agents/e2e-qa-engineer.agent.md
- READY: .github/agents/fuzzing-expert.agent.md
- READY: .github/agents/gcc-c-expert.agent.md
- READY: .github/agents/gnu-autotools-expert.agent.md
- READY: .github/agents/implementation-secure-c-expert.agent.md
- READY: .github/agents/libsodium-expert.agent.md
- READY: .github/agents/libsodium-vuln-response-expert.agent.md
- READY: .github/agents/principal-software-engineer.agent.md
- READY: .github/agents/prompt-engineer.agent.md
- READY: .github/agents/release-automation-expert.agent.md
- READY: .github/agents/repo-architect.agent.md
- READY: .github/agents/sbom-expert.agent.md
- READY: .github/agents/se-technical-writer.agent.md
- READY: .github/agents/valgrind-expert.agent.md
- READY: .github/agents/vhs-expert.agent.md

## Instructions

- READY: .github/instructions/actionlint.instructions.md
- READY: .github/instructions/agentsmd.instructions.md
- READY: .github/instructions/c-memory-safety.instructions.md
- READY: .github/instructions/changelog.instructions.md
- READY: .github/instructions/ci.instructions.md
- READY: .github/instructions/cmocka-testing.instructions.md
- READY: .github/instructions/code-smells.instructions.md
- READY: .github/instructions/contributing.instructions.md
- READY: .github/instructions/copilot-customization.instructions.md
- READY: .github/instructions/dependency-pinning.instructions.md
- READY: .github/instructions/design-weakness-review.instructions.md
- READY: .github/instructions/fuzz-review.instructions.md
- READY: .github/instructions/github-secrets-integration.instructions.md
- READY: .github/instructions/gnu-autotools.instructions.md
- READY: .github/instructions/implementation-weakness-guards.instructions.md
- READY: .github/instructions/libsodium-lowlevel-vuln-guards.instructions.md
- READY: .github/instructions/libsodium-patterns.instructions.md
- READY: .github/instructions/makefile.instructions.md
- READY: .github/instructions/readme.instructions.md
- READY: .github/instructions/sbom.instructions.md
- READY: .github/instructions/security.instructions.md
- READY: .github/instructions/vhs-tape.instructions.md

## Prompts

- READY: .github/prompts/audit-code-smells.prompt.md
- READY: .github/prompts/cmocka-test-scaffold.prompt.md
- READY: .github/prompts/copilot-config-review.prompt.md
- READY: .github/prompts/release-readiness.prompt.md
- READY: .github/prompts/sealed-box-design-review.prompt.md
- READY: .github/prompts/security-review.prompt.md
- READY: .github/prompts/vhs-tape-review.prompt.md

## Skills

- READY: .github/skills/audit-c-memory-safety/SKILL.md
- READY: .github/skills/audit-code-smells/SKILL.md
- READY: .github/skills/audit-design-weaknesses/SKILL.md
- READY: .github/skills/audit-gnu-autotools/SKILL.md
- READY: .github/skills/audit-implementation-weaknesses/SKILL.md
- READY: .github/skills/audit-libsodium-lowlevel-safety/SKILL.md
- READY: .github/skills/audit-sbom/SKILL.md
- READY: .github/skills/audit-syft/SKILL.md
- READY: .github/skills/create-agentsmd/SKILL.md
- READY: .github/skills/create-changelogmd/SKILL.md
- READY: .github/skills/create-copilot-instructions/SKILL.md
- READY: .github/skills/create-makefile/SKILL.md
- READY: .github/skills/create-readmemd/SKILL.md
- READY: .github/skills/create-vhs-tape/SKILL.md
- READY: .github/skills/generate-cmocka-tests/SKILL.md
- READY: .github/skills/generate-sbom/SKILL.md
- READY: .github/skills/github-secrets-workflow/SKILL.md
- READY: .github/skills/run-actionlint/SKILL.md
- READY: .github/skills/setup-sealed-box-scaffolding/SKILL.md
- READY: .github/skills/slack-mrkdwn-formatter/SKILL.md
- READY: .github/skills/update-agent-models/SKILL.md
- TEMPLATE: .github/skills/update-ci-skill/SKILL.md
- TEMPLATE: .github/skills/update-contributing-skill/SKILL.md
- TEMPLATE: .github/skills/update-dependency-pins/SKILL.md
- READY: .github/skills/update-gnu-build-system/SKILL.md
- TEMPLATE: .github/skills/update-security-skill/SKILL.md
- READY: .github/skills/update-syft/SKILL.md
- READY: .github/skills/validate-copilot-config/SKILL.md

## Maintenance

- Sync after adding/removing any file under `.github/agents`, `.github/instructions`, `.github/prompts`, or `.github/skills`.
- `scripts/validate-copilot-config.sh` enforces frontmatter and policy correctness; this manifest tracks presence and status.
