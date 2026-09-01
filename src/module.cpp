#include "module.h"

#include <KPluginFactory>
#include <QFile>
#include <QStandardPaths>

#include "writer.h"

namespace ads {

ActivityDefaultsSwitcherModule::ActivityDefaultsSwitcherModule(QObject *parent, const QVariantList &args)
    : KDEDModule(parent)
    , m_consumer(new KActivities::Consumer(this))
    , m_switcher(std::make_unique<Resolver>(), std::make_unique<Writer>(), std::make_unique<ProfileStore>())
{
    Q_UNUSED(args);

    // Watch mimeapps.list (and its directory, to notice the file's creation)
    // so System Settings edits reach the switcher's capture path. A
    // QFileSystemWatcher is used instead of KConfigWatcher because
    // KConfigWatcher only fires for writes done with the KConfig Notify flag,
    // and both the System Settings component chooser and our writer write
    // mimeapps.list without it (KApplicationTrader::setPreferredService) - so
    // it would never observe a real edit.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    m_mimeappsPath = configDir + QStringLiteral("/mimeapps.list");
    if (!configDir.isEmpty() && QFile::exists(configDir)) {
        m_watcher.addPath(configDir);
    }
    ensureMimeappsWatched();
    // File-change events can arrive while mimeapps.list is still being
    // written (KConfig truncates and rewrites it), so edits are coalesced and
    // only processed once the file has settled.
    m_editTimer.setSingleShot(true);
    m_editTimer.setInterval(100);
    connect(&m_editTimer, &QTimer::timeout, this, [this]() {
        onMimeappsChanged();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        m_editTimer.start();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        // A directory event can mean mimeapps.list was created (or recreated
        // after being deleted). Edits to an existing file arrive via
        // fileChanged; the rest of the directory noise (KDE writes plenty of
        // config files) is ignored.
        if (!m_watcher.files().contains(m_mimeappsPath) && QFile::exists(m_mimeappsPath)) {
            m_watcher.addPath(m_mimeappsPath);
            m_editTimer.start();
        }
    });

    connect(m_consumer, &KActivities::Consumer::currentActivityChanged, this, [this](const QString &) {
        refresh();
    });
    connect(m_consumer, &KActivities::Consumer::activitiesChanged, this, [this](const QStringList &) {
        refresh();
    });
    connect(m_consumer, &KActivities::Consumer::serviceStatusChanged, this, [this](KActivities::Consumer::ServiceStatus status) {
        if (status == KActivities::Consumer::Running) {
            refresh();
        }
    });

    // Apply once at startup, in case the service is already running.
    if (m_consumer->serviceStatus() == KActivities::Consumer::Running) {
        refresh();
    }
}

QString ActivityDefaultsSwitcherModule::activityName(const QString &id)
{
    KActivities::Info *info = m_info.value(id);
    if (!info) {
        info = new KActivities::Info(id, this);
        m_info.insert(id, info);
    }
    return info->name();
}

QList<Activity> ActivityDefaultsSwitcherModule::orderedActivities()
{
    QList<Activity> activities;
    const QStringList ids = m_consumer->activities();
    activities.reserve(ids.size());
    for (const QString &id : ids) {
        activities.append(Activity{id, activityName(id)});
    }
    return activities;
}

void ActivityDefaultsSwitcherModule::refresh()
{
    if (m_consumer->serviceStatus() != KActivities::Consumer::Running) {
        return;
    }

    const QString currentId = m_consumer->currentActivity();
    const QList<Activity> activities = orderedActivities();
    const Activity current{currentId, activityName(currentId)};
    const std::optional<Activity> baseline = Resolver::selectBaselineActivity(activities);

    m_switcher.switchActivity(current, baseline, m_store.load(), readCurrentMimeDefaults());
}

void ActivityDefaultsSwitcherModule::onMimeappsChanged()
{
    if (m_consumer->serviceStatus() != KActivities::Consumer::Running) {
        return;
    }

    const QString currentId = m_consumer->currentActivity();
    const QList<Activity> activities = orderedActivities();
    const Activity current{currentId, activityName(currentId)};
    const std::optional<Activity> baseline = Resolver::selectBaselineActivity(activities);

    m_switcher.captureUserEdits(current, baseline, m_store.load(), readCurrentMimeDefaults());
}

void ActivityDefaultsSwitcherModule::ensureMimeappsWatched()
{
    if (QFile::exists(m_mimeappsPath) && !m_watcher.files().contains(m_mimeappsPath)) {
        m_watcher.addPath(m_mimeappsPath);
    }
}

K_PLUGIN_CLASS_WITH_JSON(ActivityDefaultsSwitcherModule, "module.json")

} // namespace ads

#include "module.moc"
