# 04: Effective-app computation and baseline marking

Type: grilling
Blocked by: 03

## Question

How is the KCM's "effective app" computed, and how are fallback / baseline / unset represented?

- The KCM's effective app per (activity, component) is: explicit binding → else the baseline's binding (marked fallback) → else unset. `Resolver::resolve` adds a third step — capture the current default — which the KCM must *not* do. Decide: a new pure function in `ads-domain`, a parameterized resolver, or KCM-local logic?
- How "explicit" vs "fallback" vs "unset" and "this is the baseline activity" are represented in the backend model so the QML can distinguish them visually.

References: `src/domain.*` (`Resolver::resolve`), ticket 07 ACs 2–3, `CONTEXT.md` (fallback binding / explicit binding / baseline activity).
