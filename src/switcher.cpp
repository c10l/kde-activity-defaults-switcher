// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "switcher.h"

namespace ads {

Switcher::Switcher(std::unique_ptr<Resolver> resolver,
                   std::unique_ptr<Writer> writer,
                   std::unique_ptr<ProfileStore> store)
    : m_resolver(resolver ? std::move(resolver) : std::make_unique<Resolver>())
    , m_writer(writer ? std::move(writer) : std::make_unique<Writer>())
    , m_store(store ? std::move(store) : std::make_unique<ProfileStore>())
{
}

int Switcher::switchActivity(const Activity &currentActivity,
                             const std::optional<Activity> &baselineActivity,
                             const QMap<ActivityId, Profile> &profiles,
                             const QMap<Component, Application> &currentDefaults)
{
    return applyAndRemember(currentActivity, baselineActivity, profiles, currentDefaults);
}

int Switcher::captureUserEdits(const Activity &currentActivity,
                               const std::optional<Activity> &baselineActivity,
                               const QMap<ActivityId, Profile> &profiles,
                               const QMap<Component, Application> &currentDefaults)
{
    QMap<ActivityId, Profile> updated = profiles;
    bool recorded = false;
    for (auto it = currentDefaults.constBegin(); it != currentDefaults.constEnd(); ++it) {
        const Component component = it.key();
        if (it.value() == m_lastDefaults.value(component)) {
            continue; // not a change
        }
        if (it.value().isEmpty()) {
            continue; // a removed default: do not record; resolution re-applies the profile
        }
        updated[currentActivity.id].setBinding(component, it.value());
        recorded = true;
    }
    if (recorded) {
        m_store->save(updated);
    }
    return applyAndRemember(currentActivity, baselineActivity, updated, currentDefaults);
}

int Switcher::applyAndRemember(const Activity &currentActivity,
                               const std::optional<Activity> &baselineActivity,
                               const QMap<ActivityId, Profile> &profiles,
                               const QMap<Component, Application> &currentDefaults)
{
    const QMap<Component, Application> decisions =
        m_resolver->resolve(currentActivity, baselineActivity, profiles, currentDefaults);
    const int written = m_writer->apply(decisions, currentDefaults);

    // Remember the state the system should now hold: every decision (the
    // writer made the system match it, or it already matched), plus any
    // current default the resolver left untouched. Decisions are recorded even
    // for components absent from currentDefaults (e.g. a fresh setup where
    // mimeapps.list had no entry yet and the daemon's own write created it) -
    // otherwise the next file-watcher event would mistake that write for a
    // user edit and record a spurious binding.
    m_lastDefaults = currentDefaults;
    for (auto it = decisions.constBegin(); it != decisions.constEnd(); ++it) {
        m_lastDefaults.insert(it.key(), it.value());
    }
    return written;
}

} // namespace ads
