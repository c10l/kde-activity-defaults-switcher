# Activity Defaults Switcher

A KDE Plasma 6 daemon — plus a System Settings module to view, edit, and reset per-activity profiles — that switches the system-wide default application for each component to match the current KDE Activity. This context holds the domain vocabulary and decisions for its native-KDE rebuild.

## Language

**Activity**:
A KDE Activity — the workspace unit this tool keys profiles by.

**Component**:
A category of default application the user chooses in System Settings (web browser, email client, file manager, text editor, and so on).
_Avoid_: application type, handler kind

**Binding**:
A single assignment of one application to one component within an activity's profile.
_Avoid_: rule, mapping, override

**Fallback binding**:
A binding the baseline activity supplies for a component the current activity has no binding for.
_Avoid_: inherited binding, default binding

**Explicit binding**:
A binding set directly on an activity's profile, rather than supplied by the baseline.
_Avoid_: override, direct binding, local binding

**Profile**:
The full set of bindings for one activity.
_Avoid_: settings, map

**Application**:
The chosen program for a binding, identified by its .desktop file (KDE service storageId).
_Avoid_: handler, service, program

**Baseline activity**:
The activity whose profile supplies fallback bindings when the current activity has no binding for a component. By convention, the activity named "Default"; if none exists, the first activity in the activity list.
_Avoid_: default activity, fallback activity

## How we talk about the code

**Interface**:
A set of methods a piece of code provides. Code talks to the outside world (KDE, the profile file, the application list) only through interfaces, so a test can pass in a fake.
_Avoid_: seam

**Fake**:
A test stand-in for an interface — a fake activity reader returns a fixed list instead of asking KDE.

**Dependency injection**:
Passing the outside-world pieces (activity reader, profile store, application lister) into an object as constructor arguments, instead of having the object reach out to them itself.

**Pure function**:
A function whose result depends only on its arguments — no KDE, D-Bus, or filesystem access. The resolver is one.

**Temporary directory**:
A throwaway directory a test points at instead of the real config, so tests never write to the user's real configuration.

## Words we do not use

- **seam** — say **interface**. "Seam" is vocabulary for retrofitting testability into legacy code; this is new code.
- **frontier** — internal vocabulary from an interview method; say "the questions ready to be answered".
- **grilling** — the name of a skill file, not a way to talk to a person; say "interview", or just ask directly.
