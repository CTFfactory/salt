# Optional Overlays

This document defines opt-in overlays that should only be adopted when the target repository needs that domain.

## Proxmox Infrastructure Overlay

Adopt this overlay only for repositories that manage Proxmox VE infrastructure directly.

Files:

- .github/agents/proxmox-server-expert.agent.md
- .github/instructions/packer.instructions.md

Integration notes:

1. Map environment variable examples to your chosen naming policy.
2. Confirm provider and template assumptions match your environment.
3. Keep these files out of base pack installs for non-Proxmox repositories.

## Nagios and Icinga2 Monitoring Overlay

Adopt this overlay only for repositories that create shell-based monitoring checks for Nagios or Icinga2.

Files:

- .github/agents/nagios-icinga2-expert.agent.md
- .github/instructions/nagios-plugin.instructions.md
- .github/prompts/nagios-plugin-review.prompt.md
- .github/skills/create-nagios-plugin/SKILL.md

Integration notes:

1. Validate local monitoring conventions before using plugin examples.
2. Update any error-taxonomy expectations to match your tools.
3. Exclude this overlay for repositories that use non-Nagios monitoring stacks.

## Bash Workflow Overlay

Adopt this overlay only if your repository has the same multi-domain shell workflow patterns that combine script utilities, incident-response orchestration, and monitoring plugin checks.

Files:

- .github/agents/bash-script-expert.agent.md
- .github/instructions/bash-script.instructions.md

Integration notes:

1. Split applyTo patterns if your shell directories are simpler.
2. Remove domain-specific assumptions that do not exist in your repo.
3. Keep this overlay optional to avoid applying unrelated shell rules.

## Infrastructure Automation and Compliance Overlay

Adopt this overlay for repositories that need deep guidance for ansible-lint, Ansible automation, InSpec compliance checks, Ubuntu LTS compatibility, and Proxmox VE 8/9 operations.

Files:

- .github/agents/ansible-automation-expert.agent.md
- .github/agents/inspec-compliance-expert.agent.md
- .github/agents/ubuntu-platform-expert.agent.md
- .github/agents/proxmox-ve-ops-expert.agent.md
- .github/instructions/ansible.instructions.md
- .github/instructions/ansible-lint.instructions.md
- .github/instructions/inspec.instructions.md
- .github/instructions/ubuntu-lts.instructions.md
- .github/instructions/proxmox-ve.instructions.md
- .github/prompts/ansible-lint-review.prompt.md
- .github/prompts/ansible-role-hardening.prompt.md
- .github/prompts/inspec-control-review.prompt.md
- .github/prompts/proxmox-change-risk-review.prompt.md
- .github/skills/create-ansible-role/SKILL.md
- .github/skills/run-ansible-lint/SKILL.md
- .github/skills/create-inspec-profile/SKILL.md
- .github/skills/proxmox-ve-change-plan/SKILL.md

Integration notes:

1. Keep modern Ubuntu LTS behavior as default and isolate 14.04 to 18.04 as compatibility-only logic.
2. Align ansible-lint invocation and rule handling between local workflows and CI.
3. Reuse existing Proxmox safety patterns and avoid conflicting instructions across overlays.

## PvJ Competition Overlay

Adopt this overlay only for repositories that operate Pros vs Joes game workflows.

Files:

- .github/agents/pvj-blue-pro-captain.agent.md
- .github/agents/pvj-expert.agent.md
- .github/agents/pvj-game-master.agent.md
- .github/agents/pvj-gold-team-expert.agent.md
- .github/agents/pvj-grey-team-expert.agent.md
- .github/agents/pvj-range-master.agent.md
- .github/agents/pvj-red-team-expert.agent.md

Integration notes:

1. Keep PvJ agents out of non-competition repositories.
2. Verify related agent references resolve only when the PvJ overlay is installed.
3. Treat PvJ workflow assumptions as event-specific and update runbooks per event.

## Classification Note

Dependency pinning artifacts are template-level and should remain in the base pack after placeholder substitution, not in optional overlays.
