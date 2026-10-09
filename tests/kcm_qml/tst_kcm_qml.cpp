// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

// The offscreen QML smoke test (ticket 22). tst_kcm pins the KCM's C++ backend;
// this pins the view (kcm/ui/main.qml) the backend renders into, which until now
// could only be linted. It loads the production QML through the real KCMUtils
// shell - the smoke plugin (kcm_smoke_module.cpp) is kcm/kcm.cpp's module over
// an injected KcmBackend - with QT_QPA_PLATFORM=offscreen, and fails on any QML
// warning or error, so a broken id, a wrong role, an unresolved enum, a delegate
// error or a mis-selected combo cannot ship silently.
//
// The row state is the view's contract with the backend: an explicit binding
// renders the bound Application, a fallback binding renders the baseline's
// Application (italic), and an unset component renders the Inherit entry; the
// activity selector marks the baseline activity, and the form groups the
// components under the Default Applications page's category headings.

#include "kcm_smoke_support.h"

#include <KPluginMetaData>

#include "activity_backend.h"
#include "application_backend.h"
#include "domain.h"
#include "kcm_backend.h"
#include "store.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

#include <KQuickConfigModule>
#include <KQuickConfigModuleLoader>

#include <cstdio>
#include <memory>

using namespace ads;

namespace {

// A fake ActivityBackend: Work (current) and Default (the baseline by name).
class FakeActivityBackend : public ActivityBackend
{
public:
    bool isRunning() const override { return true; }
    QString currentActivityId() const override { return currentId; }
    QList<Activity> allActivities() const override { return activities; }
    QString activityName(const QString &id) const override { return id; }

    // Scriptable, so a test can drive the view to the empty state (zero
    // activities) and back.
    QString currentId = QStringLiteral("work-id");
    QList<Activity> activities{
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
};

// A fake ApplicationBackend with a fixed candidate list per component and
// display names for the baseline's bindings, so the view's ComboBoxes are
// deterministic and no KApplicationTrader/KSycoca call is made.
class FakeApplicationBackend : public ApplicationBackend
{
public:
    QList<ApplicationCandidate> applicationsFor(Component component) const override
    {
        // Recorded so a test can pin which Components the view asks about (a
        // section heading must never be asked about - its component is -1).
        requestedComponents << static_cast<int>(component);
        return candidates.value(component);
    }
    QString displayNameFor(const Application &application) const override
    {
        return names.value(application, application);
    }
    QString iconNameFor(const Application &application) const override
    {
        return icons.value(application, QStringLiteral("application-x-executable"));
    }

    QMap<Component, QList<ApplicationCandidate>> candidates;
    QHash<Application, QString> names;
    QHash<Application, QString> icons;
    // Every Component the view has asked about, in order.
    mutable QList<int> requestedComponents;
};

// A fake ApplicationChooser: scripts the "Other…" answer so the picker's escape
// hatch is exercised through the view without a dialog.
class FakeApplicationChooser : public ApplicationChooser
{
public:
    std::optional<Application> choose(Component component) override
    {
        ++calls;
        chosenFor = component;
        return result;
    }

    int calls = 0;
    std::optional<Application> result;
    std::optional<Component> chosenFor;
};

// A fake ProfileStore returning a scripted snapshot.
class FakeStore : public ProfileStore
{
public:
    QMap<ActivityId, Profile> load() const override
    {
        ++loads;
        return loaded;
    }
    void save(const QMap<ActivityId, Profile> &profiles) const override
    {
        ++saves;
        saved = profiles;
    }

    QMap<ActivityId, Profile> loaded;
    mutable int loads = 0;
    mutable int saves = 0;
    mutable QMap<ActivityId, Profile> saved;
};

QQuickItem *rowForComponent(QQuickItem *root, Component component);

// The picker of a component row: the ComponentComboBox the row hosts, found by
// its objectName (the row itself is the Repeater delegate carrying `component`).
QObject *comboForComponent(QQuickItem *root, Component component)
{
    QQuickItem *row = rowForComponent(root, component);
    return row ? row->findChild<QObject *>(QStringLiteral("componentPicker")) : nullptr;
}

// A Repeater item that is a form row: the desktop style also creates a Repeater
// inside every ComboBox popup, and those items have no `component` property -
// reading one as a row made them match component 0 (Browser).
bool isFormRow(QQuickItem *item)
{
    return item && item->property("component").isValid();
}

QQuickItem *rowForComponent(QQuickItem *root, Component component)
{
    for (QObject *object : root->findChildren<QObject *>()) {
        if (!QString(object->metaObject()->className()).contains(QStringLiteral("Repeater"))) {
            continue;
        }
        const int count = object->property("count").toInt();
        for (int i = 0; i < count; ++i) {
            QQuickItem *row = nullptr;
            QMetaObject::invokeMethod(object, "itemAt", Qt::DirectConnection,
                                      Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, i));
            if (isFormRow(row) &&
                row->property("component").toInt() == static_cast<int>(component)) {
                return row;
            }
        }
    }
    return nullptr;
}

// The entries a ComboBox popup renders. The KDE desktop style puts them in the
// popup's own Menu item list (itemAt); the Breeze and Basic styles put them in
// the popup's ListView (itemAtIndex). Each accessor is checked before it is
// invoked, because inviting a method that does not exist logs a warning the
// suite would fail on.
QList<QQuickItem *> popupEntries(QObject *popup)
{
    QList<QQuickItem *> entries;
    const auto collect = [&entries](QObject *source, const char *name, const char *signature) {
        if (!source || source->metaObject()->indexOfMethod(signature) == -1) {
            return;
        }
        for (int i = 0; i < source->property("count").toInt(); ++i) {
            QQuickItem *item = nullptr;
            if (QMetaObject::invokeMethod(source, name, Qt::DirectConnection,
                                          Q_RETURN_ARG(QQuickItem *, item), Q_ARG(int, i)) &&
                item) {
                entries << item;
            }
        }
    };

    collect(popup, "itemAt", "itemAt(int)");
    if (entries.isEmpty()) {
        collect(qobject_cast<QQuickItem *>(popup->property("contentItem").value<QObject *>()),
                "itemAtIndex", "itemAtIndex(int)");
    }
    return entries;
}

// The activity selector, found by its objectName. Its displayText marks the
// baseline activity.
QObject *activitySelector(QQuickItem *root)
{
    return root->findChild<QObject *>(QStringLiteral("activitySelector"));
}

// The Reset button (the only QQC2.Button whose text is "Reset").
QObject *resetButton(QQuickItem *root)
{
    for (QObject *object : root->findChildren<QObject *>()) {
        if (QString(object->metaObject()->className()).startsWith(QStringLiteral("Button")) &&
            object->property("text").toString() == QStringLiteral("Reset")) {
            return object;
        }
    }
    return nullptr;
}

// The "No activities to configure" placeholder (ticket 07 AC 9): a
// Kirigami.PlaceholderMessage whose text the empty state sets.
QObject *emptyStatePlaceholder(QQuickItem *root)
{
    for (QObject *object : root->findChildren<QObject *>()) {
        if (!QString(object->metaObject()->className())
                 .contains(QStringLiteral("PlaceholderMessage"))) {
            continue;
        }
        if (object->property("text").toString() == QStringLiteral("No activities to configure")) {
            return object;
        }
    }
    return nullptr;
}

// Every Repeater delegate row currently instantiated, with its component. Used
// to check the empty state hides (and disables) every row.
QList<QQuickItem *> componentRows(QQuickItem *root)
{
    QList<QQuickItem *> rows;
    for (QObject *object : root->findChildren<QObject *>()) {
        if (!QString(object->metaObject()->className()).contains(QStringLiteral("Repeater"))) {
            continue;
        }
        for (int i = 0; i < object->property("count").toInt(); ++i) {
            QQuickItem *row = nullptr;
            if (QMetaObject::invokeMethod(object, "itemAt", Qt::DirectConnection,
                                          Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, i)) &&
                isFormRow(row)) {
                rows << row;
            }
        }
    }
    return rows;
}

// Collects Qt's warning/critical messages while the view loads, writing them
// straight to stderr (never re-entering the handler, which would recurse).
QStringList *s_loggedWarnings = nullptr;

void collectWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context);
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        if (s_loggedWarnings) {
            s_loggedWarnings->append(message);
        }
        fprintf(stderr, "%s\n", qPrintable(message));
    }
}

} // namespace

class KcmQmlSmokeTest : public QObject
{
    Q_OBJECT

public:
    // QTEST_MAIN constructs QGuiApplication - and so selects the platform
    // plugin - before any test slot runs, so the offscreen choice has to be
    // made here. Setting it in initTestCase() is too late: a headless runner
    // aborts trying to load xcb before the first test even starts.
    static void initMain();

private slots:
    void initTestCase();

    void viewLoadsWithoutWarningsOrErrors();
    void kcmMetadataLoadsAndPlacesItWithDefaultApplications();
    void formGroupsComponentsLikeTheDefaultApplicationsPage();
    void switchingActivityRetargetsThePickers();
    void pickersAreOnlyAskedForRealComponents();
    void pickerDropdownListsTheApplications();
    void explicitBindingRowShowsTheBoundApplication();
    void fallbackBindingRowShowsTheBaselineApplication();
    void unsetComponentRowShowsTheInheritEntry();
    void activitySelectorMarksTheBaselineActivity();
    void shellWiringAppliesOnApplyNotInstantly();
    void emptyStateHidesPickersAndDisablesEditing();
    // Declared last: they bind an Application and so change the fixture state
    // for the rows the earlier tests assert on.
    void otherEntryAsksTheChooserAndBindsTheAnswer();
    void cancellingTheOtherEntryKeepsTheRowsApplication();
    void cleanupTestCase();

private:
    // The loaded module and the backend it renders; created once for the suite
    // because KQuickConfigModuleLoader's engine and the QML enum registration
    // are process-wide.
    std::unique_ptr<KcmBackend> m_backend;
    std::unique_ptr<KQuickConfigModule> m_module;
    // Non-owning handles to the fakes owned by m_backend, for the tests that
    // drive the view (the empty state) or observe the apply path (save()).
    FakeStore *m_store = nullptr;
    FakeActivityBackend *m_activityBackend = nullptr;
    FakeApplicationBackend *m_applications = nullptr;
    FakeApplicationChooser *m_chooser = nullptr;
    // The engine handed to the module loader, so the test can observe its
    // warnings from the moment mainUi() loads the view (module.engine() is only
    // meaningful after that).
    std::shared_ptr<QQmlEngine> m_engine;
    // Every QML warning/error the view emits, for the whole suite: a warning
    // raised while an assertion drives the view (a mis-selected combo, a bad
    // delegate binding) must fail just as one raised at load does.
    QStringList m_qmlWarnings;
};

void KcmQmlSmokeTest::initMain()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    // The KDE Quick Controls styles host a ComboBox's entries in a Menu popup
    // instead of the control's ListView, and that shape is what broke the
    // dropdown (see pickerDropdownListsTheApplications). Prefer the desktop
    // style - what System Settings itself uses - when it is installed; where it
    // is not (e.g. a minimal CI image), the default style is used and that test
    // still holds, only with less coverage of the popup shape.
    const QString desktopStyle = QLibraryInfo::path(QLibraryInfo::QmlImportsPath) +
                                 QStringLiteral("/org/kde/desktop/ComboBox.qml");
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE") && QFile::exists(desktopStyle)) {
        qputenv("QT_QUICK_CONTROLS_STYLE", "org.kde.desktop");
    }
}

void KcmQmlSmokeTest::initTestCase()
{
    // The smoke plugin path CMake bakes in is a compile definition (not argv)
    // so QtTest does not mistake it for a test function name. The platform is
    // selected in initMain(), before QGuiApplication exists.
    const QString pluginFile = QStringLiteral(ADS_KCM_SMOKE_PLUGIN);
    QVERIFY2(QFile::exists(pluginFile),
             qPrintable(QStringLiteral("smoke plugin not found: %1").arg(pluginFile)));

    // Work binds Browser explicitly; Default binds Email, which Work therefore
    // sees as a fallback. Every other component is unset for both - except
    // ArchiveManager, which Work binds to an Application the picker does not
    // enumerate, so switching onto Work grows that picker's entry list.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    work.setBinding(Component::ArchiveManager, QStringLiteral("org.kde.ark.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Email, QStringLiteral("kmail.desktop"));

    auto store = std::make_unique<FakeStore>();
    m_store = store.get();
    store->loaded = {
        {QStringLiteral("work-id"), work},
        {QStringLiteral("default-id"), baseline},
    };

    auto activityBackend = std::make_unique<FakeActivityBackend>();
    m_activityBackend = activityBackend.get();

    auto applications = std::make_unique<FakeApplicationBackend>();
    m_applications = applications.get();
    auto chooser = std::make_unique<FakeApplicationChooser>();
    m_chooser = chooser.get();
    applications->candidates.insert(
        Component::Browser,
        {
            ApplicationCandidate{QStringLiteral("falkon.desktop"), QStringLiteral("Falkon"),
                                 QStringLiteral("falkon")},
            ApplicationCandidate{QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"),
                                 QStringLiteral("firefox")},
        });
    applications->candidates.insert(
        Component::Email, {ApplicationCandidate{QStringLiteral("kmail.desktop"),
                                                QStringLiteral("KMail"), QStringLiteral("kmail")}});
    applications->names.insert(QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"));
    applications->names.insert(QStringLiteral("kmail.desktop"), QStringLiteral("KMail"));
    applications->names.insert(QStringLiteral("org.kde.ark.desktop"), QStringLiteral("Ark"));

    m_backend = std::make_unique<KcmBackend>(std::move(activityBackend), std::move(applications),
                                             std::move(store), std::move(chooser));
    m_backend->load();
    kcm_smoke::setBackend(m_backend.get());

    // Load the smoke plugin through the production loader, which owns the
    // engine and installs the `kcm` context property and the attached
    // ConfigModule the same way System Settings does. The metadata is the
    // plugin's (Id matches the QML resource prefix the plugin bundles).
    const QJsonObject metadata{
        {QStringLiteral("KPlugin"),
         QJsonObject{
             {QStringLiteral("Id"), QStringLiteral("ads-kcm-smoke")},
             {QStringLiteral("Name"), QStringLiteral("Activity Defaults Smoke")},
             {QStringLiteral("Version"), QStringLiteral("0.1")},
         }},
    };
    const KPluginMetaData pluginMetaData(metadata, pluginFile);
    m_engine = std::make_shared<QQmlEngine>();

    // Capture QML warnings for the whole suite (engine warnings for QML compile
    // and runtime errors, the message handler for anything else Qt logs).
    QObject::connect(m_engine.get(), &QQmlEngine::warnings, this,
                     [this](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings) {
                             m_qmlWarnings << warning.toString();
                         }
                     });
    s_loggedWarnings = &m_qmlWarnings;
    qInstallMessageHandler(&collectWarnings);

    const auto result = KQuickConfigModuleLoader::loadModule(pluginMetaData, nullptr, {}, m_engine);
    QVERIFY2(result.plugin, qPrintable(result.errorString));
    m_module.reset(result.plugin);
}

void KcmQmlSmokeTest::viewLoadsWithoutWarningsOrErrors()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY2(root, qPrintable(
                       QStringLiteral("main.qml failed to load: %1").arg(m_module->errorString())));
    QCOMPARE(m_module->errorString(), QString());

    // Let the view settle (delegate incubation) before judging it.
    QEventLoop loop;
    QTimer::singleShot(50, &loop, &QEventLoop::quit);
    loop.exec();

    QStringList unique = m_qmlWarnings;
    unique.removeDuplicates();
    QVERIFY2(
        unique.isEmpty(),
        qPrintable(QStringLiteral("QML warnings/errors:\n%1").arg(unique.join(QLatin1Char('\n')))));
}

// The KCM plugin's metadata is what System Settings finds the module by and
// places it with: a metadata file it cannot parse leaves the module with no id
// at all - it disappears from System Settings instead of reporting anything
// (ticket 26 pinned the same hazard for the kded module's plugin id).
//
// The path must be absolute: QPluginLoader does not resolve a relative one, and
// KPluginMetaData then comes back empty for a plugin that is perfectly fine -
// which is exactly how a working KCM was misdiagnosed as broken while this was
// being verified (ticket 31).
void KcmQmlSmokeTest::kcmMetadataLoadsAndPlacesItWithDefaultApplications()
{
    const QString pluginFile = QStringLiteral(ADS_KCM_PLUGIN);
    QVERIFY2(QFile::exists(pluginFile), qPrintable(pluginFile));

    const KPluginMetaData metadata(pluginFile);
    QVERIFY2(metadata.isValid(), qPrintable(pluginFile));
    QCOMPARE(metadata.pluginId(), QStringLiteral("activity-defaults-switcher-kcm"));

    // The Default Applications page's own category, so the per-activity page
    // sits in that group (after Default Applications' 10 and File Associations'
    // 20) rather than as an entry of its own elsewhere.
    QCOMPARE(metadata.rawData()
                 .value(QStringLiteral("X-KDE-System-Settings-Parent-Category"))
                 .toString(),
             QStringLiteral("applications-defaults"));
}

// The form is laid out like System Settings' Default Applications page:
// components grouped under that page's category headings, each carrying that
// page's label. FormLayout renders every row's Kirigami.FormData.label as a
// heading beside the row, so a changed grouping or label lands here.
void KcmQmlSmokeTest::formGroupsComponentsLikeTheDefaultApplicationsPage()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QStringList labels;
    for (QObject *object : root->findChildren<QObject *>()) {
        if (QString(object->metaObject()->className()).contains(QStringLiteral("Heading"))) {
            labels << object->property("text").toString();
        }
    }
    const QString joined = labels.join(QLatin1Char('\n'));
    const auto hasLabel = [&labels](const QString &needle) {
        for (const QString &text : labels) {
            if (text.contains(needle)) {
                return true;
            }
        }
        return false;
    };

    // The category headings the two pages share.
    for (const QString &section : {QStringLiteral("Internet"), QStringLiteral("Multimedia"),
                                   QStringLiteral("Documents"), QStringLiteral("Utilities")}) {
        QVERIFY2(hasLabel(section),
                 qPrintable(QStringLiteral("missing heading %1 in:\n%2").arg(section, joined)));
    }
    // The Default Applications labels rather than bare component names.
    QVERIFY2(
        hasLabel(QStringLiteral("Web browser:")),
        qPrintable(QStringLiteral("missing the Default Applications label in:\n%1").arg(joined)));
}

// Switching the selected Activity must move each picker onto the new profile's
// Application. The row data and the picker's entry list are recomputed in the
// same update but not necessarily in a helpful order, so the picker has to
// re-derive its selection from both - otherwise the field keeps the previous
// Activity's choice, or falls back to the Inherit entry, after a switch.
void KcmQmlSmokeTest::switchingActivityRetargetsThePickers()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    m_backend->setSelectedActivityId(QStringLiteral("work-id"));
    QObject *browser = comboForComponent(root, Component::Browser);
    QVERIFY(browser);
    QCOMPARE(browser->property("currentValue").toString(), QStringLiteral("firefox.desktop"));
    QCOMPARE(browser->property("currentText").toString(), QStringLiteral("Firefox"));

    // Work binds Browser; Default does not, so the picker must move to the
    // Inherit entry rather than keep showing Firefox.
    m_backend->setSelectedActivityId(QStringLiteral("default-id"));
    QCOMPARE(browser->property("currentValue").toString(), QString());
    QCOMPARE(browser->property("currentText").toString(), QStringLiteral("Inherit"));

    // Work also binds ArchiveManager to an Application the picker does not
    // enumerate, so switching back onto Work grows the entry list in the same
    // update: the picker must land on the bound Application, not stay on the
    // Inherit entry its now-stale list pointed at.
    QObject *archive = comboForComponent(root, Component::ArchiveManager);
    QVERIFY(archive);
    QCOMPARE(archive->property("currentText").toString(), QStringLiteral("Inherit"));

    m_backend->setSelectedActivityId(QStringLiteral("work-id"));
    QCOMPARE(archive->property("currentValue").toString(), QStringLiteral("org.kde.ark.desktop"));
    QCOMPARE(archive->property("currentText").toString(), QStringLiteral("Ark"));
    QCOMPARE(browser->property("currentValue").toString(), QStringLiteral("firefox.desktop"));
}

// The form is populated from the backend's rows, which include one
// section-heading row per section. A heading is not a Component - its
// component is -1 - and the view must not ask the backend for a heading's
// picker entries: the invalid value used to reach primaryMimeTypeFor()'s
// Q_UNREACHABLE(), and the garbage mime it returned crashed the KCM once a real
// enumeration backend was behind it.
void KcmQmlSmokeTest::pickersAreOnlyAskedForRealComponents()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    // Re-evaluate every row's picker (an activity switch refreshes all rows).
    m_backend->setSelectedActivityId(QStringLiteral("default-id"));
    m_backend->setSelectedActivityId(QStringLiteral("work-id"));

    QVERIFY2(!m_applications->requestedComponents.isEmpty(),
             "the view never asked the backend for picker entries");
    for (const int component : m_applications->requestedComponents) {
        QVERIFY2(component >= 0 && component < static_cast<int>(Component::Count),
                 qPrintable(QStringLiteral("the view asked for the picker entries of component %1")
                                .arg(component)));
    }
}

// The picker's dropdown must render its entries. Opening a Popup needs a
// window (the KCM's view is not shown in this suite), so the picker is loaded
// on its own here. The KDE styles - the desktop one is what System Settings
// uses - host the entries in a Menu popup, where an entry cannot size itself
// from ListView.view (null there) and the model item has to be bound the way
// the styles' own delegates bind it: getting that wrong left every entry empty
// and the popup a ~28px strip of clipped text. Nothing caught it, because no
// test ever opened a popup.
void KcmQmlSmokeTest::pickerDropdownListsTheApplications()
{
    QQmlComponent component(m_engine.get(),
                            QUrl(QStringLiteral("qrc:/kcm/ads-kcm-smoke/ComponentComboBox.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> picker(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(picker, qPrintable(component.errorString()));

    // The entry shape KcmBackend::pickerEntries produces.
    const QVariantList model{
        QVariantMap{{QStringLiteral("storageId"), QString()},
                    {QStringLiteral("displayName"), QStringLiteral("Inherit")},
                    {QStringLiteral("icon"), QStringLiteral("edit-undo")},
                    {QStringLiteral("isInheritEntry"), true}},
        QVariantMap{{QStringLiteral("storageId"), QStringLiteral("org.kde.falkon.desktop")},
                    {QStringLiteral("displayName"), QStringLiteral("Falkon")},
                    {QStringLiteral("icon"), QStringLiteral("falkon")},
                    {QStringLiteral("isInheritEntry"), false}},
        QVariantMap{{QStringLiteral("storageId"), QStringLiteral("firefox.desktop")},
                    {QStringLiteral("displayName"), QStringLiteral("Firefox")},
                    {QStringLiteral("icon"), QStringLiteral("firefox")},
                    {QStringLiteral("isInheritEntry"), false}},
        QVariantMap{{QStringLiteral("storageId"), QString()},
                    {QStringLiteral("displayName"), QStringLiteral("Other…")},
                    {QStringLiteral("icon"), QStringLiteral("application-x-shellscript")},
                    {QStringLiteral("isOtherEntry"), true}},
    };
    picker->setProperty("model", model);
    picker->setProperty("application", QStringLiteral("firefox.desktop"));
    QCOMPARE(picker->property("currentValue").toString(), QStringLiteral("firefox.desktop"));
    QCOMPARE(picker->property("currentText").toString(), QStringLiteral("Firefox"));

    QQuickWindow window;
    window.resize(400, 300);
    window.setColor(Qt::white);
    picker->setParentItem(window.contentItem());
    picker->setWidth(320);
    picker->setHeight(picker->implicitHeight());
    window.show();

    QObject *popup = picker->property("popup").value<QObject *>();
    QVERIFY2(popup, "the picker has no popup");
    QVERIFY(!popup->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 2000);

    const QList<QQuickItem *> entries = popupEntries(popup);
    QCOMPARE(entries.size(), 4);
    QCOMPARE(entries.at(0)->property("text").toString(), QStringLiteral("Inherit"));
    QCOMPARE(entries.at(1)->property("text").toString(), QStringLiteral("Falkon"));
    QCOMPARE(entries.at(2)->property("text").toString(), QStringLiteral("Firefox"));
    // The escape hatch the Default Applications page also lists, last.
    QCOMPARE(entries.at(3)->property("text").toString(), QStringLiteral("Other…"));

    // Every entry is laid out at its popup's width - not collapsed to the width
    // of an icon, which is what the broken delegate left behind (28px).
    for (const QQuickItem *entry : entries) {
        QVERIFY2(entry->width() >= entry->implicitWidth() && entry->width() > 100,
                 qPrintable(QStringLiteral("entry '%1' is %2 wide (implicit %3)")
                                .arg(entry->property("text").toString())
                                .arg(entry->width())
                                .arg(entry->implicitWidth())));
    }

    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    QTRY_VERIFY_WITH_TIMEOUT(!popup->property("visible").toBool(), 2000);
}

void KcmQmlSmokeTest::explicitBindingRowShowsTheBoundApplication()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QQuickItem *row = rowForComponent(root, Component::Browser);
    QVERIFY2(row, "no row rendered for Browser");
    QCOMPARE(row->property("bindingSource").toInt(), static_cast<int>(BindingSource::Explicit));

    QObject *combo = comboForComponent(root, Component::Browser);
    QVERIFY2(combo, "no picker rendered for Browser");
    QCOMPARE(combo->property("currentValue").toString(), QStringLiteral("firefox.desktop"));
    QCOMPARE(combo->property("currentText").toString(), QStringLiteral("Firefox"));
}

void KcmQmlSmokeTest::fallbackBindingRowShowsTheBaselineApplication()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QQuickItem *row = rowForComponent(root, Component::Email);
    QVERIFY2(row, "no row rendered for Email");
    QCOMPARE(row->property("bindingSource").toInt(), static_cast<int>(BindingSource::Fallback));

    QObject *combo = comboForComponent(root, Component::Email);
    QVERIFY2(combo, "no picker rendered for Email");
    QCOMPARE(combo->property("currentValue").toString(), QStringLiteral("kmail.desktop"));
    QCOMPARE(combo->property("currentText").toString(), QStringLiteral("KMail"));
}

void KcmQmlSmokeTest::unsetComponentRowShowsTheInheritEntry()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QQuickItem *row = rowForComponent(root, Component::Terminal);
    QVERIFY2(row, "no row rendered for Terminal");
    QCOMPARE(row->property("bindingSource").toInt(), static_cast<int>(BindingSource::Unset));
    QCOMPARE(row->property("effectiveApplication").toString(), QString());

    QObject *combo = comboForComponent(root, Component::Terminal);
    QVERIFY2(combo, "no picker rendered for Terminal");
    QCOMPARE(combo->property("currentIndex").toInt(), 0);
    QCOMPARE(combo->property("currentText").toString(), QStringLiteral("Inherit"));
}

void KcmQmlSmokeTest::activitySelectorMarksTheBaselineActivity()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QObject *selector = activitySelector(root);
    QVERIFY2(selector, "no activity selector rendered");
    QCOMPARE(selector->property("count").toInt(), 2);

    bool markedBaseline = false;
    bool unmarkedWork = false;
    for (int i = 0; i < selector->property("count").toInt(); ++i) {
        selector->setProperty("currentIndex", i);
        const QString value = selector->property("currentValue").toString();
        const QString display = selector->property("displayText").toString();
        if (value == QStringLiteral("default-id")) {
            markedBaseline = display.contains(QStringLiteral("(baseline)"));
        } else if (value == QStringLiteral("work-id")) {
            unmarkedWork = !display.contains(QStringLiteral("(baseline)"));
        }
    }
    QVERIFY2(markedBaseline, "the baseline activity was not marked in the selector");
    QVERIFY2(unmarkedWork, "a non-baseline activity was marked as baseline");
}

// The shell wiring the smoke plugin shares with production (ticket 24,
// boundary 1): the module must apply on Apply, not instantly - the ticket 23
// contract - and an edit must drive needsSave through the backend's
// pending-changes signal, and save() must reach the store. Breaking any of
// these in kcm/kcm_shell.cpp fails this test, where before it left the smoke
// suite green because the shell was a test-only copy.
void KcmQmlSmokeTest::shellWiringAppliesOnApplyNotInstantly()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QCOMPARE(m_module->supportsInstantApply(), false);
    QCOMPARE(m_module->needsSave(), false);
    const int savesBefore = m_store->saves;
    const int loadsBefore = m_store->loads;

    // load() must forward to backend()->load(), which re-reads the store. The
    // smoke suite otherwise only drives the backend directly, so a shell whose
    // load() stopped forwarding would silently ship.
    m_module->load();
    QCOMPARE(m_store->loads, loadsBefore + 1);

    // Edit through the backend's public API (the view's gesture path).
    m_backend->setSelectedActivityId(QStringLiteral("work-id"));
    m_backend->setBinding(static_cast<int>(Component::Terminal), QStringLiteral("konsole.desktop"));

    // The pending-changes connection must have enabled Apply; nothing written
    // yet (no instant apply).
    QCOMPARE(m_backend->hasPendingChanges(), true);
    QCOMPARE(m_module->needsSave(), true);
    QCOMPARE(m_store->saves, savesBefore);

    // Apply persists through the store, the daemon's watch file.
    m_module->save();
    QCOMPARE(m_store->saves, savesBefore + 1);
    QCOMPARE(m_backend->hasPendingChanges(), false);
    QCOMPARE(m_module->needsSave(), false);
    QCOMPARE(m_store->saved.value(QStringLiteral("work-id"))
                 .bindingFor(Component::Terminal)
                 .value_or(QString()),
             QStringLiteral("konsole.desktop"));
}

// The empty state (ticket 07 AC 9, ticket 24 boundary 2): with no activities
// the placeholder is shown and editing is disabled - the selector and every
// component row are hidden and not enabled. The suite's backend is driven to
// zero activities and back, so the test is repeatable.
void KcmQmlSmokeTest::emptyStateHidesPickersAndDisablesEditing()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    QVERIFY2(emptyStatePlaceholder(root) == nullptr ||
                 !emptyStatePlaceholder(root)->property("visible").toBool(),
             "the empty-state placeholder was visible with activities present");

    QObject *reset = resetButton(root);
    QVERIFY2(reset, "no Reset button rendered");
    if (!reset) {
        return;
    }
    QVERIFY2(reset->property("enabled").toBool(),
             "the Reset button was disabled with activities present");

    // Drive the view to the empty state through the backend's public API.
    m_activityBackend->activities.clear();
    m_backend->load();

    QObject *placeholder = emptyStatePlaceholder(root);
    QVERIFY2(placeholder, "no empty-state placeholder was rendered");
    if (!placeholder) {
        return;
    }
    QVERIFY2(placeholder->property("visible").toBool(),
             "the empty-state placeholder was not visible with zero activities");

    QObject *selector = activitySelector(root);
    QVERIFY2(selector, "no activity selector rendered");
    if (!selector) {
        return;
    }
    QVERIFY2(!selector->property("visible").toBool(),
             "the activity selector was visible with zero activities");
    QVERIFY2(!selector->property("enabled").toBool(),
             "the activity selector was enabled with zero activities");

    QVERIFY2(!reset->property("enabled").toBool(),
             "the Reset button was enabled with zero activities");

    const QList<QQuickItem *> rows = componentRows(root);
    QVERIFY2(!rows.isEmpty(), "no component rows were instantiated at all");
    for (QQuickItem *row : rows) {
        QVERIFY2(!row->property("visible").toBool(),
                 "a component row was visible with zero activities");
        QVERIFY2(!row->property("enabled").toBool(),
                 "a component row was enabled with zero activities");
    }

    // Restore, so a later test (or a rerun) sees the populated view.
    m_activityBackend->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    m_backend->load();
    QVERIFY(!emptyStatePlaceholder(root) ||
            !emptyStatePlaceholder(root)->property("visible").toBool());
}

// The picker's last entry is the Default Applications page's "Other…", and
// choosing it asks the ApplicationChooser: the answer is bound to the row, and
// the picker re-derives its selection so the field shows the chosen Application
// rather than the entry that was only ever an action.
void KcmQmlSmokeTest::otherEntryAsksTheChooserAndBindsTheAnswer()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    m_backend->load();
    m_backend->setSelectedActivityId(QStringLiteral("work-id"));
    QObject *combo = comboForComponent(root, Component::Browser);
    QVERIFY(combo);

    const QVariantList entries = combo->property("model").toList();
    QCOMPARE(entries.size(), 4); // Inherit, Falkon, Firefox, Other…
    const QVariantMap other = entries.constLast().toMap();
    QCOMPARE(other.value(QStringLiteral("isOtherEntry")).toBool(), true);
    QCOMPARE(other.value(QStringLiteral("displayName")).toString(), QStringLiteral("Other…"));

    m_chooser->result = QStringLiteral("org.kde.konsole.desktop");
    const int otherIndex = entries.size() - 1;
    combo->setProperty("currentIndex", otherIndex);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection,
                                      Q_ARG(int, otherIndex)));

    QCOMPARE(m_chooser->calls, 1);
    QCOMPARE(m_chooser->chosenFor.value_or(Component::Count), Component::Browser);
    QCOMPARE(combo->property("currentValue").toString(), QStringLiteral("org.kde.konsole.desktop"));
    QCOMPARE(combo->property("currentText").toString(), QStringLiteral("org.kde.konsole.desktop"));
    QCOMPARE(rowForComponent(root, Component::Browser)->property("effectiveApplication").toString(),
             QStringLiteral("org.kde.konsole.desktop"));
    QCOMPARE(m_backend->hasPendingChanges(), true);

    m_backend->load(); // restore the fixture for any later run
}

// Cancelling the chooser (and a chooser that returns no Application) leaves the
// row on the Activity's binding: the field must not stay stuck on the "Other…"
// entry, which is an action and never a state.
void KcmQmlSmokeTest::cancellingTheOtherEntryKeepsTheRowsApplication()
{
    QQuickItem *root = m_module->mainUi();
    QVERIFY(root);

    m_backend->load();
    m_backend->setSelectedActivityId(QStringLiteral("work-id"));
    QObject *combo = comboForComponent(root, Component::Browser);
    QVERIFY(combo);
    QCOMPARE(combo->property("currentValue").toString(), QStringLiteral("firefox.desktop"));

    const int otherIndex = combo->property("model").toList().size() - 1;
    const int callsBefore = m_chooser->calls;
    m_chooser->result = std::nullopt;
    combo->setProperty("currentIndex", otherIndex);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection,
                                      Q_ARG(int, otherIndex)));

    QCOMPARE(m_chooser->calls, callsBefore + 1);
    QCOMPARE(combo->property("currentValue").toString(), QStringLiteral("firefox.desktop"));
    QCOMPARE(combo->property("currentText").toString(), QStringLiteral("Firefox"));
    QCOMPARE(rowForComponent(root, Component::Browser)->property("effectiveApplication").toString(),
             QStringLiteral("firefox.desktop"));
    QCOMPARE(m_backend->hasPendingChanges(), false);

    m_backend->load();
}

void KcmQmlSmokeTest::cleanupTestCase()
{
    qInstallMessageHandler(nullptr);
    s_loggedWarnings = nullptr;

    QStringList unique = m_qmlWarnings;
    unique.removeDuplicates();
    QVERIFY2(
        unique.isEmpty(),
        qPrintable(QStringLiteral("QML warnings/errors:\n%1").arg(unique.join(QLatin1Char('\n')))));
}

QTEST_MAIN(KcmQmlSmokeTest)

#include "tst_kcm_qml.moc"
