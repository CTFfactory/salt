#!/usr/bin/env bash
# Validate GitHub Copilot customization files against the policy declared in
# .github/instructions/copilot-customization.instructions.md.
#
# Exit codes:
#   0 - clean
#   1 - one or more ERRORs (blocking)
#   2 - WARNINGs only (advisory; CI may treat as soft-fail)
set -euo pipefail

ROOT_DIR="${1:-.}"
cd "$ROOT_DIR"

POLICY_FILE=".github/instructions/copilot-customization.instructions.md"

err_count=0
warn_count=0

fail() {
  printf 'ERROR: %s\n' "$1" >&2
  err_count=$((err_count + 1))
}

warn() {
  printf 'WARNING: %s\n' "$1" >&2
  warn_count=$((warn_count + 1))
}

trim_scalar() {
  printf '%s' "$1" | tr -d "'\"" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//'
}

frontmatter() {
  local file="$1"
  awk '
    BEGIN { in_fm = 0; seen = 0 }
    /^---$/ {
      seen++
      if (seen == 1) { in_fm = 1; next }
      if (seen == 2) { exit }
    }
    in_fm { print }
  ' "$file"
}

extract_field() {
  local file="$1"
  local field="$2"
  frontmatter "$file" | sed -n "s/^${field}:[[:space:]]*//p" | head -n1
}

# ---------------------------------------------------------------------------
# Policy loader
# ---------------------------------------------------------------------------
#
# Parses the YAML block delimited by <!-- BEGIN COPILOT POLICY --> and
# <!-- END COPILOT POLICY --> in the SSOT instruction file. Populates:
#   POLICY_ALLOWED_TOOLS  (array)
#   POLICY_INVALID_TOOLS  (assoc: invalid -> replacement)
#   POLICY_APPROVED       (array)
#   POLICY_RETIRING       (assoc: model -> migration target)
#   POLICY_RETIRED        (assoc: model -> migration target)

declare -a POLICY_ALLOWED_TOOLS=()
declare -a POLICY_APPROVED=()
declare -A POLICY_INVALID_TOOLS=()
declare -A POLICY_RETIRING=()
declare -A POLICY_RETIRED=()

load_policy() {
  if [[ ! -f "$POLICY_FILE" ]]; then
    fail "policy SSOT not found at $POLICY_FILE"
    return 1
  fi

  local block
  block=$(awk '/<!-- BEGIN COPILOT POLICY -->/{flag=1;next}/<!-- END COPILOT POLICY -->/{flag=0}flag' "$POLICY_FILE")
  if [[ -z "$block" ]]; then
    fail "$POLICY_FILE missing BEGIN/END COPILOT POLICY markers"
    return 1
  fi

  local section=""
  while IFS= read -r raw; do
    case "$raw" in
      '```'*) continue ;;
      'allowed_tools:') section=allowed_tools; continue ;;
      'invalid_tools:') section=invalid_tools; continue ;;
      'approved_models:') section=approved_models; continue ;;
      'retiring_models:') section=retiring_models; continue ;;
      'retired_models:') section=retired_models; continue ;;
    esac

    case "$section" in
      allowed_tools)
        if [[ "$raw" =~ ^[[:space:]]+-[[:space:]]+(.+)$ ]]; then
          POLICY_ALLOWED_TOOLS+=("$(trim_scalar "${BASH_REMATCH[1]}")")
        fi
        ;;
      approved_models)
        if [[ "$raw" =~ ^[[:space:]]+-[[:space:]]+(.+)$ ]]; then
          POLICY_APPROVED+=("$(trim_scalar "${BASH_REMATCH[1]}")")
        fi
        ;;
      invalid_tools)
        if [[ "$raw" =~ ^[[:space:]]+([^:]+):[[:space:]]+(.+)$ ]]; then
          POLICY_INVALID_TOOLS["$(trim_scalar "${BASH_REMATCH[1]}")"]="$(trim_scalar "${BASH_REMATCH[2]}")"
        fi
        ;;
      retiring_models)
        if [[ "$raw" =~ ^[[:space:]]+([^:]+):[[:space:]]+(.+)$ ]]; then
          POLICY_RETIRING["$(trim_scalar "${BASH_REMATCH[1]}")"]="$(trim_scalar "${BASH_REMATCH[2]}")"
        fi
        ;;
      retired_models)
        if [[ "$raw" =~ ^[[:space:]]+([^:]+):[[:space:]]+(.+)$ ]]; then
          POLICY_RETIRED["$(trim_scalar "${BASH_REMATCH[1]}")"]="$(trim_scalar "${BASH_REMATCH[2]}")"
        fi
        ;;
    esac
  done <<< "$block"

  if [[ ${#POLICY_ALLOWED_TOOLS[@]} -eq 0 ]]; then
    fail "$POLICY_FILE allowed_tools list is empty"
  fi
  if [[ ${#POLICY_APPROVED[@]} -eq 0 ]]; then
    fail "$POLICY_FILE approved_models list is empty"
  fi
}

is_allowed_tool() {
  local needle="$1"
  local t
  for t in "${POLICY_ALLOWED_TOOLS[@]}"; do
    [[ "$t" == "$needle" ]] && return 0
  done
  return 1
}

is_approved_model() {
  local needle="$1"
  local m
  for m in "${POLICY_APPROVED[@]}"; do
    [[ "$m" == "$needle" ]] && return 0
  done
  return 1
}

applyto_matches_files() {
  local pattern="$1"
  local candidate
  local -a candidates=()
  local -a files=()

  # Bash compgen -G can hang on repo-relative patterns that begin with "**/".
  # For this validator we only need to know whether the pattern matches any
  # repository file, so probe only the equivalent root-relative form.
  if [[ "$pattern" == '**/'* ]]; then
    candidates=("${pattern#**/}")
  else
    candidates=("$pattern")
  fi

  for candidate in "${candidates[@]}"; do
    [[ -z "$candidate" ]] && continue
    mapfile -t files < <(compgen -G "$candidate" || true)
    if [[ ${#files[@]} -gt 0 ]]; then
      return 0
    fi
  done

  return 1
}

# ---------------------------------------------------------------------------
# Field-level helpers
# ---------------------------------------------------------------------------

require_field() {
  local file="$1" field="$2"
  if ! frontmatter "$file" | grep -qE "^${field}:"; then
    fail "$file missing required frontmatter field '${field}'"
  fi
}

forbid_field() {
  local file="$1" field="$2"
  if frontmatter "$file" | grep -qE "^${field}:"; then
    fail "$file has forbidden frontmatter field '${field}'"
  fi
}

validate_frontmatter_delimiters() {
  local file="$1" count
  count=$(grep -c '^---$' "$file" || true)
  if [[ "$count" -lt 2 ]]; then
    fail "$file has invalid frontmatter delimiters"
  fi
}

# Parse a tools: list (single line, YAML flow style: tools: ['a', 'b']) into
# one tool name per line on stdout. Multi-line block form is currently not used
# in this repository; we accept both flow and block styles.
parse_tool_values() {
  local file="$1"
  local raw
  raw=$(extract_field "$file" "tools")
  raw=$(printf '%s' "$raw" | sed 's/#.*$//')
  if [[ -z "$raw" ]]; then
    return 0
  fi
  if [[ "$raw" == \[*\] ]]; then
    raw=${raw#[}
    raw=${raw%]}
    IFS=',' read -r -a items <<< "$raw"
    local item
    for item in "${items[@]}"; do
      trim_scalar "$item"
      printf '\n'
    done
    return 0
  fi
  # Block scalar form: read continuation lines beginning with whitespace + '-'
  awk '
    BEGIN { in_fm = 0; seen = 0; in_tools = 0 }
    /^---$/ { seen++; if (seen == 1) { in_fm = 1; next } if (seen == 2) exit }
    !in_fm { next }
    /^tools:/ { in_tools = 1; next }
    in_tools {
      if (match($0, /^[[:space:]]+-[[:space:]]+(.+)$/, m)) {
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", m[1])
        gsub(/^['\''"]|['\''"]$/, "", m[1])
        print m[1]
        next
      }
      if ($0 !~ /^[[:space:]]/) { in_tools = 0 }
    }
  ' "$file"
}

check_tools() {
  local file="$1" tool replacement
  while IFS= read -r tool; do
    [[ -z "$tool" ]] && continue
    if [[ -n "${POLICY_INVALID_TOOLS[$tool]:-}" ]]; then
      replacement="${POLICY_INVALID_TOOLS[$tool]}"
      fail "$file uses invalid tool '$tool' (replace with '$replacement')"
      continue
    fi
    if ! is_allowed_tool "$tool"; then
      fail "$file uses unknown tool '$tool' (not listed in $POLICY_FILE allowed_tools)"
    fi
  done < <(parse_tool_values "$file")
}

parse_model_values() {
  local file="$1" raw
  raw=$(extract_field "$file" "model")
  raw=$(printf '%s' "$raw" | sed 's/#.*$//')
  raw=$(trim_scalar "$raw")
  [[ -z "$raw" ]] && return 0

  if [[ "$raw" == \[*\] ]]; then
    raw=${raw#[}
    raw=${raw%]}
    IFS=',' read -r -a items <<< "$raw"
    local item
    for item in "${items[@]}"; do
      trim_scalar "$item"
      printf '\n'
    done
  else
    trim_scalar "$raw"
    printf '\n'
  fi
}

check_models() {
  local file="$1" model
  while IFS= read -r model; do
    [[ -z "$model" ]] && continue
    if [[ -n "${POLICY_RETIRED[$model]:-}" ]]; then
      fail "$file uses retired model '$model' (migrate to '${POLICY_RETIRED[$model]}')"
      continue
    fi
    if [[ -n "${POLICY_RETIRING[$model]:-}" ]]; then
      warn "$file uses retiring model '$model' (plan migration to '${POLICY_RETIRING[$model]}')"
      continue
    fi
    if ! is_approved_model "$model"; then
      fail "$file uses unknown model '$model' (not listed in $POLICY_FILE approved_models)"
    fi
  done < <(parse_model_values "$file")
}

validate_markdown_path_references() {
  local file="$1" referenced_path
  while IFS= read -r referenced_path; do
    [[ -z "$referenced_path" ]] && continue
    referenced_path=${referenced_path#./}
    if [[ "$referenced_path" == *'*'* || "$referenced_path" == *'<'* || "$referenced_path" == *'>'* || "$referenced_path" == *','* || "$referenced_path" == *://* ]]; then
      continue
    fi
    if [[ ! -e "$referenced_path" ]]; then
      fail "$file references missing repository file '$referenced_path'"
    fi
  done < <(
    {
      grep -oE '(\./)?(\.github|docs|include|src|tests|scripts)/[^` )]+' "$file" || true
      grep -oE '(^|[^A-Za-z0-9_./-])(AGENTS\.md|README\.md|CONTRIBUTING\.md|CHANGELOG\.md|SECURITY\.md)([^A-Za-z0-9_./-]|$)' "$file" | \
        sed -E 's/^.*(AGENTS\.md|README\.md|CONTRIBUTING\.md|CHANGELOG\.md|SECURITY\.md).*$/\1/' || true
    } | sed 's/[`),.:]*$//' | sort -u
  )
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

load_policy

shopt -s nullglob globstar

agents=(.github/agents/*.agent.md)
instructions=(.github/instructions/*.instructions.md)
prompts=(.github/prompts/*.prompt.md)
skills=(.github/skills/*/SKILL.md)

[[ ${#agents[@]} -eq 0 ]] && fail "no agent files found under .github/agents"
[[ ${#instructions[@]} -eq 0 ]] && fail "no instruction files found under .github/instructions"
[[ ${#prompts[@]} -eq 0 ]] && fail "no prompt files found under .github/prompts"
[[ ${#skills[@]} -eq 0 ]] && fail "no skill files found under .github/skills"

for file in "${agents[@]}"; do
  validate_frontmatter_delimiters "$file"
  require_field "$file" "name"
  require_field "$file" "description"
  require_field "$file" "model"
  require_field "$file" "tools"
  forbid_field "$file" "applyTo"
  if frontmatter "$file" | grep -qE '^infer:'; then
    fail "$file uses deprecated frontmatter field 'infer'"
  fi
  check_tools "$file"
  validate_markdown_path_references "$file"
done

for file in "${instructions[@]}"; do
  validate_frontmatter_delimiters "$file"
  require_field "$file" "description"
  require_field "$file" "applyTo"
  forbid_field "$file" "tools"
  forbid_field "$file" "model"
  validate_markdown_path_references "$file"
done

for file in "${prompts[@]}"; do
  validate_frontmatter_delimiters "$file"
  require_field "$file" "description"
  forbid_field "$file" "applyTo"
  check_tools "$file"
  validate_markdown_path_references "$file"
done

for file in "${skills[@]}"; do
  validate_frontmatter_delimiters "$file"
  require_field "$file" "name"
  require_field "$file" "description"
  forbid_field "$file" "applyTo"
  forbid_field "$file" "tools"
  forbid_field "$file" "model"

  skill_name=$(trim_scalar "$(extract_field "$file" "name")")
  dir_name=$(basename "$(dirname "$file")")
  if [[ "$skill_name" != "$dir_name" ]]; then
    fail "$file has name '$skill_name' but directory is '$dir_name'"
  fi

  desc=$(extract_field "$file" "description")
  desc_trim=$(trim_scalar "$desc")
  desc_len=${#desc_trim}
  if (( desc_len > 1024 )); then
    fail "$file description is $desc_len chars (max 1024)"
  fi

  validate_markdown_path_references "$file"
done

# Duplicate agent display names
declare -A seen_agent_names=()
for file in "${agents[@]}"; do
  agent_name=$(trim_scalar "$(extract_field "$file" "name")")
  if [[ -n "${seen_agent_names[$agent_name]:-}" ]]; then
    fail "$file duplicates agent display name '$agent_name' also used in ${seen_agent_names[$agent_name]}"
  else
    seen_agent_names[$agent_name]="$file"
  fi
done

# Prompt agent reference resolution
valid_agents=(ask agent plan)
for file in "${agents[@]}"; do
  agent_name=$(trim_scalar "$(extract_field "$file" "name")")
  if [[ -n "$agent_name" ]]; then
    valid_agents+=("$agent_name")
  fi
done

for file in "${prompts[@]}"; do
  prompt_agent=$(trim_scalar "$(extract_field "$file" "agent")")
  if [[ -n "$prompt_agent" ]]; then
    found=0
    for candidate in "${valid_agents[@]}"; do
      if [[ "$prompt_agent" == "$candidate" ]]; then
        found=1
        break
      fi
    done
    if [[ "$found" -eq 0 ]]; then
      fail "$file references unknown agent '$prompt_agent'"
    fi
  fi
done

# Handoff agent reference resolution
# Collect valid slugs (file-name based, the canonical resolution key used by VS Code).
declare -a valid_handoff_slugs=()
for file in "${agents[@]}"; do
  slug=$(basename "$file" .agent.md)
  valid_handoff_slugs+=("$slug")
done

for file in "${agents[@]}"; do
  while IFS= read -r handoff_agent; do
    [[ -z "$handoff_agent" ]] && continue
    found=0
    for slug in "${valid_handoff_slugs[@]}"; do
      if [[ "$handoff_agent" == "$slug" ]]; then
        found=1
        break
      fi
    done
    if [[ "$found" -eq 0 ]]; then
      fail "$file handoff references unknown or non-slug agent '$handoff_agent' (use kebab-case slug, not display name)"
    fi
  done < <(
    awk '
      BEGIN { in_fm = 0; seen = 0; in_handoffs = 0; in_item = 0 }
      /^---$/ { seen++; if (seen == 1) { in_fm = 1; next } if (seen == 2) exit }
      !in_fm { next }
      /^handoffs:/ { in_handoffs = 1; next }
      in_handoffs {
        if (/^[[:space:]]+-[[:space:]]/) { in_item = 1 }
        if (in_item && match($0, /^[[:space:]]+agent:[[:space:]]+(.+)$/, m)) {
          val = m[1]
          gsub(/^[[:space:]]+|[[:space:]]+$/, "", val)
          gsub(/^['\''"]|['\''"]$/, "", val)
          print val
        }
        if ($0 !~ /^[[:space:]]/) { in_handoffs = 0; in_item = 0 }
      }
    ' "$file"
  )
done

# Models
for file in "${agents[@]}" "${prompts[@]}"; do
  check_models "$file"
done

# applyTo coverage
for file in "${instructions[@]}"; do
  raw_apply=$(frontmatter "$file" | sed -n "s/^applyTo:[[:space:]]*//p" | head -n1)
  raw_apply=$(printf '%s' "$raw_apply" | tr -d "'\"")
  IFS=',' read -r -a patterns <<< "$raw_apply"
  matched=0
  for pattern in "${patterns[@]}"; do
    pattern="$(echo "$pattern" | xargs)"
    [[ -z "$pattern" ]] && continue
    if applyto_matches_files "$pattern"; then
      matched=1
      break
    fi
  done
  if [[ "$matched" -eq 0 ]]; then
    warn "$file has applyTo that matches no files: $raw_apply"
  fi
done

if [[ -f .github/copilot-instructions.md ]]; then
  validate_markdown_path_references .github/copilot-instructions.md
  if ! grep -q '/copilot-config-review' .github/copilot-instructions.md; then
    fail ".github/copilot-instructions.md missing /copilot-config-review validation gate"
  fi
  if ! grep -q '/validate-copilot-config' .github/copilot-instructions.md; then
    fail ".github/copilot-instructions.md missing /validate-copilot-config validation gate"
  fi
fi

if (( err_count > 0 )); then
  printf 'Copilot customization validation failed: %d error(s), %d warning(s).\n' "$err_count" "$warn_count" >&2
  exit 1
fi
if (( warn_count > 0 )); then
  printf 'Copilot customization validation passed with %d warning(s).\n' "$warn_count"
  exit 2
fi
printf 'Copilot customization validation passed.\n'
