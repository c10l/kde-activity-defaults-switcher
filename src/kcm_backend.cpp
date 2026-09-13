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

    const Component selected = static_cast<Component>(component);
    const bool isComponent = selected < Component::Count;
    const std::optional<Application> baselineBinding =
        isComponent ? baselineProfile().bindingFor(selected) : std::nullopt;

    QVariantMap inherit;
    inherit.insert(QStringLiteral("storageId"), QString());
    inherit.insert(QStringLiteral("isInheritEntry"), true);
    inherit.insert(QStringLiteral("displayName"),
                   baselineBinding.has_value()
                       ? i18ndc(kcmTranslationDomain,
                                "@item picker entry: inherit the baseline activity's binding",
                                "Inherit (baseline: %1)",
                                m_applicationBackend->displayNameFor(*baselineBinding))
                       : i18nd(kcmTranslationDomain, "Inherit"));
    entries.append(inherit);

    if (!isComponent) {
        return entries;
    }

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
            candidates.append(
                ApplicationCandidate{*effective, m_applicationBackend->displayNameFor(*effective)});
        }
    }

    entries.reserve(candidates.size() + 1);
    for (const ApplicationCandidate &candidate : candidates) {
        entries.append(QVariantMap{{QStringLiteral("storageId"), candidate.storageId},
                                   {QStringLiteral("displayName"), candidate.displayName},
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
    if (m_selectedActivityId.isEmpty()) {
        return;
    }
    const Component selected = static_cast<Component>(component);
    if (selected >= Component::Count) {
        return;
    }

    m_profiles[m_selectedActivityId].setBinding(selected, application);
    markPending();
    refreshRow(selected);
}

void KcmBackend::clearBinding(int component)
{
    if (m_selectedActivityId.isEmpty()) {
        return;
    }
    const Component selected = static_cast<Component>(component);
    if (selected >= Component::Count) {
        return;
    }

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
