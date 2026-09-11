// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "component_rows_model.h"

#include "translation_domain.h"

#include <KLocalizedString>

#include <iterator>

namespace ads {

namespace {

int rowFor(const QMap<Component, EffectiveBinding> &bindings, Component component)
{
    auto it = bindings.constFind(component);
    if (it == bindings.constEnd()) {
        return -1;
    }
    return static_cast<int>(std::distance(bindings.constBegin(), it));
}

} // namespace

QString componentDisplayName(Component component)
{
    switch (component) {
    case Component::Browser:
        return i18nd(kcmTranslationDomain, "Web Browser");
    case Component::Email:
        return i18nd(kcmTranslationDomain, "Email Client");
    case Component::FileManager:
        return i18nd(kcmTranslationDomain, "File Manager");
    case Component::TextEditor:
        return i18nd(kcmTranslationDomain, "Text Editor");
    case Component::Calendar:
        return i18nd(kcmTranslationDomain, "Calendar");
    case Component::Geo:
        return i18nd(kcmTranslationDomain, "Map");
    case Component::ImageViewer:
        return i18nd(kcmTranslationDomain, "Image Viewer");
    case Component::MusicPlayer:
        return i18nd(kcmTranslationDomain, "Music Player");
    case Component::PdfViewer:
        return i18nd(kcmTranslationDomain, "PDF Viewer");
    case Component::Tel:
        return i18nd(kcmTranslationDomain, "Dialer");
    case Component::VideoPlayer:
        return i18nd(kcmTranslationDomain, "Video Player");
    case Component::ArchiveManager:
        return i18nd(kcmTranslationDomain, "Archive Manager");
    case Component::Terminal:
        return i18nd(kcmTranslationDomain, "Terminal");
    case Component::Count:
        break;
    }
    return QString();
}

ComponentRowsModel::ComponentRowsModel(QObject *parent) : QAbstractListModel(parent) {}

int ComponentRowsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_bindings.size());
}

QVariant ComponentRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_bindings.size()) {
        return {};
    }

    auto it = m_bindings.constBegin();
    std::advance(it, index.row());
    const Component component = it.key();
    const EffectiveBinding &binding = it.value();

    switch (role) {
    case ComponentRole:
        return static_cast<int>(component);
    case NameRole:
        return componentDisplayName(component);
    case EffectiveApplicationRole:
        return binding.application.value_or(QString());
    case BindingSourceRole:
        return static_cast<int>(binding.source);
    case BaselineApplicationRole:
        return binding.baselineApplication.value_or(QString());
    default:
        return {};
    }
}

QHash<int, QByteArray> ComponentRowsModel::roleNames() const
{
    return {
        {ComponentRole, QByteArrayLiteral("component")},
        {NameRole, QByteArrayLiteral("name")},
        {EffectiveApplicationRole, QByteArrayLiteral("effectiveApplication")},
        {BindingSourceRole, QByteArrayLiteral("bindingSource")},
        {BaselineApplicationRole, QByteArrayLiteral("baselineApplication")},
    };
}

void ComponentRowsModel::setBindings(const QMap<Component, EffectiveBinding> &bindings)
{
    beginResetModel();
    m_bindings = bindings;
    endResetModel();
}

void ComponentRowsModel::updateBinding(Component component, const EffectiveBinding &binding)
{
    const int row = rowFor(m_bindings, component);
    if (row < 0) {
        return;
    }
    m_bindings[component] = binding;
    const QModelIndex changed = index(row);
    Q_EMIT dataChanged(changed, changed);
}

} // namespace ads
