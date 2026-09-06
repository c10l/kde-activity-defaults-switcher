// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "module.h"

#include <KPluginFactory>
#include <QFile>
#include <QStandardPaths>

#include "kactivities_backend.h"
#include "writer.h"

namespace ads {

// kded6 plugin constructor: runs with the real KActivities-backed backend.
ActivityDefaultsSwitcherModule::ActivityDefaultsSwitcherModule(QObject *parent,
                                                               const QVariantList &args)
    : KDEDModule(parent), m_switcher(std::make_unique<Resolver>(), std::make_unique<Writer>(),
                                     std::make_unique<ProfileStore>())
{
    Q_UNUSED(args);
    init(std::make_unique<KActivitiesActivityBackend>(this));
}

// Testable shell constructor: an injected backend + resolver/writer/store.
ActivityDefaultsSwitcherModule::ActivityDefaultsSwitcherModule(
    QObject *parent, std::unique_ptr<ActivityBackend> backend, std::unique_ptr<Resolver> resolver,
    std::unique_ptr<Writer> writer, std::unique_ptr<ProfileStore> store)
    : KDEDModule(parent), m_switcher(std::move(resolver), std::move(writer), std::move(store))
{
    init(std::move(backend));
}

void ActivityDefaultsSwitcherModule::init(std::unique_ptr<ActivityBackend> backend)
{
    m_backend = std::move(backend);

    // Watch mimeapps.list and kdeglobals (and their directory, to notice a
    // file's creation) so System Settings edits reach the switcher's capture
    // path. A QFileSystemWatcher is used instead of KConfigWatcher because
    // KConfigWatcher only fires for writes done with the KConfig Notify flag,
    // and both the System Settings component chooser and our writer write these
    // files without it (KApplicationTrader::setPreferredService and the
    // kdeglobals write) - so it would never observe a real edit.
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    m_mimeappsPath = configDir + QStringLiteral("/mimeapps.list");
    m_kdeglobalsPath = configDir + QStringLiteral("/kdeglobals");
    if (!configDir.isEmpty() && QFile::exists(configDir)) {
        m_watcher.addPath(configDir);
    }
    ensureConfigWatched(m_mimeappsPath);
    ensureConfigWatched(m_kdeglobalsPath);

    // File-change events can arrive while a config file is still being written
    // (KConfig truncates and rewrites it), so edits are coalesced and only
    // processed once the file has settled.
    m_editTimer.setSingleShot(true);
    m_editTimer.setInterval(100);
    connect(&m_editTimer, &QTimer::timeout, this, [this]() { onDefaultsChanged(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) { m_editTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        // A directory event can mean a watched config file was created (or
        // recreated after being deleted). Edits to an existing file arrive via
        // fileChanged; the rest of the directory noise (KDE writes plenty of
        // config files) is ignored.
        bool appeared = false;
        for (const QString &path : {m_mimeappsPath, m_kdeglobalsPath}) {
            if (!m_watcher.files().contains(path) && QFile::exists(path)) {
                m_watcher.addPath(path);
                appeared = true;
            }
        }
        if (appeared) {
            m_editTimer.start();
        }
    });

    connect(m_backend.get(), &ActivityBackend::currentActivityChanged, this,
            [this](const QString &) { refresh(); });
    connect(m_backend.get(), &ActivityBackend::activitiesChanged, this, [this]() { refresh(); });
    connect(m_backend.get(), &ActivityBackend::activitiesRenamed, this, [this]() { refresh(); });
    connect(m_backend.get(), &ActivityBackend::runningChanged, this, [this](bool running) {
        if (running) {
            refresh();
        }
    });

    // Apply once at startup, in case the service is already running.
    if (isRunning()) {
        refresh();
    }
}

bool ActivityDefaultsSwitcherModule::isRunning() const
{
    return m_backend && m_backend->isRunning();
}

void ActivityDefaultsSwitcherModule::refresh()
{
    if (!isRunning()) {
        return;
    }

    const QString currentId = m_backend->currentActivityId();
    const QList<Activity> activities = m_backend->allActivities();
    const Activity current{currentId, currentName(currentId, activities)};
    const std::optional<Activity> baseline = selectedBaseline(activities);

    m_switcher.switchActivity(current, baseline, m_store.load(), readCurrentDefaults());
}

void ActivityDefaultsSwitcherModule::onDefaultsChanged()
{
    if (!isRunning()) {
        return;
    }

    const QString currentId = m_backend->currentActivityId();
    const QList<Activity> activities = m_backend->allActivities();
    const Activity current{currentId, currentName(currentId, activities)};
    const std::optional<Activity> baseline = selectedBaseline(activities);

    m_switcher.captureUserEdits(current, baseline, m_store.load(), readCurrentDefaults());
}

QString ActivityDefaultsSwitcherModule::currentName(const QString &id,
                                                    const QList<Activity> &activities) const
{
    for (const Activity &activity : activities) {
        if (activity.id == id) {
            return activity.name;
        }
    }
    // The current id may reference an activity the aggregate list does not yet
    // report (e.g. transient state); fall back to the backend's own lookup so a
    // name is still carried where the backend knows one.
    if (m_backend) {
        return m_backend->activityName(id);
    }
    return QString();
}

// Picks the baseline activity from the backend's activity list, ignoring any
// activity whose name is not yet available (a freshly created KActivities::Info
// can report an empty name before it synchronizes). Such a transient empty name
// would sort first in the canonical order and, absent an activity literally
// named "Default", would otherwise be mis-selected as the fallback baseline.
// A nameless activity is never a valid baseline candidate: its eventual
// synchronized name is reported via activitiesRenamed and re-resolves then.
std::optional<Activity>
ActivityDefaultsSwitcherModule::selectedBaseline(const QList<Activity> &activities) const
{
    QList<Activity> named;
    for (const Activity &activity : activities) {
        if (!activity.name.isEmpty()) {
            named.append(activity);
        }
    }
    return Resolver::selectBaselineActivity(named);
}

void ActivityDefaultsSwitcherModule::ensureConfigWatched(const QString &path)
{
    if (QFile::exists(path) && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }
}

K_PLUGIN_CLASS_WITH_JSON(ActivityDefaultsSwitcherModule, "module.json")

} // namespace ads

#include "module.moc"
