# 01: Resolver + domain model

**What to build:** The pure resolver and the domain types (Activity, Component, Binding, Profile, Application). Given the current activity, the baseline activity, the profiles, and the current default applications, it returns the decisions (component → application) to apply, following the resolution order in the repository glossary. No KDE, D-Bus, or filesystem I/O.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

- [ ] Domain types use the glossary vocabulary (Activity, Component, Binding, Profile, Application, Baseline activity).
- [ ] Resolution order implemented: activity binding → baseline activity binding → capture current default application.
- [ ] Baseline activity selected as the "Default" activity when present, else the first activity in the ordered list.
- [ ] Per-component independence: resolving one component never affects another.
- [ ] Unknown or empty activity handled without crashing.
- [ ] Pure function: no KDE/D-Bus/filesystem access.
- [ ] Unit tests written TDD-first cover every resolution case; the test command passes.
