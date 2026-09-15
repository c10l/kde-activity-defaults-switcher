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
// activity selector marks the baseline activity.

#include "kcm_smoke_support.h"

#include "activity_backend.h"
#include "application_backend.h"
#include "domain.h"
#include "kcm_backend.h"
#include "store.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQuickItem>
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
        return candidates.value(component);
    }
    QString displayNameFor(const Application &application) const override
    {
        return names.value(application, application);
    }

    QMap<Component, QList<ApplicationCandidate>> candidates;
    QHash<Application, QString> names;
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

// The ComboBox of a component row, found among the RowLayout delegate's
// children. The row itself is the Repeater delegate carrying `component`.
QObject *comboForComponent(QQuickItem *root, Component component)
{
    for (QObject *object : root->findChildren<QObject *>()) {
        if (!QString(object->metaObject()->className()).contains(QStringLiteral("Repeater"))) {
            continue;
        }
        const int count = object->property("count").toInt();
        for (int i = 0; i < count; ++i) {
            QQuickItem *row = nullptr;
            const bool invoked =
                QMetaObject::invokeMethod(object, "itemAt", Qt::DirectConnection,
                                          Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, i));
            if (!invoked || !row ||
                row->property("component").toInt() != static_cast<int>(component)) {
                continue;
            }
            for (QObject *child : row->children()) {
                if (QString(child->metaObject()->className())
                        .startsWith(QStringLiteral("ComboBox"))) {
                    return child;
                }
            }
        }
    }
    return nullptr;
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
            if (row && row->property("component").toInt() == static_cast<int>(component)) {
                return row;
            }
        }
    }
    return nullptr;
}

// The activity selector: the only ComboBox the view creates outside the
// Repeater delegates (whose items are reached through itemAt), so it is the
// only ComboBox in findChildren. Its displayText marks the baseline activity.
QObject *activitySelector(QQuickItem *root)
{
    const QList<QObject *> children = root->findChildren<QObject *>();
    for (QObject *object : children) {
        if (QString(object->metaObject()->className()).startsWith(QStringLiteral("ComboBox"))) {
            return object;
        }
    }
    return nullptr;
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
                row) {
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
    void explicitBindingRowShowsTheBoundApplication();
    void fallbackBindingRowShowsTheBaselineApplication();
    void unsetComponentRowShowsTheInheritEntry();
    void activitySelectorMarksTheBaselineActivity();
    void shellWiringAppliesOnApplyNotInstantly();
    void emptyStateHidesPickersAndDisablesEditing();
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
    // sees as a fallback. Every other component is unset for both.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
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
    applications->candidates.insert(
        Component::Browser,
        {
            ApplicationCandidate{QStringLiteral("falkon.desktop"), QStringLiteral("Falkon")},
            ApplicationCandidate{QStringLiteral("firefox.desktop"), QStringLiteral("Firefox")},
        });
    applications->candidates.insert(
        Component::Email,
        {ApplicationCandidate{QStringLiteral("kmail.desktop"), QStringLiteral("KMail")}});
    applications->names.insert(QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"));
    applications->names.insert(QStringLiteral("kmail.desktop"), QStringLiteral("KMail"));

    m_backend = std::make_unique<KcmBackend>(std::move(activityBackend), std::move(applications),
                                             std::move(store));
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
