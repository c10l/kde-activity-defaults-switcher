// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

// Ticket 26: the kded6 module's plugin id must be a valid D-Bus path element.
//
// kded6 derives the module name from KPluginMetaData::pluginId(), which returns
// the JSON "Id" if present and otherwise the plugin file's basename, and then
// builds a D-Bus object path "/modules/<id>" from it. D-Bus path elements may
// only contain letters, digits and underscores, so a hyphenated id (the .so
// basename "activity-defaults-switcher-kded", with no Id in module.json) is
// rejected: kded6 logs "The kded module name ... is invalid!" and never
// registers the module as a D-Bus object. This test loads the built plugin's
// real KPluginMetaData (the same metadata kded6 reads) and pins the id's shape,
// so the warning cannot return silently. The filename and the id no longer have
// to match, which is why the check is on pluginId() rather than on the basename.

#include <KPluginMetaData>
#include <QRegularExpression>
#include <QtTest>

class ModuleMetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void pluginIdIsDbusValid();
};

// The D-Bus path element grammar: [A-Za-z0-9_]+ (no hyphen, no dot).
static const QRegularExpression kDbusPathElement(QStringLiteral("^[A-Za-z0-9_]+$"));

void ModuleMetadataTest::pluginIdIsDbusValid()
{
    // ADS_KDED_PLUGIN is the built plugin file ($<TARGET_FILE:...>) injected by
    // CMake, so this reads the very artifact kded6 will load, not a copy.
    const KPluginMetaData metaData(QStringLiteral(ADS_KDED_PLUGIN));
    QVERIFY2(metaData.isValid(), "the built kded plugin must carry valid KPlugin metadata");

    const QString pluginId = metaData.pluginId();
    QVERIFY2(!pluginId.isEmpty(), "the plugin id must not be empty");

    // The pin: the id is a legal D-Bus path element, so "/modules/<id>" is a
    // valid object path and kded6 registers the module instead of warning.
    QVERIFY2(kDbusPathElement.match(pluginId).hasMatch(),
             qPrintable(QStringLiteral("plugin id \"%1\" is not a valid D-Bus path element "
                                       "(must match [A-Za-z0-9_]+)")
                            .arg(pluginId)));

    // And it is the name we intend, not merely a valid-looking fallback.
    QCOMPARE(pluginId, QStringLiteral("kde_activity_defaults_switcher"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ModuleMetadataTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_module_metadata.moc"
