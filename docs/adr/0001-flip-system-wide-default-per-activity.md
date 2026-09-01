# Flip the system-wide default per activity

The switcher works by writing the system-wide default application — `mimeapps.list` for most components (via `KApplicationTrader::setPreferredService`) and `kdeglobals` for the terminal component — whenever the current activity changes or a binding is edited, so the global default always equals the current activity's profile.

## Considered Options

- **Per-app interception** (per-app `xdg-open` shims, or an xdg-desktop-portal override): rejected — not app-agnostic; Flatpak apps only go through the portal, native apps don't, and there is no per-app hook that covers both.
- **A custom xdg-desktop-portal backend**: rejected — heavy for a small feature, and it only covers portal-aware apps.
- **Flip the system-wide default** (chosen): every app already resolves through `mimeapps.list`/`kdeglobals`, so one write point covers native and Flatpak alike.

## Consequences

- The global state genuinely changes: while on an activity with bindings, System Settings shows that activity's applications as the system defaults.
- The user's own default is preserved by capturing it into the profile, not by leaving `mimeapps.list` untouched.
- "Current activity" means the focused one; with two activities on two screens, the focused screen wins.
