# Activity Defaults Switcher — Spec

## Problem Statement

Default applications in KDE are system-wide: switching Activities (e.g. Work vs Default) does not change which web browser, email client, or text editor opens. A user who wants a work browser on one Activity and a personal one on another must change the default manually every time they switch.

## Solution

An always-on KDE Plasma 6 daemon (a kded6 module) that, whenever the current Activity changes or the user edits a default application in System Settings, resolves the current Activity's profile and writes the system-wide default for each component to match — so every application, native or Flatpak, opens with the Activity-appropriate program.

## User Stories

1. As a user, I want my web browser to differ between Activities, so that links open in the right browser per Activity.
2. As a user, I want my email client to follow the Activity, so that mailto links open the right app per Activity.
3. As a user, I want my text editor to follow the Activity, so that files open in the right editor per Activity.
4. As a user, I want my file manager, image viewer, music player, video player, PDF viewer, calendar, map, dialer, and archive manager to each follow the Activity, so that opening any file type uses the right app per Activity.
5. As a user, I want the terminal emulator to follow the Activity, so that "Open Terminal" uses the right terminal per Activity.
6. As a user, I want changing a default in System Settings while on an Activity to be captured into that Activity's profile, so that I never have to edit a config file.
7. As a user, I want an Activity with no binding for a component to fall back to the baseline activity's binding, so that I only override what actually differs.
8. As a user, I want the baseline activity to be the activity named "Default" (or the first activity when none is named "Default"), so that a fresh KDE setup works with zero configuration.
9. As a user, I want the daemon to start automatically at login, so the behaviour is always on without manual steps.
10. As a user, I want bindings to persist across restarts and survive switching away and back, so that my choices stick.
11. As a user, I want the behaviour to apply to Flatpak applications as well as native ones, so that the browser choice affects every app.
12. As a user, I want nothing to be rewritten when an Activity's bindings already match the current defaults, so there is no churn or surprise.
13. As a user, I want a component with no binding in my Activity or the baseline activity to capture the current default application, so that the baseline is established from the system's existing state.
14. As a maintainer, I want the resolution rules covered by automated tests, so that the domain logic is trustworthy and regression-proof.

## Implementation Decisions

- Ship as a **kded6 module** (auto-loaded), not a systemd service or shell script; event-driven rather than polling.
- The **resolver** is a pure function; all domain rules live in it (see the repository glossary).
- **Resolution order**: current activity's binding → baseline activity's binding → capture the current default application.
- **Baseline activity selection**: the activity named "Default"; if none, the first activity in the ordered activity list.
- **Components**: the 13 component-chooser categories — browser, email, filemanager, texteditor, calendar, geo, imageviewer, musicplayer, pdfviewer, tel, videoplayer, archivemanager, terminal — each mapped to its mime keys. Terminal is the exception: stored in kdeglobals, not mimeapps.
- **Writing**: mime components via `KApplicationTrader::setPreferredService`; terminal via `kdeglobals` `TerminalService`/`TerminalApplication`. (ADR-0001.)
- **Profile store**: a KConfig file, one group per component, activity name as key, storageId as value.
- **Change detection**: `KActivities::Consumer` for activity changes; `KConfigWatcher` on mimeapps.list and kdeglobals for System Settings edits.
- **Language/build**: C++/Qt6/KF6, CMake + ECM.
- Use the repository glossary vocabulary in all code and tests.

## Testing Decisions

- A good test asserts **external behaviour, not implementation**: resolver tests assert inputs→decisions; writer tests assert the resulting config entries; shell tests assert "event → resolve → apply" wiring.
- The **resolver** is the primary test target (exhaustive, injected fakes, no KDE calls).
- The **writer** is tested against a temporary config directory.
- The **kded shell** is tested with a fake resolver/writer injected.
- TDD: red → green → refactor; no code without a failing test first.
- Seams (agreed): resolver (highest), writer, kded shell.

## Out of Scope

- The System Settings **KCM** (view/edit/reset profiles) — see ticket 05, pending a future grill session.
- Disambiguating Activities across multiple screens beyond "the focused Activity".
- Per-mime-type overrides below the component level (a component always maps to its full mime list).
- Distribution packaging (deb, etc.).

## Further Notes

- Domain vocabulary lives in the repository glossary; the mechanism rationale is ADR-0001.
- Tickets are delineated under the issues directory, in dependency order, for independent agents.
