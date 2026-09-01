# Activity Defaults Switcher

A KDE Plasma 6 daemon that switches the system-wide default application for each component to match the current KDE Activity. This context holds the domain vocabulary and decisions for its native-KDE rebuild.

## Language

**Activity**:
A KDE Activity — the workspace unit this tool keys profiles by.

**Component**:
A category of default application the user chooses in System Settings (web browser, email client, file manager, text editor, and so on).
_Avoid_: application type, handler kind

**Binding**:
A single assignment of one application to one component within an activity's profile.
_Avoid_: rule, mapping, override

**Profile**:
The full set of bindings for one activity.
_Avoid_: settings, map

**Application**:
The chosen program for a binding, identified by its .desktop file (KDE service storageId).
_Avoid_: handler, service, program

**Baseline activity**:
The activity whose profile supplies fallback bindings when the current activity has no binding for a component. By convention, the activity named "Default"; if none exists, the first activity in the activity list.
_Avoid_: default activity, fallback activity
