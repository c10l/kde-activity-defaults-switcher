// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "store.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace ads {

ProfileStore::ProfileStore(const QString &fileName) : m_fileName(fileName) {}

QMap<ActivityId, Profile> ProfileStore::load() const
{
    auto config = KSharedConfig::openConfig(m_fileName, KConfig::SimpleConfig);
    config->reparseConfiguration();

    QMap<ActivityId, Profile> profiles;
    for (const Component component : allComponents()) {
        const KConfigGroup group(config, componentKey(component));
        const QStringList ids = group.keyList();
        for (const QString &id : ids) {
            const QString value = group.readEntry(id, QString());
            if (!value.isEmpty()) {
                profiles[id].setBinding(component, value);
            }
        }
    }
    return profiles;
}

void ProfileStore::save(const QMap<ActivityId, Profile> &profiles) const
{
    auto config = KSharedConfig::openConfig(m_fileName, KConfig::SimpleConfig);

    // Rewrite the whole file from the given profiles: drop every component
    // group first so bindings removed since the last save disappear too.
    for (const Component component : allComponents()) {
        config->group(componentKey(component)).deleteGroup();
    }
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        for (const Binding &binding : it.value().bindings()) {
            if (binding.application.isEmpty()) {
                continue;
            }
            KConfigGroup group(config, componentKey(binding.component));
            group.writeEntry(it.key(), binding.application);
        }
    }
    config->sync();
}

} // namespace ads
