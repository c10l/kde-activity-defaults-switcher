#pragma once

#include <KDEDModule>

#include <PlasmaActivities/Consumer>
#include <PlasmaActivities/Info>

#include <QFileSystemWatcher>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "store.h"
#include "switcher.h"

namespace ads {

// The auto-loaded kded6 module: it observes the current KDE Activity and
// mimeapps.list and drives the Switcher (resolver + writer + profile store).
// On an activity change it resolves and applies; on a System Settings edit it
// first captures the edit into the current activity's profile. This is thin
// glue - all decision logic lives in the resolver, the write path in the
// writer, the capture rules in the Switcher, and persistence in the
// ProfileStore.
class ActivityDefaultsSwitcherModule : public KDEDModule
{
    Q_OBJECT

public:
    explicit ActivityDefaultsSwitcherModule(QObject *parent, const QVariantList &args);

private:
    QString activityName(const QString &id);
    QList<Activity> orderedActivities();
    void refresh();
    void onMimeappsChanged();
    void ensureMimeappsWatched();

    KActivities::Consumer *m_consumer = nullptr;
    QHash<QString, KActivities::Info *> m_info;
    QFileSystemWatcher m_watcher;
    QTimer m_editTimer; // coalesces rapid file-change events
    ProfileStore m_store; // loads profiles to feed the switcher
    Switcher m_switcher;
    QString m_mimeappsPath;
};

} // namespace ads
