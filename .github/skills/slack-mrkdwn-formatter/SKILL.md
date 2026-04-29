---
name: slack-mrkdwn-formatter
description: 'Convert standard Markdown to Slack mrkdwn syntax. Use when a user asks to format, draft, or convert content for Slack messages, notifications, bot payloads, or webhook posts. Slack uses mrkdwn — its own markup variant — which differs meaningfully from standard Markdown.'
argument-hint: '[markdown text or target message type]'
---

# Skill: Slack mrkdwn Formatter

## Overview

Slack uses a markup variant called **mrkdwn** that resembles Markdown but differs
in syntax, supported features, and character escaping rules. Content authored as
standard Markdown will not render correctly in Slack without conversion. This
skill guides that conversion accurately.

> **Authoritative reference:** https://docs.slack.dev/messaging/formatting-message-text

## When to Use This Skill

Use this skill when:
- User asks to write, draft, or format a message specifically for Slack
- User asks to convert existing Markdown content to Slack format
- User is building a bot, webhook, or Incoming Webhook payload
- User mentions Slack notifications, Slack alerts, or Slack Block Kit
- User asks about Slack-specific formatting (mentions, channel links, dates)

---

## Conversion Reference

### Text Formatting

| Feature | Standard Markdown | Slack mrkdwn | Notes |
|---|---|---|---|
| Bold | `**bold**` or `__bold__` | `*bold*` | Single asterisks |
| Italic | `*italic*` or `_italic_` | `_italic_` | Underscores only |
| Strikethrough | `~~strike~~` | `~strike~` | Single tildes |
| Inline code | `` `code` `` | `` `code` `` | Same |
| Code block | ` ```lang\ncode\n``` ` | ` ```\ncode\n``` ` | Strip language hint |
| Blockquote | `> quote` | `> quote` | Same for single-line |
| Multi-line blockquote | n/a | `>>> quote` | Three `>` for contiguous paragraph |

**Rule:** Slack does not support nested formatting (e.g., `*bold _and italic_*`).
Keep only the outermost format marker.

---

### Links

**Standard Markdown:**
```
[link text](https://example.com)
```

**Slack mrkdwn:**
```
<https://example.com|link text>
```

**Auto-link (no display text):**
```
<https://example.com>
```

**Email link:**
```
<mailto:user@example.com|Email User>
```

Plain URLs in text are auto-transformed by Slack into clickable links — no
conversion needed unless you want custom display text.

---

### Lists

Both `- item` and `* item` render in user-posted messages. For **app-published
text** (bots, webhooks, Block Kit), there is no official list syntax. Use plain
text with line breaks and bullet characters:

```
• Item one
• Item two
• Item three
```

Ordered lists have very limited support. Prefer converting `1. …` sequences to
numbered plain-text lines (`1. Item`) or plain bullets.

---

### Headings

Headings (`#`, `##`, `###`, etc.) are **not supported** in mrkdwn.

**Strategies:**
- Short headings → `*HEADING TEXT*` (bold)
- Section titles → `*Title*\n` followed by content
- Structural dividers → omit entirely or use `---` (horizontal rules render in some clients)

---

## Unsupported Features and Handling

| Markdown Feature | Action |
|---|---|
| Headings `# H1` | Replace with `*BOLD TITLE*` |
| Tables | Rewrite as plain text columns, or escalate to Block Kit (`section` with `fields`) |
| HTML tags `<b>`, `<br>` etc. | Strip entirely |
| Nested formatting | Keep outermost style only |
| Images `![alt](url)` | Replace with `<url\|alt text>` link, or omit if decorative |
| Footnotes | Inline the content at the point of reference |
| Task lists `[ ]` / `[x]` | Replace with `•` for open, `✅` or `☑` for checked |
| Horizontal rules `---` | Omit or replace with blank line |

---

## Character Escaping (Important)

Slack uses `&`, `<`, and `>` as mrkdwn control characters. **Escape them** in
literal text before publishing:

| Character | Escape as |
|---|---|
| `&` | `&amp;` |
| `<` | `&lt;` |
| `>` | `&gt;` |

Do **not** HTML-entity-encode the entire string — only these three characters.

**Why this matters:** If user input flows into a Slack message unescaped, a
string like `<@everyone>` in arbitrary text could trigger a workspace-wide
notification. Always escape before substituting variables into mrkdwn strings.

---

## Slack-Specific Syntax

These features have no Markdown equivalent. Add them when the user's intent
implies mentions, links to Slack resources, or time-aware messages.

### User Mentions
```
<@U12345678>           # Mention by user ID
```
Always use IDs, not display names — names can change. Get the user ID from the
Slack profile → overflow menu → "Copy member ID".

### Channel Links
```
<#C12345678>           # Link to channel by ID (display name resolved by Slack)
<#C12345678|general>   # With explicit display name
```

### Group (User Group) Mentions
```
<!subteam^SAZ94GDB8>   # Mention a user group
```

### Special Mentions
```
<!here>       # Notify active members of a channel (use sparingly)
<!channel>    # Notify all channel members (use sparingly)
<!everyone>   # Notify all workspace members — #general only (use very sparingly)
```

### Localized Dates
```
<!date^1711843200^{date} at {time}|March 31, 2024 at 8:00 AM UTC>
```
Format: `<!date^<unix_timestamp>^<token_string>^<optional_url>|<fallback_text>>`

Token options: `{date_num}`, `{date}`, `{date_short}`, `{date_long}`,
`{date_pretty}`, `{date_short_pretty}`, `{date_long_pretty}`,
`{time}`, `{time_secs}`, `{ago}`

---

## Conversion Workflow

### Step 1 — Identify the output context
- Is this plain mrkdwn text, or does the layout need Block Kit?
- Does it contain mentions, links, or time-sensitive data?
- Is this user-to-user chat, a bot message, or an Incoming Webhook payload?

### Step 2 — Apply syntax conversions
- `**bold**` → `*bold*`
- `*italic*` → `_italic_`
- `~~strike~~` → `~strike~`
- `[text](url)` → `<url|text>`
- ` ```lang ` → ` ``` ` (strip language specifier)
- Strip or convert all unsupported features (see table above)
- Escape `&`, `<`, `>` in literal text values

### Step 3 — Handle unsupported features
- Replace headings with `*BOLD*` titles
- Convert tables to plain-text or Block Kit `fields`
- Inline footnotes; strip HTML tags
- Convert task lists to emoji + bullet lines

### Step 4 — Validate output
- No `**double-asterisk**`, `__double-underscore__`, or `~~double-tilde~~` remain
- No `[text](url)` Markdown links remain
- No heading `#` prefix lines remain
- No bare `<`, `>`, `&` in literal text that should be escaped
- Mentions use IDs, not display names
- Fallback text is included in all `<!date>` tokens

---

## Example Conversions

### Deployment Alert Notification

**Markdown input:**
```markdown
## Deployment Complete

**Service:** migration-worker
**Status:** ✅ Success
**Details:** Migrated `web-app-prod` from `cluster-a` → `cluster-b`

See the [release notes](https://github.com/example-org/example-repo/releases/tag/v1.4.0) for changes.
```

**Slack mrkdwn output:**
```
*Deployment Complete*

*Service:* migration-worker
*Status:* ✅ Success
*Details:* Migrated `web-app-prod` from `cluster-a` → `cluster-b`

See the <https://github.com/example-org/example-repo/releases/tag/v1.4.0|release notes> for changes.
```

---

### Batch Step Result Summary

**Markdown input:**
```markdown
### Batch Run: `migration_deploy.yaml`

| Step | VM | Status |
|---|---|---|
| snapshot-web | web-app-prod | ✅ Success |
| snapshot-db  | db-primary   | ✅ Success |
| snapshot-ci  | ci-runner    | ❌ Error   |

**Error:** `ci-runner` guest agent not responding — see [logs](https://example.com/logs/456).
```

**Slack mrkdwn output:**
```
*Batch Run: `migration_deploy.yaml`*

• snapshot-web — web-app-prod — ✅ Success
• snapshot-db  — db-primary   — ✅ Success
• snapshot-ci  — ci-runner    — ❌ Error

*Error:* `ci-runner` guest agent not responding — see <https://example.com/logs/456|logs>.
```

> Tables have no mrkdwn equivalent. Rewrite as bullet lines with `—` separators,
> or use Block Kit `section` with `fields` for aligned two-column layout.

---

### VM Status Report (with channel ping)

**Markdown input:**
```markdown
⚠️ **Alert** — Node `cluster-b:203` is unreachable.

Please check the infrastructure node immediately.
Contact the **on-call team** if the issue persists.
```

**Slack mrkdwn output:**
```
⚠️ *Alert* — Node `cluster-b:203` is unreachable.

Please check the infrastructure node immediately.
<!here> Contact the *on-call team* if the issue persists.
```

---

## Block Kit for Complex Layouts

Escalate from plain mrkdwn to Block Kit when the message needs:
- Tables or aligned columns (use `section` + `fields`)
- Interactive buttons or select menus
- Images alongside text
- Headers with visual separation (`header` block)

**Minimal section block using mrkdwn:**
```json
{
  "type": "section",
  "text": {
    "type": "mrkdwn",
    "text": "*migration-worker* moved `web-app-prod` successfully\n<https://example.com|View details>"
  }
}
```

Prototype layouts interactively at: https://api.slack.com/tools/block-kit-builder

---

## References

- Official Slack formatting docs: https://docs.slack.dev/messaging/formatting-message-text
- Block Kit reference: https://docs.slack.dev/reference/block-kit/blocks
- Block Kit Builder: https://api.slack.com/tools/block-kit-builder
- Emoji reference: https://github.com/iamcal/emoji-data
