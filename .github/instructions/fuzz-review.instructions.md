---
description: 'Where to review salt fuzzing artifacts and how to report overnight libFuzzer results'
applyTo: '**/.fuzz/**,**/fuzz-*.log,**/scripts/fuzz.sh,**/fuzz/corpus/**'
---

# Fuzz Review Instructions

Use these rules when reviewing completed fuzzing campaigns for the `salt` CLI.
They define where the overnight artifacts live, which files are authoritative
for findings, and how to decide whether follow-up test updates are warranted.

## Artifact map

- Start with `.fuzz/summary.txt` for the top-level verdict, total budget,
  per-harness budgets, corpus counts, crash counts, and the log path recorded
  for each harness.
- Read `.fuzz/run.log` next for chronology: build start, harness order,
  per-harness completion, minimization, and any non-zero return-code messages.
- Use `.fuzz/<harness>/logs/fuzz.log` for per-harness libFuzzer metrics such as
  `DONE`, `stat::number_of_executed_units`, `stat::average_exec_per_sec`,
  `stat::peak_rss_mb`, and recommended-dictionary output.
- Use `.fuzz/<harness>/logs/merge.log` to confirm that the end-of-run
  `-merge=1` minimization succeeded and to capture minimized corpus size,
  features, and coverage-edge totals.
- Inspect `.fuzz/<harness>/crashes/` directly. Any crash artifact in these
  directories is a blocker even if the campaign otherwise ran to completion.
- Compare `.fuzz/<harness>/corpus/` with committed regression seeds under
  `fuzz/corpus/<harness>/` before recommending corpus updates.
- Treat repo-root `fuzz-*.log` files as supplemental worker output. Prefer the
  per-harness logs under `.fuzz/<harness>/logs/` when both exist.

## Overnight-scope rules

- `scripts/fuzz.sh` is the authoritative source for overnight orchestration.
- The default overnight harness set is `core`, which runs the three
  highest-churn harnesses: `parse`, `cli`, and `output`.
- When `HARNESS_SET=all` is present, the overnight run also covers `boundary`
  and `roundtrip`.
- If the run used the default `core` set, call out that `boundary` and
  `roundtrip` remain covered by `make fuzz-run`, `make fuzz-smoke`, and
  `make fuzz-regression` rather than that specific long-run campaign.
- Record whether the run used the default 7-hour budget or an override via
  `TOTAL_BUDGET_SECONDS`, and note whether `PARALLEL` was greater than `1`.

## Review checklist

- Confirm the summary result and verify whether crash counts are zero for every
  harness listed in `.fuzz/summary.txt`.
- Check `.fuzz/run.log` for `CRASH:`, `ERROR:`, `WARN:`, or non-clean harness
  completion lines. A clean report requires both zero crash artifacts and no
  unresolved harness failure.
- Search logs for sanitizer or libFuzzer failure signatures such as
  `AddressSanitizer`, `UndefinedBehaviorSanitizer`, `libFuzzer: deadly signal`,
  `runtime error:`, `timeout`, or `OOM`.
- Capture, per harness, the execution count, execution rate, peak RSS, and the
  minimized corpus size after merge.
- Note whether corpus minimization succeeded for every overnight harness.
- Distinguish between a clean run with low coverage growth and an actual bug.
  Plateaued coverage is a review note, not a failure, unless it contradicts an
  expected baseline.

## Reporting rules

- Lead with the outcome: clean run vs. crash findings.
- Separate confirmed findings from follow-up recommendations.
- Cite concrete file paths in the report so the next reviewer can jump directly
  to the same artifacts.
- Do not recommend new cmocka unit tests unless a crash, timeout, sanitizer
  finding, or specific reproducer reveals a missing behavior contract.
- Prefer fuzz-corpus recommendations when the run is clean but the minimized
  working corpus under `.fuzz/` materially exceeds the committed seeds under
  `fuzz/corpus/`.
- Do not recommend checking in the entire working corpus wholesale. Promote
  only curated, high-signal minimized inputs that improve regression coverage.
- If the overnight run used `HARNESS_SET=core`, say so explicitly instead of
  implying full-harness long-run coverage.

## Example review shape

```text
Result: clean overnight fuzz run; no crash artifacts under .fuzz/*/crashes/.

Evidence:
- .fuzz/summary.txt
- .fuzz/run.log
- .fuzz/parse/logs/fuzz.log
- .fuzz/cli/logs/fuzz.log
- .fuzz/output/logs/fuzz.log

Follow-up:
- No mandatory unit-test additions from this run.
- Consider curating new regression seeds from .fuzz/{parse,cli,output}/corpus
  into fuzz/corpus/{parse,cli,output}/ if the minimized corpus materially grew.
```
