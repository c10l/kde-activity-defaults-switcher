---
name: work-ticket
description: "Work one ticket from the activity-defaults-switcher plan to done, TDD-first, within its blockers and scope. Use when the user says to work on / pick up / grab a ticket, or assigns a ticket number."
---

# Work on a Ticket

You are working in the **activity-defaults-switcher** project — the repository you are in. All work is a ticket under `.scratch/activity-defaults-switcher/issues/`, numbered in dependency order. Take exactly one ticket to done — no more, no less.

If told to "grab the next ticket", take the lowest-numbered `ready-for-agent` ticket whose **Blocked by** tickets are all done.

## 1. Read before you touch anything

Read, in order:

1. The ticket — `.scratch/activity-defaults-switcher/issues/<NN>-*.md`. Its acceptance criteria are the contract.
2. The spec — `.scratch/activity-defaults-switcher/spec.md`.
3. The glossary — `CONTEXT.md`. Use its vocabulary in all code, tests, and reports: **Activity, Component, Binding, Profile, Application, Baseline activity**. Do not use the glossary's _Avoid_ words: rule, mapping, override, handler, service, map, application type.
4. The ADRs — `docs/adr/`. In particular ADR-0001: the daemon flips the system-wide default per activity. Do not contradict an ADR.

## 2. Check blockers and status first

- If the ticket's **Blocked by** lists any ticket that is not done, stop and say so.
- If **Status** is not `ready-for-agent` (e.g. `blocked — pending design`), stop and say so.
- Only start when both pass. Never start blocked work "in anticipation".

## 3. TDD: red → green, one slice at a time

Follow the `tdd` skill. The essentials:

- Write a failing test that asserts one acceptance criterion's external behaviour, at an agreed seam.
- Make it pass with the smallest change.
- Repeat, one test → one implementation per cycle. Do not write all tests up front, and do not write production code without a failing test first.
- Refactoring is for the review stage, not the red/green loop.

## 4. Test only at the agreed seams

From the spec's Testing Decisions:

- **Resolver** — pure; no KDE/D-Bus/filesystem calls. The primary unit-test target.
- **Writer** — test against a temporary config directory.
- **kded shell** — test with a fake resolver/writer injected.

A good test asserts behaviour through the public interface, never implementation details, and its expected values come from the spec or a worked example — never recomputed the way the code does it.

## 5. Scope discipline

- Satisfy the ticket's acceptance criteria and nothing else. Later tickets' work is out of bounds (e.g. no KCM work before ticket 05).
- If you hit a decision or bug that belongs to another ticket, record it in your report; do not fix it silently and do not expand the ticket.

## 6. Verify, then report

- Tests green (`ctest` or the project's test command).
- Tickets 02+ include an end-to-end check (e.g. switch activity → the default flips). Do it.

Report concisely when done:

- What you built, as behaviour (not a diff).
- Which tests you added and which acceptance criteria they pin.
- Any deviation from the spec, and notes for later tickets.
