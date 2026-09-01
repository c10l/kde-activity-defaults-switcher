# 02: Activity identity (id vs name)

**What to build:** Settle how an Activity is identified and update the domain model. KDE identifies activities by UUID; names are user-renamable. The current model keys profiles by name, so renaming an activity orphans its profile.

Decision to implement: `Activity` carries both id (UUID) and name; profiles are keyed by id; the baseline activity is still selected by name ("Default", else first) and returned with its id.

**Blocked by:** 01 (Resolver + domain model).

**Status:** ready-for-agent

- [ ] Activity type carries id and name.
- [ ] Profiles keyed by activity id.
- [ ] `selectBaselineActivity` still matches on name and returns the full Activity (id + name).
- [ ] Resolver and its tests updated to the new shape; tests pass.
- [ ] While updating the tests, add the missing mixed-case assertion: components without a decision are absent from a non-empty result.
