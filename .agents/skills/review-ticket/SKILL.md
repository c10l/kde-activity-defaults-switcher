---
name: review-ticket
description: "Review a ticket's implementation against its acceptance criteria and update its status. Use when the user says to review a ticket or review the implementation of a ticket."
---

# Review a Ticket

You are reviewing an implementation in the **activity-defaults-switcher** project — the repository you are in. Your job: judge a claimed-finished ticket against its contract, then update its status. Implementing agents may not touch Status or tick boxes; you do.

## 1. Gather before judging

Read, in order:

1. The ticket — `.scratch/activity-defaults-switcher/issues/<NN>-*.md`. Its acceptance criteria are the contract.
2. The spec — `.scratch/activity-defaults-switcher/spec.md`.
3. The glossary — `CONTEXT.md`. Code and tests must use its vocabulary (Activity, Component, Binding, Profile, Application, Baseline activity) and none of the _Avoid_ words.
4. The ADRs — `docs/adr/`. The work must not contradict an ADR (e.g. ADR-0001: flip the system-wide default per activity).

## 2. Inspect the work

- Find the ticket's commit(s) (`git log`, `git show`) — or the uncommitted diff if it hasn't been committed.
- Run the tests (`ctest --test-dir build --output-on-failure` or the project's test command).
- Read the changed source and tests in full.

## 3. Judge every acceptance criterion

Check each box in the ticket, one by one, against the actual code and test output: met, not met, or unclear.

Also check:

- **Seams** — tests sit at the agreed seams (resolver pure; writer on a temp config dir; shell with fakes) and assert behaviour, not implementation.
- **Vocabulary** — no _Avoid_ words in code, tests, or comments.
- **Scope** — nothing from later tickets (e.g. no KCM work before ticket 07).
- **Process** — work is committed, and the agent did not self-edit the ticket's Status or tick its boxes.

## 4. Verdict

- **Approve** — every criterion met. Set `Status: done`, tick the boxes, and commit that change.
- **Approve with fixes** — only minor, non-blocking findings. Do not mark done.
- **Send back** — a criterion is unmet or something is blocking. Report exactly what's missing. Do not mark done.

**Every finding must land in the ticket backlog — never only in the chat report.** For each finding, blocking or not:

- If it belongs to an existing ticket, append it there (a `Note from review of NN:` line, or a new checkbox when it is a concrete acceptance criterion).
- Otherwise create a new ticket under `issues/` numbered to preserve dependency order, with its own `Blocked by` list and acceptance criteria.
- Commit the ticket edits alongside any Status/checkbox change, so the finding survives past the session.

## 5. Report

Report concisely: the verdict, what passed, findings (blocking vs non-blocking, in dependency order), and the ticket changes you made — for each finding, which existing ticket it was appended to, or which new ticket it became.
