# Agent Review Appendices

This file tracks per-agent robustness review status for Copilot customization.

## Review Criteria

- Frontmatter validity and naming conventions
- Valid model and tool usage
- Sub-agent convention compliance
- Skill and prompt linkage coverage
- Broken reference checks

## Orchestrator and Cross-Domain Agents (Mandatory Sub-Agent Metadata)

| Agent File | Status | Notes |
|---|---|---|
| `.github/agents/copilot-config-expert.agent.md` | Completed | Added `agents` and `handoffs`; routing to prompt-engineer, principal-software-engineer, repo-architect |
| `.github/agents/principal-software-engineer.agent.md` | Completed | Fixed title-casing and description punctuation; added `agents` and `handoffs` |
| `.github/agents/prompt-engineer.agent.md` | Completed | Added `agents` and `handoffs`; linked validation and editorial handoff |
| `.github/agents/repo-architect.agent.md` | Completed | Added `agents` and `handoffs`; added explicit Skills and Prompts linkage |
| `.github/agents/cicd-pipeline-architect.agent.md` | Completed | Added `agents` and `handoffs`; added explicit Skills and Prompts linkage |
| `.github/agents/release-automation-expert.agent.md` | Completed | Added `agents` and `handoffs`; added explicit Skills and Prompts linkage |
| `.github/agents/devops-expert.agent.md` | Completed | Added `agents` and `handoffs`; removed stale non-existent agent reference |

## Governance and Supporting Agents

| Agent File | Status | Notes |
|---|---|---|
| `.github/agents/dependency-pinning-expert.agent.md` | Completed | Normalized description punctuation; added explicit Skills and Prompts linkage |
| `.github/agents/se-technical-writer.agent.md` | Completed | Added explicit Skills and Prompts linkage |

## Specialist and Overlay Agent Backlog (Optional Sub-Agent Metadata)

| Agent File | Status | Next Action |
|---|---|---|
| `.github/agents/ansible-automation-expert.agent.md` | Completed | Added Skills and Prompts linkage to ansible skills and prompts |
| `.github/agents/inspec-compliance-expert.agent.md` | Completed | Added Skills and Prompts linkage to inspec profile and review prompt |
| `.github/agents/nagios-icinga2-expert.agent.md` | Completed | Added Skills and Prompts linkage to nagios plugin skill and review prompt |
| `.github/agents/proxmox-server-expert.agent.md` | Completed | Added explicit skills linkage for change planning and risk review prompt |
| `.github/agents/proxmox-ve-ops-expert.agent.md` | Completed | Added explicit linkage to proxmox change plan skill and risk prompt |
| `.github/agents/ubuntu-platform-expert.agent.md` | Completed | Added explicit linkage to ubuntu-relevant automation and hardening workflows |
| `.github/agents/vhs-expert.agent.md` | Completed | Replaced stale prompt references with existing vhs review prompt |
| `.github/agents/bash-script-expert.agent.md` | Completed | Added Skills and Prompts linkage and removed stale related-agent references |
| `.github/agents/pvj-*.agent.md` | Completed | Added family-level routing standard and optional handoff pattern in PvJ Expert; replaced stale external references |
| `.github/agents/pvj-blue-joe-player.agent.md` | Completed | New agent; fixed description separator to em-dash; pvj-expert already includes in agents list and handoff |

## Validation Result

All modified agent files in this implementation pass editor diagnostics at time of update.
