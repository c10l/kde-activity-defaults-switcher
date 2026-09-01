# 06: Terminal component + baseline fallback

**What to build:** The terminal component (written to `kdeglobals` `TerminalService`/`TerminalApplication` rather than mimeapps) and the baseline fallback: when no activity named "Default" exists, the first activity in the ordered list supplies fallbacks.

**Blocked by:** 05 (All components + profile store + capture).

**Status:** ready-for-agent

- [ ] Terminal flips per activity via `kdeglobals`.
- [ ] When no "Default" activity exists, the first activity in the ordered list supplies fallbacks.
- [ ] Changing the terminal in System Settings records a binding for the current activity.
- [ ] A terminal value the daemon itself writes to `kdeglobals` is not captured back as a user edit — only a genuine System Settings terminal change records a binding (the terminal-side equivalent of the 05 `m_lastDefaults` fix).

**Note (from review of 05):** The profile store already persists terminal bindings (group `Terminal`); terminal is deliberately absent from `readCurrentMimeDefaults` and the mime map (kdeglobals, as scoped). Terminal capture lands here, so `m_lastDefaults` must track what the writer actually applied (mime now, kdeglobals terminal here) — see the checkbox above and the 05 review finding on `applyAndRemember`.

**Note (from review of 04):** `KActivities::Consumer::activities()` documents no ordering guarantee (only "the list of all existing activities"). The spec's "first activity in the ordered list" fallback needs an explicit, tested notion of "ordered" — settle what it means and pin it with a test; do not assume `activities()` order is stable.
