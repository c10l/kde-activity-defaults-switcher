#include "module.h"

#include <KApplicationTrader>
#include <KPluginFactory>
#include <KService>

namespace ads {

namespace {

// The current browser default, read through the same mimeapps.list mechanism
// the writer writes (ADR-0001). Used by the resolver as the capture fallback.
QString currentBrowserDefault()
{
    const KService::Ptr service = KApplicationTrader::preferredService(QStringLiteral("x-scheme-handler/http"));
    return service ? service->storageId() : QString();
}

} // namespace

ActivityDefaultsSwitcherModule::ActivityDefaultsSwitcherModule(QObject *parent, const QVariantList &args)
    : KDEDModule(parent)
    , m_consumer(new KActivities::Consumer(this))
{
    Q_UNUSED(args);

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

    // Ticket 05 introduces the profile store; until then there are no
    // bindings, so the resolver falls back to capturing the current default
    // (which the writer skips as a no-op).
    const QMap<ActivityId, Profile> profiles;
    const QMap<Component, Application> currentDefaults{{Component::Browser, currentBrowserDefault()}};

    m_switcher.switchActivity(current, baseline, profiles, currentDefaults);
}

K_PLUGIN_CLASS_WITH_JSON(ActivityDefaultsSwitcherModule, "module.json")

} // namespace ads

#include "module.moc"
