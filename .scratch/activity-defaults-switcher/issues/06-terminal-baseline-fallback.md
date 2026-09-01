# 06: Terminal component + baseline fallback

**What to build:** The terminal component (written to `kdeglobals` `TerminalService`/`TerminalApplication` rather than mimeapps) and the baseline fallback: when no activity named "Default" exists, the first activity in the ordered list supplies fallbacks.

**Blocked by:** 05 (All components + profile store + capture).

**Status:** ready-for-agent

- [ ] Terminal flips per activity via `kdeglobals`.
- [ ] When no "Default" activity exists, the first activity in the ordered list supplies fallbacks.
- [ ] Changing the terminal in System Settings records a binding for the current activity.

**Note (from review of 04):** `KActivities::Consumer::activities()` documents no ordering guarantee (only "the list of all existing activities"). The spec's "first activity in the ordered list" fallback needs an explicit, tested notion of "ordered" — settle what it means and pin it with a test; do not assume `activities()` order is stable.
