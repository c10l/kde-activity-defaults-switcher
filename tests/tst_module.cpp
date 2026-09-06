// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "module.h"

#include <KSharedConfig>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "activity_backend.h"
#include "store.h"
#include "writer.h"

using namespace ads;

namespace {

// A fake ActivityBackend the shell tests drive: tests set the reported running
// state/current/list/names and then emit the change signals the module is
// connected to (the real KActivities connection is never made).
class FakeBackend : public ActivityBackend
{
    Q_OBJECT

public:
    explicit FakeBackend(QObject *parent = nullptr) : ActivityBackend(parent) {}

    bool isRunning() const override { return m_running; }
    QString currentActivityId() const override { return m_currentId; }
    QList<Activity> allActivities() const override { return m_activities; }
    QString activityName(const QString &id) const override
    {
        for (const Activity &a : m_activities) {
            if (a.id == id) {
                return a.name;
            }
        }
        return m_names.value(id);
    }

    void setRunning(bool running, bool emitSignal = true)
    {
        m_running = running;
        if (emitSignal) {
            emit runningChanged(running);
        }
    }
    void setCurrentActivity(const QString &id)
    {
        m_currentId = id;
        emit currentActivityChanged(id);
    }
    void setActivities(const QList<Activity> &activities)
    {
        m_activities = activities;
        emit activitiesChanged();
    }
    void setActivityName(const QString &id, const QString &name) { m_names.insert(id, name); }
    // Simulate a user renaming one activity: update the reported name for that
    // id (both in the activity list and the fallback lookup) and emit the
    // rename signal the module is connected to, exactly as KActivities::Info's
    // nameChanged does on the real backend.
    void renameActivity(const QString &id, const QString &name)
    {
        for (auto &a : m_activities) {
            if (a.id == id) {
                a.name = name;
            }
        }
        setActivityName(id, name);
        emit activitiesRenamed();
    }

    bool m_running = false;
    QString m_currentId;
    QList<Activity> m_activities;
    QHash<QString, QString> m_names;
};

// A fake resolver records every call and returns scripted decisions (mirrors
// the switcher tests' FakeResolver).
class FakeResolver : public Resolver
{
public:
    QMap<Component, Application>
    resolve(const Activity &currentActivity, const std::optional<Activity> &baselineActivity,
            const QMap<ActivityId, Profile> &profiles,
            const QMap<Component, Application> &currentDefaults) const override
    {
        ++calls;
        lastCurrent = currentActivity;
        lastBaseline = baselineActivity;
        lastProfiles = profiles;
        lastDefaults = currentDefaults;
        return decisions;
    }

    QMap<Component, Application> decisions;
    mutable int calls = 0;
    mutable Activity lastCurrent;
    mutable std::optional<Activity> lastBaseline;
    mutable QMap<ActivityId, Profile> lastProfiles;
    mutable QMap<Component, Application> lastDefaults;
};

// A fake writer records every apply call (mirrors the switcher tests).
class FakeWriter : public Writer
{
public:
    int apply(const QMap<Component, Application> &decisions,
              const QMap<Component, Application> &currentDefaults) const override
    {
        ++calls;
        lastDecisions = decisions;
        lastDefaults = currentDefaults;
        return decisions.size();
    }

    mutable int calls = 0;
    mutable QMap<Component, Application> lastDecisions;
    mutable QMap<Component, Application> lastDefaults;
};

// A fake store records every save and returns a scripted load (mirrors the
// switcher tests).
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

// Writes a throwaway mimeapps.list whose browser (x-scheme-handler/http) points
// at the given storage id - the exact shape the System Settings component
// chooser and the writer use, so readCurrentDefaults sees a real browser default.
static void writeBrowserDefault(const QString &configDir, const QString &storageId)
{
    QFile f(configDir + QStringLiteral("/mimeapps.list"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    const QByteArray body = "[Default Applications]\n"
                            "x-scheme-handler/http=" +
                            storageId.toUtf8() + ";\n";
    f.write(body);
    f.close();
}

} // namespace

class ModuleTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void injectsFakesAndNeverTouchesLiveSystem();
    void activityChangeCallsSwitchActivity();
    void serviceStatusRunningTriggersRefresh();
    void alreadyRunningAtStartupRefreshesOnce();
    void notRunningDoesNotResolve();
    void activityChangeResolvesWithCorrectBaseline();
    void resolverReceivesLoadedProfilesAndCurrentDefaults();
    void activityRenameTriggersRefresh();
    void renamingActivityToDefaultMakesItBaseline();
    void renamingDefaultAwayFallsBackToAlphabeticalFirst();
    void transientEmptyNameDoesNotBecomeBaseline();
    void mimeappsChangeCapturesBindingIntoCurrentActivity();
    void recreatedFileIsReWatchedAndCaptureRuns();
    void rapidChangesCoalesceIntoSingleCapture();
    void configDirCreatedAfterStartupIsWatchedAndCaptureRuns();

private:
    QString configDir() const;
    QString dataDir() const;
};

void ModuleTest::initTestCase()
{
    QVERIFY(QDir().mkpath(configDir()));
    QDir().mkpath(dataDir());
}

void ModuleTest::init()
{
    // Each test starts from a clean config: drop the files a previous test's
    // module may have written so no cross-test state leaks through them.
    QFile::remove(configDir() + QStringLiteral("/mimeapps.list"));
    QFile::remove(configDir() + QStringLiteral("/kdeglobals"));
    QFile::remove(configDir() + QStringLiteral("/activity-defaults-switcherrc"));
    KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::SimpleConfig)
        ->reparseConfiguration();
    KSharedConfig::openConfig(QStringLiteral("kdeglobals"))->reparseConfiguration();
}

QString ModuleTest::configDir() const
{
    return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
}

QString ModuleTest::dataDir() const
{
    return QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
}

// Criterion 1: the module accepts injected fakes, so a shell test can drive it
// without a real KActivities backend and without constructing any KDE/DBus or
// live-filesystem object.
void ModuleTest::injectsFakesAndNeverTouchesLiveSystem()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);

    // Constructing the module with fakes must succeed headless; no KActivities
    // consumer, D-Bus registration, or live config write happens.
    ActivityDefaultsSwitcherModule module(
        nullptr, std::move(backend), std::make_unique<FakeResolver>(),
        std::make_unique<FakeWriter>(), std::make_unique<FakeStore>());
}

// Criterion 2: a current-activity change is routed to Switcher::switchActivity,
// which resolves and applies but never persists a profile (the capture path is
// the one that writes the store).
void ModuleTest::activityChangeCallsSwitchActivity()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
        Activity{QStringLiteral("home-id"), QStringLiteral("Home")},
    });
    backend->setCurrentActivity(QStringLiteral("default-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();
    auto writer = std::make_unique<FakeWriter>();
    auto *writerPtr = writer.get();
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::move(writer), std::move(store));

    // Changing the current activity drives the switch path (refresh): the
    // resolver and writer run with the new current activity, and the store is
    // never written (only the capture path persists a profile).
    const int callsAfterStartup = resolverPtr->calls; // 1, from the startup refresh
    QCOMPARE(storePtr->saves, 0);

    // Switch to a *different* current activity.
    backendPtr->setCurrentActivity(QStringLiteral("home-id"));

    QCOMPARE(resolverPtr->calls, callsAfterStartup + 1);
    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("home-id"));
    QCOMPARE(resolverPtr->lastCurrent.name, QStringLiteral("Home"));
    QVERIFY(writerPtr->calls >= 1);
    QCOMPARE(storePtr->saves, 0); // still no persistence: switch, not capture
}

// Criterion 6: when the activities service was already Running at construction,
// the module refreshes once at startup (resolver is called with the reported
// current activity), and arriving Running later also refreshes.
void ModuleTest::alreadyRunningAtStartupRefreshesOnce()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    });
    backend->setCurrentActivity(QStringLiteral("default-id"));
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());

    QCOMPARE(resolverPtr->calls, 1);
    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("default-id"));
}

// Criterion 6 (bis): the service becoming Running later triggers a refresh.
void ModuleTest::serviceStatusRunningTriggersRefresh()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(false); // not running at construction: no startup refresh
    backend->setActivities({
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    });
    backend->setCurrentActivity(QStringLiteral("work-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());
    QCOMPARE(resolverPtr->calls, 0); // not running: nothing resolved

    // The KActivities service comes up.
    backendPtr->setRunning(true);
    QCOMPARE(resolverPtr->calls, 1);
    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("work-id"));
}

// Criterion 7: while the backend is not Running, neither an activity change nor
// a running-state change mid-flight triggers any resolve (the module guards
// every entry point).
void ModuleTest::notRunningDoesNotResolve()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(false);
    backend->setActivities({Activity{QStringLiteral("work-id"), QStringLiteral("Work")}});
    backend->setCurrentActivity(QStringLiteral("work-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());
    QCOMPARE(resolverPtr->calls, 0);

    // Firing the activity-change signal while still not running must not resolve.
    backendPtr->setCurrentActivity(QStringLiteral("some-other-id"));
    QCOMPARE(resolverPtr->calls, 0);

    // Still not running -> a repeated running(bool false) must not resolve.
    backendPtr->setRunning(false);
    QCOMPARE(resolverPtr->calls, 0);
}

// Criterion 2 (detail): when the current activity changes, the baseline the
// module hands the resolver is the activity named "Default" selected from the
// backend's reported activity list (the spec's baseline rule).
void ModuleTest::activityChangeResolvesWithCorrectBaseline()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    });
    backend->setCurrentActivity(QStringLiteral("work-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());

    QCOMPARE(resolverPtr->calls, 1);
    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("work-id"));
    QCOMPARE(resolverPtr->lastCurrent.name, QStringLiteral("Work"));
    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("default-id"));

    // Switching the current activity re-resolves with the new current.
    backendPtr->setCurrentActivity(QStringLiteral("default-id"));
    QCOMPARE(resolverPtr->calls, 2);
    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("default-id"));
}

// The module feeds the resolver the profiles loaded from the store and the
// current defaults it read, alongside the current/baseline it derived (the
// "profiles/defaults" half of criterion 2). Seed a real (throwaway) store and a
// real mimeapps.list, then watch both flow into the resolver.
void ModuleTest::resolverReceivesLoadedProfilesAndCurrentDefaults()
{
    // Seed the profile store the module loads from: Work binds the browser to
    // chromium.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));
    ProfileStore(configDir() + QStringLiteral("/activity-defaults-switcherrc"))
        .save({{QStringLiteral("work-id"), work}});
    // Seed a current default for the browser that differs from the binding, so
    // both a profile binding and a current default are visible downstream.
    writeBrowserDefault(configDir(), QStringLiteral("firefox.desktop"));

    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({Activity{QStringLiteral("work-id"), QStringLiteral("Work")}});
    backend->setCurrentActivity(QStringLiteral("work-id"));
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());

    // Startup (already Running) refresh passed the loaded profile and the read
    // current default into the resolver alongside current/baseline.
    QVERIFY(resolverPtr->calls >= 1);
    QCOMPARE(
        resolverPtr->lastProfiles.value(QStringLiteral("work-id")).bindingFor(Component::Browser),
        QStringLiteral("chromium.desktop"));
    QCOMPARE(resolverPtr->lastDefaults.value(Component::Browser),
             QStringLiteral("firefox.desktop"));
}

// AC1: a rename is surfaced to the module through the backend's rename signal,
// so the module refreshes and re-selects the baseline from the freshly-read
// activity names. Here renaming "Bob" to "Aaron" moves it ahead of "Alice" in
// the canonical order, which would never re-resolve unless the rename reached
// the module.
void ModuleTest::activityRenameTriggersRefresh()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({
        Activity{QStringLiteral("alice-id"), QStringLiteral("Alice")},
        Activity{QStringLiteral("bob-id"), QStringLiteral("Bob")},
    });
    backend->setCurrentActivity(QStringLiteral("alice-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());

    // Startup refresh: no "Default", so the fallback baseline is the
    // alphabetical-first activity (Alice).
    const int callsAtStartup = resolverPtr->calls;
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("alice-id"));

    // Rename Bob to Aaron: it now leads the canonical order, so the fallback
    // baseline must move to bob-id and the module must refresh to notice.
    backendPtr->renameActivity(QStringLiteral("bob-id"), QStringLiteral("Aaron"));

    QCOMPARE(resolverPtr->calls, callsAtStartup + 1);
    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("bob-id"));
    QCOMPARE(resolverPtr->lastBaseline->name, QStringLiteral("Aaron"));
}

// AC2: renaming an activity (that was not the baseline) to "Default" makes it
// the baseline, because the rename is observed and re-resolved rather than
// deferred until some unrelated event.
void ModuleTest::renamingActivityToDefaultMakesItBaseline()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("other-id"), QStringLiteral("Alpha")},
    });
    backend->setCurrentActivity(QStringLiteral("work-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());

    // Startup: no "Default", so the fallback baseline is the alphabetical-first
    // (Alpha / other-id), not work-id.
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("other-id"));

    // Rename the work activity to literally be "Default".
    backendPtr->renameActivity(QStringLiteral("work-id"), QStringLiteral("Default"));

    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("work-id"));
    QCOMPARE(resolverPtr->lastBaseline->name, QStringLiteral("Default"));
}

// AC3: renaming the "Default" activity away from that name drops it as the
// baseline and the fallback (first in the canonical order) takes over.
void ModuleTest::renamingDefaultAwayFallsBackToAlphabeticalFirst()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({
        Activity{QStringLiteral("def-id"), QStringLiteral("Default")},
        Activity{QStringLiteral("alpha-id"), QStringLiteral("Alpha")},
        Activity{QStringLiteral("zulu-id"), QStringLiteral("Zulu")},
    });
    backend->setCurrentActivity(QStringLiteral("alpha-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());

    // The activity literally named "Default" is the baseline at startup.
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("def-id"));

    // Rename it away from "Default"; with no "Default" left the fallback is the
    // alphabetical-first (Alpha).
    backendPtr->renameActivity(QStringLiteral("def-id"), QStringLiteral("Middle"));

    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("alpha-id"));
    QCOMPARE(resolverPtr->lastBaseline->name, QStringLiteral("Alpha"));
}

// AC4: a not-yet-synchronized KActivities::Info can transiently report an empty
// name for an activity. Because empty sorts first in the canonical order, an
// empty-named activity could otherwise be mis-selected as the fallback baseline
// when no "Default" exists. The module must ignore nameless activities when
// choosing the baseline.
void ModuleTest::transientEmptyNameDoesNotBecomeBaseline()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    // A freshly added activity whose info has not synchronized yet reports an
    // empty name, alongside the real (named) current activity.
    backend->setActivities({
        Activity{QStringLiteral("fresh-id"), QString()},
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
    });
    backend->setCurrentActivity(QStringLiteral("work-id"));
    FakeBackend *backendPtr = backend.get();
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(),
                                          std::make_unique<FakeStore>());
    const int callsAtStartup = resolverPtr->calls;

    // No "Default" and the empty-named activity sorts first, but the baseline
    // must be the named "Work", never the transient empty one.
    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("work-id"));

    // A re-resolve (e.g. the freshly added activity arriving via
    // activitiesChanged) must still not hand the empty name over as baseline.
    backendPtr->setActivities({
        Activity{QStringLiteral("fresh-id"), QString()},
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
    });
    QCOMPARE(resolverPtr->calls, callsAtStartup + 1);
    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("work-id"));
    QCOMPARE(resolverPtr->lastBaseline->name, QStringLiteral("Work"));
}

// Criterion 3: a change to a watched config file (a System Settings edit) is
// routed to Switcher::captureUserEdits - after coalescing - which records the
// new default as a binding for the current activity.
void ModuleTest::mimeappsChangeCapturesBindingIntoCurrentActivity()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({Activity{QStringLiteral("work-id"), QStringLiteral("Work")}});
    backend->setCurrentActivity(QStringLiteral("work-id"));
    auto resolver = std::make_unique<FakeResolver>();
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(), std::move(store));

    // A browser default appears (a user picks it) in the watched config file.
    writeBrowserDefault(configDir(), QStringLiteral("chromium.desktop"));

    // Let the file watcher notice and the 100 ms coalescing timer settle.
    QTest::qWait(350);

    // captureUserEdits ran: the edit was recorded as a *binding* for the
    // current activity in the store (a pure refresh would never save).
    QVERIFY(storePtr->saves >= 1);
    QCOMPARE(storePtr->saved.value(QStringLiteral("work-id")).bindingFor(Component::Browser),
             QStringLiteral("chromium.desktop"));
}

// Criterion 5: a watched config file that is deleted and recreated is re-added
// to the watcher, and its change then drives capture.
void ModuleTest::recreatedFileIsReWatchedAndCaptureRuns()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({Activity{QStringLiteral("work-id"), QStringLiteral("Work")}});
    backend->setCurrentActivity(QStringLiteral("work-id"));
    auto resolver = std::make_unique<FakeResolver>();
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    // Pre-create the file so the module starts watching it at construction.
    // Startup refresh sees chromium as the current default.
    writeBrowserDefault(configDir(), QStringLiteral("chromium.desktop"));

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(), std::move(store));

    // The file is genuinely removed (the watcher path is dropped), then
    // recreated pointing elsewhere - as when the component chooser rewrites it as
    // a brand new file. This must fire directoryChanged -> the module re-adds the
    // (now present, no-longer-watched) file to the watcher and captures.
    QFile file(configDir() + QStringLiteral("/mimeapps.list"));
    QVERIFY(file.remove());
    QTest::qWait(300); // let the removal settle (drops the watched path)

    writeBrowserDefault(configDir(), QStringLiteral("firefox.desktop"));
    QTest::qWait(350); // let re-add + coalescing + capture run

    QVERIFY(storePtr->saves >= 1);
    QCOMPARE(storePtr->saved.value(QStringLiteral("work-id")).bindingFor(Component::Browser),
             QStringLiteral("firefox.desktop"));
}

// Criterion 4: several rapid config-file changes within the coalescing window
// collapse into a single capture that reflects the final state.
void ModuleTest::rapidChangesCoalesceIntoSingleCapture()
{
    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({Activity{QStringLiteral("work-id"), QStringLiteral("Work")}});
    backend->setCurrentActivity(QStringLiteral("work-id"));
    auto resolver = std::make_unique<FakeResolver>();
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(), std::move(store));

    // Three rapid edits land within the 100 ms coalescing window; only the
    // final (firefox) should be captured.
    writeBrowserDefault(configDir(), QStringLiteral("chromium.desktop"));
    writeBrowserDefault(configDir(), QStringLiteral("thunderbird.desktop"));
    writeBrowserDefault(configDir(), QStringLiteral("firefox.desktop"));
    QTest::qWait(350);

    // A single coalesced capture records only the final state.
    QCOMPARE(storePtr->saves, 1);
    QCOMPARE(storePtr->saved.value(QStringLiteral("work-id")).bindingFor(Component::Browser),
             QStringLiteral("firefox.desktop"));
}

// Ticket 10, AC1+AC2: the module must keep observing the config directory even
// when it does not exist at construction (a fresh/blank config). Delete the
// config directory before building the module so nothing watches for its later
// creation; then create the directory and a mimeapps.list after startup. Both
// must be picked up (the directory watched, then the file), and the user's edit
// - a genuine file change - must drive capture into the store (spec user story
// 6), exactly as it does when the directory existed all along.
void ModuleTest::configDirCreatedAfterStartupIsWatchedAndCaptureRuns()
{
    // Start from a truly absent config dir: take the shared XDG_CONFIG_HOME
    // down so the module is constructed with nothing to watch for its creation.
    QDir config(configDir());
    QVERIFY(config.removeRecursively());

    auto backend = std::make_unique<FakeBackend>();
    backend->setRunning(true);
    backend->setActivities({Activity{QStringLiteral("work-id"), QStringLiteral("Work")}});
    backend->setCurrentActivity(QStringLiteral("work-id"));
    auto resolver = std::make_unique<FakeResolver>();
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    ActivityDefaultsSwitcherModule module(nullptr, std::move(backend), std::move(resolver),
                                          std::make_unique<FakeWriter>(), std::move(store));

    // The config directory appears only after the module started (fresh login).
    QVERIFY(QDir().mkpath(configDir()));
    QTest::qWait(400); // let the module notice the new directory and start watching it

    // A mimeapps.list is now created in it; this edit must be captured into the
    // current activity's profile.
    writeBrowserDefault(configDir(), QStringLiteral("firefox.desktop"));
    QTest::qWait(400); // directory watch -> file watch -> coalescing -> capture

    QVERIFY(storePtr->saves >= 1);
    QCOMPARE(storePtr->saved.value(QStringLiteral("work-id")).bindingFor(Component::Browser),
             QStringLiteral("firefox.desktop"));

    // Leave the shared config dir present for the tests that follow.
    QDir().mkpath(configDir());
}

int main(int argc, char **argv)
{
    // Isolate every XDG location in throwaway dirs (set before the app is
    // created, matching the other test binaries) so the module's file watching
    // and KConfig reads never touch the live system config.
    QTemporaryDir configHome;
    QTemporaryDir dataHome;
    QTemporaryDir cacheHome;
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());

    QCoreApplication app(argc, argv);
    ModuleTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_module.moc"
