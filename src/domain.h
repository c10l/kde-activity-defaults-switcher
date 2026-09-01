#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include <optional>

namespace ads {

// An Application is the chosen program for a binding, identified by its
// .desktop file (KDE service storageId), e.g. "org.kde.falkon.desktop".
using Application = QString;

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
};

// Every component, in a stable order.
QList<Component> allComponents();

// An Activity is the workspace unit this tool keys profiles by. KDE
// identifies activities by UUID; the name is user-renamable, so profiles are
// keyed by id and the name is carried alongside for the baseline selection.
struct Activity {
    QString id;
    QString name;
};

inline bool operator==(const Activity &a, const Activity &b)
{
    return a.id == b.id;
}

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

// The Resolver turns the current and baseline activity profiles plus the
// current system defaults into the set of decisions (component -> application)
// to apply. It is a pure function: no KDE, D-Bus, or filesystem access.
class Resolver
{
public:
    // The baseline activity is the activity named "Default" when present,
    // otherwise the first activity in the ordered list. Empty when the list
    // is empty.
    static std::optional<Activity> selectBaselineActivity(const QList<Activity> &orderedActivities);

    // Resolution order per component: the current activity's binding, then the
    // baseline activity's binding, then capture the current default
    // application. A component with none of those is absent from the result.
    // `profiles` is keyed by activity id.
    QMap<Component, Application> resolve(
        const Activity &currentActivity,
        const std::optional<Activity> &baselineActivity,
        const QMap<QString, Profile> &profiles,
        const QMap<Component, Application> &currentDefaults) const;
};

} // namespace ads
