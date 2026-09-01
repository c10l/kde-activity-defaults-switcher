#pragma once

#include "domain.h"
#include "store.h"
#include "writer.h"

#include <memory>

namespace ads {

// The Switcher is the kded shell wiring: it resolves the current state through
// the resolver, applies the result via the writer, and captures user edits
// (System Settings changes) into the current activity's profile via the
// profile store. It knows nothing of KDE, D-Bus, or the filesystem - the kded
// module feeds it the current state.
class Switcher
{
public:
    explicit Switcher(std::unique_ptr<Resolver> resolver = {},
                      std::unique_ptr<Writer> writer = {},
                      std::unique_ptr<ProfileStore> store = {});

    // Resolves every component for `currentActivity` and applies the result,
    // returning the number of components written. Components whose decision
    // already equals the current default are not rewritten (no churn).
    int switchActivity(const Activity &currentActivity,
                       const std::optional<Activity> &baselineActivity,
                       const QMap<ActivityId, Profile> &profiles,
                       const QMap<Component, Application> &currentDefaults);

    // Called when mimeapps.list changed (a System Settings edit): records a
    // binding for every component whose current default differs from the one
    // the switcher last ensured, persists the updated profile, then resolves
    // and applies like switchActivity. Empty current defaults (a removed
    // default) are not recorded - resolution re-applies the profile, so the
    // system heals back to the activity's bindings.
    int captureUserEdits(const Activity &currentActivity,
                         const std::optional<Activity> &baselineActivity,
                         const QMap<ActivityId, Profile> &profiles,
                         const QMap<Component, Application> &currentDefaults);

private:
    int applyAndRemember(const Activity &currentActivity,
                         const std::optional<Activity> &baselineActivity,
                         const QMap<ActivityId, Profile> &profiles,
                         const QMap<Component, Application> &currentDefaults);

    std::unique_ptr<Resolver> m_resolver;
    std::unique_ptr<Writer> m_writer;
    std::unique_ptr<ProfileStore> m_store;
    // The defaults the system should hold after the last apply; used to tell a
    // genuine user edit apart from the daemon's own writes.
    QMap<Component, Application> m_lastDefaults;
};

} // namespace ads
