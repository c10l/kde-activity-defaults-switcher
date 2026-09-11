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
    QString currentActivityId() const override { return QStringLiteral("work-id"); }
    QList<Activity> allActivities() const override
    {
        return {
            Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
            Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
        };
    }
    QString activityName(const QString &id) const override { return id; }
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
    QMap<ActivityId, Profile> load() const override { return loaded; }
    void save(const QMap<ActivityId, Profile> &) const override {}

    QMap<ActivityId, Profile> loaded;
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

private slots:
    void initTestCase();

    void viewLoadsWithoutWarningsOrErrors();
    void explicitBindingRowShowsTheBoundApplication();
    void fallbackBindingRowShowsTheBaselineApplication();
    void unsetComponentRowShowsTheInheritEntry();
    void activitySelectorMarksTheBaselineActivity();
    void cleanupTestCase();

private:
    // The loaded module and the backend it renders; created once for the suite
    // because KQuickConfigModuleLoader's engine and the QML enum registration
    // are process-wide.
    std::unique_ptr<KcmBackend> m_backend;
    std::unique_ptr<KQuickConfigModule> m_module;
    // The engine handed to the module loader, so the test can observe its
    // warnings from the moment mainUi() loads the view (module.engine() is only
    // meaningful after that).
    std::shared_ptr<QQmlEngine> m_engine;
    // Every QML warning/error the view emits, for the whole suite: a warning
    // raised while an assertion drives the view (a mis-selected combo, a bad
    // delegate binding) must fail just as one raised at load does.
    QStringList m_qmlWarnings;
};

void KcmQmlSmokeTest::initTestCase()
{
    // The offscreen platform plugin plus a plugin path CMake bakes in. The path
    // is a compile definition (not argv) so QtTest does not mistake it for a
    // test function name.
    qputenv("QT_QPA_PLATFORM", "offscreen");

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
    store->loaded = {
        {QStringLiteral("work-id"), work},
        {QStringLiteral("default-id"), baseline},
    };

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

    m_backend = std::make_unique<KcmBackend>(std::make_unique<FakeActivityBackend>(),
                                             std::move(applications), std::move(store));
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
