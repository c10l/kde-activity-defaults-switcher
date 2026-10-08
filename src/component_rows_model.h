// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "domain.h"

#include <QAbstractListModel>
#include <QMap>

namespace ads {

// The component rows the KCM's selected activity shows: one section-header row
// per section, then one row per Component of that section, in the reading order
// of the System Settings Default Applications page (Internet, Multimedia,
// Documents, Utilities). Each component row carries the component, its
// translated label, its effective Application (the explicit binding, else the
// baseline's fallback binding, else empty), the BindingSource of that effective
// app, and the baseline's own storageId for the component. A section-header row
// carries only its section title (isSection true, component -1) so the view
// renders the same grouping as the Default Applications form.
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
        SectionRole,
        IsSectionRole,
    };
    Q_ENUM(Role)

    explicit ComponentRowsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Refreshes every component row's effective binding (the form's structure
    // is fixed, so the rows themselves are never replaced). Every Component is
    // expected to be present (effectiveBindings guarantees it).
    void setBindings(const QMap<Component, EffectiveBinding> &bindings);

    // Replaces one component row in place after an edit, signalling only that
    // row.
    void updateBinding(Component component, const EffectiveBinding &binding);

private:
    struct Row {
        bool isSection = false;
        Component component = Component::Count;
        QString section;
        EffectiveBinding binding;
    };

    QList<Row> m_rows;
};

// The translated row label for a Component, e.g. "Web browser:".
QString componentLabel(Component component);

// The translated section a Component is grouped under, mirroring the System
// Settings Default Applications page.
QString componentSection(Component component);

} // namespace ads
