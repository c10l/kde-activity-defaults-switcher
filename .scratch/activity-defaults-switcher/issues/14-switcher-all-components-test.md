# 14: Switcher-level test that flips all 13 components

**What to build:** The "all 13 components resolve and flip per activity" behaviour is currently verified piecemeal — the writer covers the 12 mime components plus the terminal write, and the resolver covers terminal resolution — but no single switcher-level end-to-end test asserts all 13 components flip in one scenario.

**Blocked by:** 06 (Terminal component + baseline fallback).

**Status:** ready-for-agent

- [ ] One switcher test drives two activities with distinct bindings for all 13 components and asserts every component's default follows the current activity, then the baseline.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `tests/tst_switcher.cpp` — `switchingActivityFlipsEveryComponent` (currently covers 3 components).
- `tests/tst_writer.cpp` — `writesEveryMimeComponentToItsPrimaryKey`.
- `.scratch/activity-defaults-switcher/spec.md` — user story 4 and the *Components* decision.
- `CONTEXT.md` — vocabulary.
