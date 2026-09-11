// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <optional>

namespace ads {

// The meta-object for the namespace itself, so the BindingSource enum can be
// exposed to QML (qmlRegisterUncreatableMetaObject) for the KCM. It carries no
// behaviour and leaves the domain logic pure.
Q_NAMESPACE

// An Application is the chosen program for a binding, identified by its
// .desktop file (KDE service storageId), e.g. "org.kde.falkon.desktop".
using Application = QString;

// Activities are keyed by their KDE UUID. The alias makes "keyed by id"
// explicit in the type system.
using ActivityId = QString;

// A Component is a category of default application the user chooses in
// System Settings.
enum class Component {
    Browser,
    Email,
    FileManager,
    TextEditor,
    Calendar,
    Geo,
    ImageViewer,
    MusicPlayer,
    PdfViewer,
    Tel,
    VideoPlayer,
    ArchiveManager,
    Terminal,
    // Never a real component: a trailing count sentinel one past the last real
    // member. Its ordinal therefore equals the number of real components, which
    // lets allComponents() derive the member set from the contiguous range
    // [Browser, Count) so the two cannot drift (ticket 19). Always add a new
    // component above this line, never below it.
    Count,
};

// Every component, in a stable order. Derived from the enum's contiguous
// range below Count (see domain.cpp), so a component added to the enum is
// always returned here - allComponents() cannot silently miss one.
QList<Component> allComponents();

// The stable config key for a component (used as the profile store's group
// name). Keys are unique and never change once written.
QString componentKey(Component component);

// An Activity is the workspace unit this tool keys profiles by. KDE
// identifies activities by UUID; the name is user-renamable, so profiles are
// keyed by id and the name is carried alongside for the baseline selection.
struct Activity {
    ActivityId id;
    QString name;
};

// The canonical order of activities: by name (case-insensitive), ties broken
// by id. KActivities::Consumer::activities() documents no ordering guarantee,
// so the baseline fallback's "first activity" follows this deterministic
// order rather than whatever order the consumer happens to return.
QList<Activity> orderedActivityList(const QList<Activity> &activities);

// A Binding is a single assignment of one application to one component within
// an activity's profile.
struct Binding {
    Component component;
    Application application;
};

// A Profile is the full set of bindings for one activity.
class Profile
{
public:
    void setBinding(Component component, const Application &application);
    void removeBinding(Component component);
    std::optional<Application> bindingFor(Component component) const;
    QList<Binding> bindings() const;
    bool hasBinding(Component component) const;
    bool isEmpty() const;

private:
    QMap<Component, Application> m_bindings;
};

// How the KCM's view derived a component's effective Application: set on the
// activity itself (Explicit), supplied by the baseline activity (Fallback), or
// absent (Unset). The KCM never captures a current default (ticket 04).
enum class BindingSource {
    Explicit,
    Fallback,
    Unset,
};
Q_ENUM_NS(BindingSource)

// The KCM's per-component view of a selected Profile against a baseline
// Profile. `application` is nullopt when Unset; `baselineApplication` is
// nullopt when the baseline has no binding for the component.
struct EffectiveBinding {
    std::optional<Application> application;
    BindingSource source = BindingSource::Unset;
    std::optional<Application> baselineApplication;
};

// Pure: every component's effective binding for `selectedProfile`, using
// `baselineProfile`'s binding as a fallback and Unset when neither has one.
// Unlike Resolver::resolve it never captures a current default and reports the
// source of each result. Every component is present in the result.
QMap<Component, EffectiveBinding> effectiveBindings(const Profile &selectedProfile,
                                                    const Profile &baselineProfile);

// The Resolver turns the current and baseline activity profiles plus the
// current system defaults into the set of decisions (component -> application)
// to apply. It is a pure function: no KDE, D-Bus, or filesystem access.
class Resolver
{
public:
    virtual ~Resolver() = default;

    // The baseline activity is the activity named "Default" when present,
    // otherwise the first activity in the canonical order (see
    // orderedActivityList). Empty when the list is empty.
    static std::optional<Activity> selectBaselineActivity(const QList<Activity> &activities);

    // Resolution order per component: the current activity's binding, then the
    // baseline activity's binding, then capture the current default
    // application. A component with none of those is absent from the result.
    // `profiles` is keyed by activity id.
    virtual QMap<Component, Application>
    resolve(const Activity &currentActivity, const std::optional<Activity> &baselineActivity,
            const QMap<ActivityId, Profile> &profiles,
            const QMap<Component, Application> &currentDefaults) const;
};

} // namespace ads
