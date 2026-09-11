// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "activity_backend.h"
#include "application_backend.h"
#include "domain.h"
#include "store.h"

#include <QObject>
#include <QVariantList>

#include <memory>

class QAbstractItemModel;

namespace ads {

class ComponentRowsModel;

// The KCM's testable C++ backend: all of the module's non-visual logic. It
// reads the activities and the profile store once (the KCM is a snapshot editor,
// no live refresh), derives the selected activity's effective bindings against
// the baseline activity, and persists edits straight through ProfileStore - the
// same file the daemon watches (ADR 0002), so a current-activity edit is applied
// live by the daemon and a non-current one applies when that activity becomes
// current. QML only renders this.
class KcmBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedActivityId READ selectedActivityId WRITE setSelectedActivityId NOTIFY
                   selectedActivityIdChanged)
    Q_PROPERTY(QString baselineActivityId READ baselineActivityId NOTIFY activitiesChanged)
    Q_PROPERTY(QString currentActivityId READ currentActivityId NOTIFY activitiesChanged)
    Q_PROPERTY(QVariantList activities READ activities NOTIFY activitiesChanged)
    Q_PROPERTY(bool hasActivities READ hasActivities NOTIFY activitiesChanged)
    Q_PROPERTY(QAbstractItemModel *componentRows READ componentRows CONSTANT)

public:
    KcmBackend(std::unique_ptr<ActivityBackend> activityBackend,
               std::unique_ptr<ApplicationBackend> applicationBackend,
               std::unique_ptr<ProfileStore> store, QObject *parent = nullptr);

    // Snapshot load: reads the store and the activity list once, selects the
    // baseline activity and defaults the selector to the current activity (or
    // the first). Called by the KCM on load (and on the shell's reset).
    void load();

    QString selectedActivityId() const;
    void setSelectedActivityId(const QString &id);
    QString baselineActivityId() const;
    QString currentActivityId() const;
    QVariantList activities() const;
    bool hasActivities() const;
    QAbstractItemModel *componentRows() const;

    // The picker's candidates for a component: the Inherit entry first (labelled
    // "Inherit (baseline: <name>)" when the baseline binds it, plain "Inherit"
    // otherwise), then the enumerated applications. Each entry is a map with
    // storageId, displayName and isInheritEntry.
    Q_INVOKABLE QVariantList pickerEntries(int component) const;
    // Sets an explicit binding for the selected activity and persists it.
    Q_INVOKABLE void setBinding(int component, const QString &application);
    // Removes the explicit binding, so the component falls back to the baseline.
    Q_INVOKABLE void clearBinding(int component);
    // Clears every explicit binding of the selected activity.
    Q_INVOKABLE void resetActivity();

Q_SIGNALS:
    void selectedActivityIdChanged();
    void activitiesChanged();

private:
    Profile selectedProfile() const;
    Profile baselineProfile() const;
    void refreshRows();
    void refreshRow(Component component);

    std::unique_ptr<ActivityBackend> m_activityBackend;
    std::unique_ptr<ApplicationBackend> m_applicationBackend;
    std::unique_ptr<ProfileStore> m_store;
    ComponentRowsModel *m_rows = nullptr;
    QMap<ActivityId, Profile> m_profiles;
    QList<Activity> m_activities;
    QString m_selectedActivityId;
    QString m_baselineActivityId;
    QString m_currentActivityId;
};

} // namespace ads
