# 04: Effective-app computation and baseline marking

Type: grilling
Status: resolved
Blocked by: 03

## Question

How is the KCM's "effective app" computed, and how are fallback / baseline / unset represented?

- The KCM's effective app per (activity, component) is: explicit binding → else the baseline's binding (marked fallback) → else unset. `Resolver::resolve` adds a third step — capture the current default — which the KCM must *not* do. Decide: a new pure function in `ads-domain`, a parameterized resolver, or KCM-local logic?
- How "explicit" vs "fallback" vs "unset" and "this is the baseline activity" are represented in the backend model so the QML can distinguish them visually.

References: `src/domain.*` (`Resolver::resolve`), ticket 07 ACs 2–3, `CONTEXT.md` (fallback binding / explicit binding / baseline activity).

## Answer

Agreed with the maintainer. Two decisions:

**1. Effective-app computation: a new pure function in `ads-domain`.** Not a parameterized `Resolver::resolve`, not KCM-local. `Resolver::resolve` is the daemon's resolution order — explicit binding → fallback binding → *capture the current default* — and it returns only `QMap<Component, Application>` with no source. The KCM's resolution order differs in two ways: its third step is *unset*, not capture, and it needs the source of each result to render explicit vs fallback vs unset. A flag on `resolve` could not supply the source and would entangle two different operations in one signature; KCM-local logic would contradict ticket 03 decision 4 and lose the shared-library reuse. A separate function keeps the KCM's configured-only view (explicit → fallback → unset, never capture) as a first-class pure function, exactly mirroring how the resolver is the daemon's pure core.

```cpp
enum class BindingSource { Explicit, Fallback, Unset };

struct EffectiveBinding {
    std::optional<Application> application;          // nullopt when Unset
    BindingSource source;
    std::optional<Application> baselineApplication;  // nullopt when the baseline has none
};

// Pure: the KCM's per-component view of a selected profile against a baseline
// profile (pass an empty Profile when there is no baseline activity). Every
// component is present: explicit binding, else the baseline binding as fallback,
// else unset.
QMap<Component, EffectiveBinding> effectiveBindings(const Profile &selectedProfile,
                                                    const Profile &baselineProfile);
```

**2. Encoding in the backend model.**

- Binding source: the `BindingSource` enum (three values), `Q_ENUM_NS`-exposed and surfaced as an integer role on each component row.
- Effective app: the row's effective-app role — the storageId string, empty when Unset.
- Baseline app: a second role — the baseline's storageId, empty when the baseline has no binding for that component (feeds the fallback label and ticket 05's "Inherit (baseline: …)" entry).
- Baseline activity: as decided in 03, the `baselineActivityId` property — QML compares it with `selectedActivityId` and badges that selector entry. Baseline-ness is an *activity* property, so it stays a property, not a per-row flag.
- Current activity: `currentActivityId` only defaults the selector's initial selection; it is **not** given a distinct visual badge in v1. Ticket 07's acceptance criteria require only the baseline marked, and the KCM is a snapshot editor with no live refresh, so a "current" badge would imply liveness the KCM does not have. Reversible later — the property already exists.
