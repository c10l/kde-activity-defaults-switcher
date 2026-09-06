# KCM writes the profile store directly; the daemon watches it and applies live

The System Settings KCM (view/edit/reset per-activity profiles — [ticket 07](../../.scratch/activity-defaults-switcher/issues/07-kcm-component.md)) edits the profile store by writing `activity-defaults-switcherrc` directly, reusing the same `ProfileStore` and domain code the daemon links. The daemon additionally watches that file and re-resolves/applies when it changes, so an edit to the current activity's profile flips the live system defaults immediately. The daemon remains the only component that flips defaults; the KCM stays a dumb editor. The daemon-side change is tracked as [ticket 16](../../.scratch/activity-defaults-switcher/issues/16-daemon-watches-profile-store.md).

## Considered Options

- **KCM writes the store directly; daemon watches it and applies** (chosen): reuses the store as the single source of truth and extends the daemon's existing file-watching pattern (`mimeapps.list`, `kdeglobals`). Smallest change that gives live apply while the daemon still owns the flip.
- **KCM applies the flip itself** (rejected): the KCM would run resolver + writer to flip the current activity's default. Works without the daemon, but duplicates the flip logic in a second process where the two copies can drift.
- **KCM talks to the daemon over D-Bus** (rejected for now — revisit candidate): the daemon becomes the sole reader/writer of the store. Cleanest ownership, but costs a new D-Bus interface and makes the KCM unable to save when the daemon is not running.
- **KCM writes the store only, no live apply** (rejected): edits take effect on the next activity switch. Simplest, but surprising — nothing visibly changes when editing the current activity.

## Consequences

- The store file remains the single source of truth and the only interface between the KCM and the daemon; no new D-Bus surface.
- The daemon gains a watch on `activity-defaults-switcherrc` (alongside `mimeapps.list` and `kdeglobals`), so a KCM save is applied live.
- Two processes write the same KConfig file. Writes are user-triggered, single-user, and KConfig writes atomically (last-write-wins), so the race is low-risk in practice — but it exists.
- The KCM requires the `ads-store`/`ads-domain` libraries the daemon already links; they become a shared dependency rather than daemon-only.

## Revisit

This decision is provisional and should be revisited if any of the following appear:

- the two-writer race on the store file materialises (e.g. a KCM save and a daemon capture interleave);
- the KCM needs to work when the daemon is not running;
- a further writer to the store is added and ownership needs tightening.

The D-Bus option (daemon as sole reader/writer) is the fallback to reconsider in those cases.
