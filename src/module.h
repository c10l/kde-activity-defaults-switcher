// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include <KDEDModule>

#include <QFileSystemWatcher>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <memory>

#include "activity_backend.h"
#include "store.h"
#include "switcher.h"

namespace ads {

// The auto-loaded kded6 module: it observes the current KDE Activity (through
// an ActivityBackend), the config files the switcher writes (mimeapps.list and
// kdeglobals) and the profile store (activity-defaults-switcherrc), and drives
// the Switcher (resolver + writer + profile store). If the backend is running,
// an activity change resolves and applies; and a change to mimeapps.list or
// kdeglobals (a System Settings edit) is first captured into the current
// activity's profile, then resolved and applied. The profile store is also
// watched because the KCM (ticket 07) edits it directly: a change there that is
// not the daemon's own capture is re-resolved and applied so a KCM save to the
// current activity flips the live defaults. This is thin glue - all decision
// logic lives in the resolver, the write path in the writer, the capture rules
// in the Switcher, and persistence in the ProfileStore.
//
// The backend and the resolver/writer/store are constructor-injectable so the
// shell glue (signal -> refresh/capture, running-state gating, the coalescing
// timer) is unit-tested with fakes and never touches KDE, D-Bus, or the live
// filesystem.
class ActivityDefaultsSwitcherModule : public KDEDModule
{
    Q_OBJECT

public:
    // kded6 plugin constructor: runs with the real KActivities-backed backend.
    explicit ActivityDefaultsSwitcherModule(QObject *parent, const QVariantList &args);

    // Testable shell constructor: an injected ActivityBackend replaces the real
    // KActivities backend, and injected resolver/writer/store are threaded into
    // the Switcher so callers observe the glue without real KDE IO.
    ActivityDefaultsSwitcherModule(QObject *parent, std::unique_ptr<ActivityBackend> backend,
                                   std::unique_ptr<Resolver> resolver = {},
                                   std::unique_ptr<Writer> writer = {},
                                   std::unique_ptr<ProfileStore> store = {});

private:
    void init(std::unique_ptr<ActivityBackend> backend);
    QString currentName(const QString &id, const QList<Activity> &activities) const;
    std::optional<Activity> selectedBaseline(const QList<Activity> &activities) const;
    bool isRunning() const;
    void refresh();
    void onDefaultsChanged();
    // Routes a watched file-change to the capture or re-apply path (a System
    // Settings edit to mimeapps.list/kdeglobals is captured; a profile-store
    // change is re-applied), coalescing both through the one edit timer.
    void markEditPending(const QString &path);
    // The coalesced dispatch after the edit window settles.
    void onEditsSettled();
    void ensureConfigWatched(const QString &path);
    void ensureConfigDirectoryWatched();
    bool isStorePath(const QString &path) const;

    std::unique_ptr<ActivityBackend> m_backend;
    QFileSystemWatcher m_watcher;
    QTimer m_editTimer; // coalesces rapid file-change events
    ProfileStore m_store; // loads profiles to feed the switcher
    Switcher m_switcher;
    QString m_configDir; // the config directory watcher observes
    QString m_configDirWatch; // the path currently watched for config changes
    QString m_mimeappsPath;
    QString m_kdeglobalsPath;
    QString m_storePath;
    // Edits coalesced within the current edit window, per destination path:
    // a change to mimeapps.list/kdeglobals is captured, a change to the profile
    // store is re-applied.
    bool m_capturePending = false;
    bool m_reapplyPending = false;

    Q_DISABLE_COPY(ActivityDefaultsSwitcherModule)
};

} // namespace ads
