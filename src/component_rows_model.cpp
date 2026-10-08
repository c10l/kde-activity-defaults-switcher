// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "component_rows_model.h"

#include "translation_domain.h"

#include <KLocalizedString>

#include <array>

namespace ads {

namespace {

// The form's reading order: the sections of the System Settings Default
// Applications page, each with its components in that page's order. The section
// of each component is derived from componentSection(), so a component moved
// here moves between headings too, and the two cannot disagree. Every Component
// appears exactly once (pinned by a test).
constexpr std::array<Component, 13> kDisplayOrder{
    Component::Browser,     Component::Email,       Component::Calendar,
    Component::Tel,         Component::ImageViewer, Component::MusicPlayer,
    Component::VideoPlayer, Component::TextEditor,  Component::PdfViewer,
    Component::FileManager, Component::Terminal,    Component::ArchiveManager,
    Component::Geo,
};

} // namespace

QString componentLabel(Component component)
{
    switch (component) {
    case Component::Browser:
        return i18nd(kcmTranslationDomain, "Web browser:");
    case Component::Email:
        return i18nd(kcmTranslationDomain, "Email client:");
    case Component::FileManager:
        return i18nd(kcmTranslationDomain, "File manager:");
    case Component::TextEditor:
        return i18nd(kcmTranslationDomain, "Text editor:");
    case Component::Calendar:
        return i18nd(kcmTranslationDomain, "Calendar:");
    case Component::Geo:
        return i18nd(kcmTranslationDomain, "Map:");
    case Component::ImageViewer:
        return i18nd(kcmTranslationDomain, "Image viewer:");
    case Component::MusicPlayer:
        return i18nd(kcmTranslationDomain, "Music player:");
    case Component::PdfViewer:
        return i18nd(kcmTranslationDomain, "PDF viewer:");
    case Component::Tel:
        return i18ndc(kcmTranslationDomain, "Default phone app", "Phone Numbers:");
    case Component::VideoPlayer:
        return i18nd(kcmTranslationDomain, "Video player:");
    case Component::ArchiveManager:
        return i18nd(kcmTranslationDomain, "Archive manager:");
    case Component::Terminal:
        return i18nd(kcmTranslationDomain, "Terminal emulator:");
    case Component::Count:
        break;
    }
    return QString();
}

QString componentSection(Component component)
{
    switch (component) {
    case Component::Browser:
    case Component::Email:
    case Component::Calendar:
    case Component::Tel:
        return i18nd(kcmTranslationDomain, "Internet");
    case Component::ImageViewer:
    case Component::MusicPlayer:
    case Component::VideoPlayer:
        return i18nd(kcmTranslationDomain, "Multimedia");
    case Component::TextEditor:
    case Component::PdfViewer:
        return i18nd(kcmTranslationDomain, "Documents");
    case Component::FileManager:
    case Component::Terminal:
    case Component::ArchiveManager:
    case Component::Geo:
        return i18nd(kcmTranslationDomain, "Utilities");
    case Component::Count:
        break;
    }
    return QString();
}

ComponentRowsModel::ComponentRowsModel(QObject *parent) : QAbstractListModel(parent)
{
    // The form's structure is static - the sections and their components never
    // change - so it is built once and only the bindings are refreshed. A model
    // reset would destroy and rebuild every delegate, and Kirigami's FormLayout
    // warns on each torn-down row; an activity switch or a Reset does not change
    // the form's shape, so no reset is ever needed.
    QString section;
    m_rows.reserve(kDisplayOrder.size() * 2);
    for (const Component component : kDisplayOrder) {
        const QString nextSection = componentSection(component);
        if (nextSection != section) {
            section = nextSection;
            Row header;
            header.isSection = true;
            header.section = section;
            m_rows.append(header);
        }
        Row row;
        row.component = component;
        row.section = section;
        m_rows.append(row);
    }
}

int ComponentRowsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant ComponentRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());

    switch (role) {
    case IsSectionRole:
        return row.isSection;
    case SectionRole:
        return row.section;
    case ComponentRole:
        // A section-header row belongs to no component: -1 is not a Component.
        return row.isSection ? -1 : static_cast<int>(row.component);
    case NameRole:
        return row.isSection ? QString() : componentLabel(row.component);
    case EffectiveApplicationRole:
        return row.binding.application.value_or(QString());
    case BindingSourceRole:
        return static_cast<int>(row.binding.source);
    case BaselineApplicationRole:
        return row.binding.baselineApplication.value_or(QString());
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
        {SectionRole, QByteArrayLiteral("section")},
        {IsSectionRole, QByteArrayLiteral("isSection")},
    };
}

void ComponentRowsModel::setBindings(const QMap<Component, EffectiveBinding> &bindings)
{
    for (Row &row : m_rows) {
        if (row.isSection) {
            continue;
        }
        const auto binding = bindings.constFind(row.component);
        row.binding = binding == bindings.constEnd() ? EffectiveBinding{} : binding.value();
    }
    if (!m_rows.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1));
    }
}

void ComponentRowsModel::updateBinding(Component component, const EffectiveBinding &binding)
{
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).isSection || m_rows.at(row).component != component) {
            continue;
        }
        m_rows[row].binding = binding;
        const QModelIndex changed = index(row);
        Q_EMIT dataChanged(changed, changed);
        return;
    }
}

} // namespace ads
