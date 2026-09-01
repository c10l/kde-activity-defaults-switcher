# 03: Writer (apply step)

**What to build:** The writer seam: given resolved decisions and the current defaults, write each mime component's application via `KApplicationTrader::setPreferredService`. The terminal component is out of scope (ticket 06).

**Blocked by:** 01 (Resolver + domain model).

**Status:** ready-for-agent

- [ ] `apply()` writes each component's application to the XDG mime config.
- [ ] Skips no-ops: a component whose decision already equals the current default is not rewritten.
- [ ] Tested against a temporary config directory, not the live system.
- [ ] No D-Bus/activity logic; only the write path.
