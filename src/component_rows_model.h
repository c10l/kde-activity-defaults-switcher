// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "domain.h"

#include <QAbstractListModel>
#include <QMap>

namespace ads {

// The component rows the KCM's selected activity shows: one row per Component,
// in the canonical component order. Each row carries the component, its
// translated display name, its effective Application (the explicit binding,
// else the baseline's fallback binding, else empty), the BindingSource of that
// effective app, and the baseline's own storageId for the component.
class ComponentRowsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ComponentRole = Qt::UserRole + 1,
        NameRole,
        EffectiveApplicationRole,
        BindingSourceRole,
        BaselineApplicationRole,
    };
    Q_ENUM(Role)

    explicit ComponentRowsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces every row with the given resolved effective bindings. Every
    // Component is expected to be present (effectiveBindings guarantees it).
    void setBindings(const QMap<Component, EffectiveBinding> &bindings);

    // Replaces one row in place after an edit, signalling only that row.
    void updateBinding(Component component, const EffectiveBinding &binding);

private:
    QMap<Component, EffectiveBinding> m_bindings;
};

// The translated row label for a Component.
QString componentDisplayName(Component component);

} // namespace ads
