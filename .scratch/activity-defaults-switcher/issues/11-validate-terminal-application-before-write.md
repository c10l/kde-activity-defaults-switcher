# 11: Validate the terminal default's Application before writing kdeglobals

**What to build:** In `writeTerminal()` (`src/writer.cpp`), handle a storage id that resolves to no valid desktop entry. Today the fallback `KService` is constructed from a desktop file derived from the storage id, so for an invalid id `service->exec()` is empty and an empty `TerminalApplication=` is written to `kdeglobals`, which can break "Open Terminal" (ADR-0001).

**Blocked by:** 06 (Terminal component + baseline fallback) — the terminal write path exists.

**Status:** done

- [x] A storage id that resolves to no desktop entry does not write an empty `TerminalApplication` (skip the write, or fall back safely).
- [x] Valid storage ids still write `TerminalService` and a non-empty `TerminalApplication`.
- [x] Writer tests against a temporary config directory pin both behaviours (extend `tests/tst_writer.cpp`).

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `src/writer.cpp` — `writeTerminal()`.
- `docs/adr/0001-flip-system-wide-default-per-activity.md` — the kdeglobals write mechanism.
- `tests/tst_writer.cpp` — existing terminal write/no-op tests.
- `CONTEXT.md` — vocabulary.
