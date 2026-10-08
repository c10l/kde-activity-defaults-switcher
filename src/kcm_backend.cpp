// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "kcm_backend.h"

#include "component_rows_model.h"
#include "translation_domain.h"

#include <KLocalizedString>

#include <QVariantMap>

#include <algorithm>

namespace ads {

KcmBackend::KcmBackend(std::unique_ptr<ActivityBackend> activityBackend,
                       std::unique_ptr<ApplicationBackend> applicationBackend,
                       std::unique_ptr<ProfileStore> store, QObject *parent)
    : QObject(parent), m_activityBackend(std::move(activityBackend)),
      m_applicationBackend(std::move(applicationBackend)),
      m_store(store ? std::move(store) : std::make_unique<ProfileStore>()),
      m_rows(new ComponentRowsModel(this))
{
    // KActivities::Consumer populates asynchronously: at the moment System
    // Settings calls load() it can still report only the nameless null
    // activity, with the real list (and names) arriving a moment later via
    // activitiesChanged / runningChanged. The KCM is a snapshot editor and does
    // not live-refresh, but it must complete that initial snapshot - otherwise
    // it opens on "No activities to configure". The guard means a snapshot that
    // already has activities is never rebuilt, so a later add/remove cannot
    // discard edits the user has buffered. activitiesRenamed covers a name that
    // synchronizes after the list does (a fresh KActivities::Info starts empty,
    // and a nameless activity is filtered out of the snapshot).
    const auto completeInitialSnapshot = [this]() {
        if (m_activities.isEmpty()) {
            load();
        }
    };
    connect(m_activityBackend.get(), &ActivityBackend::activitiesChanged, this,
            completeInitialSnapshot);
    connect(m_activityBackend.get(), &ActivityBackend::runningChanged, this,
            completeInitialSnapshot);
    connect(m_activityBackend.get(), &ActivityBackend::activitiesRenamed, this,
            completeInitialSnapshot);
}

void KcmBackend::load()
{
    m_profiles = m_store->load();

    // The baseline rule is the daemon's (Resolver::selectBaselineActivity), with
    // the same nameless guard the daemon applies: a freshly created activity can
    // transiently report an empty name, which sorts first and would otherwise be
    // mis-selected as the fallback baseline.
    const QList<Activity> reported = m_activityBackend->allActivities();
    QList<Activity> named;
    named.reserve(reported.size());
    for (const Activity &activity : reported) {
        if (!activity.name.isEmpty()) {
            named.append(activity);
        }
    }
    m_activities = orderedActivityList(named);
    m_currentActivityId = m_activityBackend->currentActivityId();

    const std::optional<Activity> baseline = Resolver::selectBaselineActivity(m_activities);
    m_baselineActivityId = baseline.has_value() ? baseline->id : QString();

    // Default the selector to the current activity when it is listed; otherwise
    // the first activity in the canonical order.
    QString selection = m_currentActivityId;
    bool currentIsListed = false;
    for (const Activity &activity : m_activities) {
        if (activity.id == selection) {
            currentIsListed = true;
            break;
        }
    }
    if (!currentIsListed) {
        selection = m_activities.isEmpty() ? QString() : m_activities.first().id;
    }
    const bool selectionChanged = selection != m_selectedActivityId;
    m_selectedActivityId = selection;

    refreshRows();
    Q_EMIT activitiesChanged();
    if (selectionChanged) {
        Q_EMIT selectedActivityIdChanged();
    }
    // A load (open, or the shell's reset) drops any buffered edits.
    if (m_hasPendingChanges) {
        m_hasPendingChanges = false;
        Q_EMIT hasPendingChangesChanged();
    }
}

QString KcmBackend::selectedActivityId() const
{
    return m_selectedActivityId;
}

bool KcmBackend::hasPendingChanges() const
{
    return m_hasPendingChanges;
}

void KcmBackend::save()
{
    if (!m_hasPendingChanges) {
        return;
    }
    m_store->save(m_profiles);
    m_hasPendingChanges = false;
    Q_EMIT hasPendingChangesChanged();
}

void KcmBackend::markPending()
{
    if (m_hasPendingChanges) {
        return;
    }
    m_hasPendingChanges = true;
    Q_EMIT hasPendingChangesChanged();
}

void KcmBackend::setSelectedActivityId(const QString &id)
{
    if (id == m_selectedActivityId) {
        return;
    }
    m_selectedActivityId = id;
    refreshRows();
    Q_EMIT selectedActivityIdChanged();
}

QString KcmBackend::baselineActivityId() const
{
    return m_baselineActivityId;
}

QString KcmBackend::currentActivityId() const
{
    return m_currentActivityId;
}

QVariantList KcmBackend::activities() const
{
    QVariantList list;
    list.reserve(m_activities.size());
    for (const Activity &activity : m_activities) {
        list.append(QVariantMap{{QStringLiteral("id"), activity.id},
                                {QStringLiteral("name"), activity.name}});
    }
    return list;
}

bool KcmBackend::hasActivities() const
{
    return !m_activities.isEmpty();
}

QAbstractItemModel *KcmBackend::componentRows() const
{
    return m_rows;
}

QVariantList KcmBackend::pickerEntries(int component) const
{
    QVariantList entries;

    // A row that is not a Component (a section heading carries -1) has no
    // picker entries at all; converting it would put a value outside the enum
    // into the domain.
    if (!isValidComponent(component)) {
        return entries;
    }
    const Component selected = static_cast<Component>(component);

    const std::optional<Application> baselineBinding = baselineProfile().bindingFor(selected);

    QVariantMap inherit;
    inherit.insert(QStringLiteral("storageId"), QString());
    inherit.insert(QStringLiteral("isInheritEntry"), true);
    // Inheriting is "revert my explicit choice", so it carries the undo icon
    // (the same one the Reset button uses) rather than an Application icon.
    inherit.insert(QStringLiteral("icon"), QStringLiteral("edit-undo"));
    inherit.insert(QStringLiteral("displayName"),
                   baselineBinding.has_value()
                       ? i18ndc(kcmTranslationDomain,
                                "@item picker entry: inherit the baseline activity's binding",
                                "Inherit (baseline: %1)",
                                m_applicationBackend->displayNameFor(*baselineBinding))
                       : i18nd(kcmTranslationDomain, "Inherit"));
    entries.append(inherit);

    QList<ApplicationCandidate> candidates = m_applicationBackend->applicationsFor(selected);

    // A binding's Application may be absent from the enumerated candidates —
    // most visibly when it is no longer installed. Offer it anyway (after the
    // enumerated ones) so the picker can show it as the row's effective app
    // instead of falling back to the Inherit entry (ticket 21).
    const std::optional<Application> effective =
        effectiveBindings(selectedProfile(), baselineProfile()).value(selected).application;
    if (effective.has_value()) {
        const bool enumerated = std::any_of(candidates.cbegin(), candidates.cend(),
                                            [&effective](const ApplicationCandidate &candidate) {
                                                return candidate.storageId == *effective;
                                            });
        if (!enumerated) {
            candidates.append(ApplicationCandidate{*effective,
                                                   m_applicationBackend->displayNameFor(*effective),
                                                   m_applicationBackend->iconNameFor(*effective)});
        }
    }

    entries.reserve(candidates.size() + 1);
    for (const ApplicationCandidate &candidate : candidates) {
        entries.append(QVariantMap{{QStringLiteral("storageId"), candidate.storageId},
                                   {QStringLiteral("displayName"), candidate.displayName},
                                   {QStringLiteral("icon"), candidate.iconName},
                                   {QStringLiteral("isInheritEntry"), false}});
    }
    return entries;
}

void KcmBackend::setBinding(int component, const QString &application)
{
    if (application.isEmpty()) {
        clearBinding(component);
        return;
    }
    if (m_selectedActivityId.isEmpty() || !isValidComponent(component)) {
        return;
    }
    const Component selected = static_cast<Component>(component);

    m_profiles[m_selectedActivityId].setBinding(selected, application);
    markPending();
    refreshRow(selected);
}

void KcmBackend::clearBinding(int component)
{
    if (m_selectedActivityId.isEmpty() || !isValidComponent(component)) {
        return;
    }
    const Component selected = static_cast<Component>(component);

    m_profiles[m_selectedActivityId].removeBinding(selected);
    markPending();
    refreshRow(selected);
}

void KcmBackend::resetActivity()
{
    if (m_selectedActivityId.isEmpty()) {
        return;
    }

    Profile &profile = m_profiles[m_selectedActivityId];
    for (const Component component : allComponents()) {
        profile.removeBinding(component);
    }
    markPending();
    refreshRows();
}

Profile KcmBackend::selectedProfile() const
{
    return m_profiles.value(m_selectedActivityId);
}

Profile KcmBackend::baselineProfile() const
{
    return m_profiles.value(m_baselineActivityId);
}

void KcmBackend::refreshRows()
{
    m_rows->setBindings(effectiveBindings(selectedProfile(), baselineProfile()));
}

void KcmBackend::refreshRow(Component component)
{
    const QMap<Component, EffectiveBinding> effective =
        effectiveBindings(selectedProfile(), baselineProfile());
    m_rows->updateBinding(component, effective.value(component));
}

} // namespace ads
