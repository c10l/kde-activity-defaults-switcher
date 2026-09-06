// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "switcher.h"

#include <KSharedConfig>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ads;

namespace {

// The switcher test runs against a throwaway XDG_CONFIG_HOME/XDG_DATA_HOME,
// never the live system config (see main() below).

QByteArray mimeappsContent()
{
    QFile f(
        QStringLiteral("%1/mimeapps.list").arg(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"))));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

QString preferredService(const QString &mimeType)
{
    // The default for a mime type is the first service after its
    // mimeapps.list key - the same config file the writer writes (ADR-0001).
    const QString text = QString::fromUtf8(mimeappsContent());
    const QString marker = mimeType + QStringLiteral("=");
    const int at = text.indexOf(marker);
    if (at < 0) {
        return {};
    }
    const int valueStart = at + marker.size();
    const int valueEnd = text.indexOf(QLatin1Char('\n'), valueStart);
    const QString line =
        valueEnd < 0 ? text.mid(valueStart) : text.mid(valueStart, valueEnd - valueStart);
    return line.section(QLatin1Char(';'), 0, 0).trimmed();
}

QByteArray kdeglobalsContent()
{
    QFile f(
        QStringLiteral("%1/kdeglobals").arg(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"))));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

QString terminalService()
{
    // The terminal default is the kdeglobals [General] TerminalService entry
    // - the same key the writer writes (ADR-0001).
    const QString text = QString::fromUtf8(kdeglobalsContent());
    const QString marker = QStringLiteral("TerminalService=");
    const int at = text.indexOf(marker);
    if (at < 0) {
        return {};
    }
    const int valueStart = at + marker.size();
    const int valueEnd = text.indexOf(QLatin1Char('\n'), valueStart);
    const QString line =
        valueEnd < 0 ? text.mid(valueStart) : text.mid(valueStart, valueEnd - valueStart);
    return line.trimmed();
}

// A fake resolver records every call and returns a scripted decision set.
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

// A fake writer records every apply call and returns a scripted result.
class FakeWriter : public Writer
{
public:
    int apply(const QMap<Component, Application> &decisions,
              const QMap<Component, Application> &currentDefaults) const override
    {
        ++calls;
        lastDecisions = decisions;
        lastDefaults = currentDefaults;
        return result;
    }

    mutable int calls = 0;
    mutable QMap<Component, Application> lastDecisions;
    mutable QMap<Component, Application> lastDefaults;
    int result = 0;
};

// A fake store records every save and returns scripted loaded profiles.
class FakeStore : public ProfileStore
{
public:
    QMap<ActivityId, Profile> load() const override { return loaded; }

    void save(const QMap<ActivityId, Profile> &profiles) const override
    {
        ++saves;
        saved = profiles;
    }

    QMap<ActivityId, Profile> loaded;
    mutable int saves = 0;
    mutable QMap<ActivityId, Profile> saved;
};

} // namespace

class SwitcherTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void appliesAllResolvedDecisionsToWriter();
    void passesCurrentStateToResolver();
    void forwardsWriterResult();
    void switchingActivityFlipsBrowserDefault();
    void switchingActivityFlipsEveryComponent();
    void switchingActivityFlipsTerminalDefault();
    void fallsBackToBaselineBrowserBinding();
    void captureRecordsUserEditAsBindingForCurrentActivity();
    void captureDoesNotRecordWhenNothingChanged();
    void captureIgnoresDaemonsOwnWrite();
    void captureDoesNotTreatDaemonsFirstWriteAsUserEdit();
    void captureRecordsTerminalEditAsBindingForCurrentActivity();
    void captureDoesNotTreatDaemonsTerminalWriteAsUserEdit();
    void captureSkipsEmptyDefault();
    void captureEditRecordsBindingAndFlipsDefault();

private:
    static QString configDir();
    static QString dataDir();
};

void SwitcherTest::initTestCase()
{
    // Install real desktop entries so KApplicationTrader resolves the storage
    // ids exactly as in a live session.
    QDir().mkpath(dataDir() + QStringLiteral("/applications"));
    for (const QString &id :
         {QStringLiteral("chromium.desktop"), QStringLiteral("firefox.desktop"),
          QStringLiteral("thunderbird.desktop"), QStringLiteral("org.kde.kmail2.desktop"),
          QStringLiteral("org.kde.kate.desktop"), QStringLiteral("org.kde.kwrite.desktop")}) {
        QFile desktopFile(dataDir() + QStringLiteral("/applications/") + id);
        QVERIFY(desktopFile.open(QIODevice::WriteOnly));
        desktopFile.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%1\n")
                              .arg(id)
                              .toUtf8());
        desktopFile.close();
    }
    // The two terminal services the terminal tests switch between. Their Exec
    // values differ from the storage ids so TerminalApplication can be asserted
    // against the real executable name.
    for (const QString &id :
         {QStringLiteral("org.kde.konsole.desktop"), QStringLiteral("org.kde.yakuake.desktop")}) {
        QFile desktopFile(dataDir() + QStringLiteral("/applications/") + id);
        QVERIFY(desktopFile.open(QIODevice::WriteOnly));
        desktopFile.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%1\n")
                              .arg(id.section(QLatin1Char('.'), 2, 2))
                              .toUtf8());
        desktopFile.close();
    }
}

QString SwitcherTest::configDir()
{
    return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
}

QString SwitcherTest::dataDir()
{
    return QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
}

void SwitcherTest::init()
{
    // Each test starts from a clean config: the previous test's mimeapps.list
    // and kdeglobals are removed and the shared config caches re-read.
    QFile::remove(configDir() + QStringLiteral("/mimeapps.list"));
    KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::SimpleConfig)
        ->reparseConfiguration();
    QFile::remove(configDir() + QStringLiteral("/kdeglobals"));
    KSharedConfig::openConfig(QStringLiteral("kdeglobals"))->reparseConfiguration();
}

void SwitcherTest::appliesAllResolvedDecisionsToWriter()
{
    // The switcher forwards every resolved decision to the writer - the
    // writer decides what it can write (mime components to mimeapps.list,
    // the terminal to kdeglobals).
    auto resolver = std::make_unique<FakeResolver>();
    resolver->decisions = {
        {Component::Browser, QStringLiteral("firefox.desktop")},
        {Component::Email, QStringLiteral("org.kde.kmail.desktop")},
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };
    auto *resolverPtr = resolver.get();
    auto writer = std::make_unique<FakeWriter>();
    auto *writerPtr = writer.get();

    Switcher switcher(std::move(resolver), std::move(writer));
    switcher.switchActivity(Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
                            Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, {},
                            {});

    QCOMPARE(resolverPtr->calls, 1);
    QCOMPARE(writerPtr->calls, 1);
    QCOMPARE(writerPtr->lastDecisions.size(), 3);
    QCOMPARE(writerPtr->lastDecisions.value(Component::Browser), QStringLiteral("firefox.desktop"));
    QCOMPARE(writerPtr->lastDecisions.value(Component::Email),
             QStringLiteral("org.kde.kmail.desktop"));
    QCOMPARE(writerPtr->lastDecisions.value(Component::Terminal),
             QStringLiteral("org.kde.konsole.desktop"));
}

void SwitcherTest::passesCurrentStateToResolver()
{
    auto resolver = std::make_unique<FakeResolver>();
    auto *resolverPtr = resolver.get();
    auto writer = std::make_unique<FakeWriter>();

    const Activity current{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity baseline{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    Profile workProfile;
    workProfile.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));
    const QMap<ActivityId, Profile> profiles{{QStringLiteral("work-uuid"), workProfile}};
    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("firefox.desktop")}};

    Switcher switcher(std::move(resolver), std::move(writer));
    switcher.switchActivity(current, baseline, profiles, currentDefaults);

    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("work-uuid"));
    QCOMPARE(resolverPtr->lastCurrent.name, QStringLiteral("Work"));
    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("default-uuid"));
    QCOMPARE(
        resolverPtr->lastProfiles.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
        QStringLiteral("chromium.desktop"));
    QCOMPARE(resolverPtr->lastDefaults.value(Component::Browser),
             QStringLiteral("firefox.desktop"));
}

void SwitcherTest::forwardsWriterResult()
{
    auto resolver = std::make_unique<FakeResolver>();
    resolver->decisions = {{Component::Browser, QStringLiteral("firefox.desktop")}};
    auto writer = std::make_unique<FakeWriter>();
    writer->result = 1;

    Switcher switcher(std::move(resolver), std::move(writer));
    const int written = switcher.switchActivity(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, {}, {});

    QCOMPARE(written, 1);
}

void SwitcherTest::switchingActivityFlipsBrowserDefault()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    const QMap<ActivityId, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    Switcher switcher; // real resolver + real writer

    const int workWritten = switcher.switchActivity(workActivity, baselineActivity, profiles, {});
    QCOMPARE(workWritten, 1);
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("chromium.desktop"));

    const int defaultWritten =
        switcher.switchActivity(defaultActivity, baselineActivity, profiles, {});
    QCOMPARE(defaultWritten, 1);
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("firefox.desktop"));
}

void SwitcherTest::switchingActivityFlipsEveryComponent()
{
    // Acceptance criterion: every component resolves and flips per activity.
    // Two activities with distinct bindings for several components; each
    // component's default follows the current activity, then the baseline.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));
    work.setBinding(Component::Email, QStringLiteral("org.kde.kmail2.desktop"));
    work.setBinding(Component::TextEditor, QStringLiteral("org.kde.kate.desktop"));

    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    baseline.setBinding(Component::Email, QStringLiteral("thunderbird.desktop"));
    baseline.setBinding(Component::TextEditor, QStringLiteral("org.kde.kwrite.desktop"));

    const QMap<ActivityId, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    Switcher switcher; // real resolver + real writer

    // Switch to Work: every component flips to Work's binding.
    QCOMPARE(switcher.switchActivity(workActivity, baselineActivity, profiles, {}), 3);
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("chromium.desktop"));
    QCOMPARE(preferredService(QStringLiteral("x-scheme-handler/mailto")),
             QStringLiteral("org.kde.kmail2.desktop"));
    QCOMPARE(preferredService(QStringLiteral("text/plain")),
             QStringLiteral("org.kde.kate.desktop"));

    // Switch to Default: every component flips to the baseline binding.
    QCOMPARE(switcher.switchActivity(defaultActivity, baselineActivity, profiles, {}), 3);
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("firefox.desktop"));
    QCOMPARE(preferredService(QStringLiteral("x-scheme-handler/mailto")),
             QStringLiteral("thunderbird.desktop"));
    QCOMPARE(preferredService(QStringLiteral("text/plain")),
             QStringLiteral("org.kde.kwrite.desktop"));
}

void SwitcherTest::fallsBackToBaselineBrowserBinding()
{
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    const QMap<ActivityId, Profile> profiles{
        {QStringLiteral("work-uuid"), Profile{}},
        {QStringLiteral("default-uuid"), baseline},
    };
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    Switcher switcher; // real resolver + real writer

    const int written = switcher.switchActivity(workActivity, baselineActivity, profiles, {});
    QCOMPARE(written, 1);
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("firefox.desktop"));
}

void SwitcherTest::switchingActivityFlipsTerminalDefault()
{
    // The terminal flips per activity via kdeglobals, exactly like the mime
    // components flip via mimeapps.list.
    Profile work;
    work.setBinding(Component::Terminal, QStringLiteral("org.kde.yakuake.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));

    const QMap<ActivityId, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    Switcher switcher; // real resolver + real writer

    QCOMPARE(switcher.switchActivity(workActivity, baselineActivity, profiles, {}), 1);
    QCOMPARE(terminalService(), QStringLiteral("org.kde.yakuake.desktop"));

    QCOMPARE(switcher.switchActivity(defaultActivity, baselineActivity, profiles, {}), 1);
    QCOMPARE(terminalService(), QStringLiteral("org.kde.konsole.desktop"));
}

void SwitcherTest::captureRecordsUserEditAsBindingForCurrentActivity()
{
    auto resolver = std::make_unique<FakeResolver>();
    resolver->decisions = {{Component::Browser, QStringLiteral("firefox.desktop")}};
    auto *resolverPtr = resolver.get();
    auto writer = std::make_unique<FakeWriter>();
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    Switcher switcher(std::move(resolver), std::move(writer), std::move(store));

    const Activity current{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const std::optional<Activity> baseline{
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}};

    // A switch first establishes the state the switcher last ensured.
    switcher.switchActivity(current, baseline, {},
                            {{Component::Browser, QStringLiteral("firefox.desktop")}});

    // The user edits the browser in System Settings while on Work.
    switcher.captureUserEdits(current, baseline, {},
                              {{Component::Browser, QStringLiteral("chromium.desktop")}});

    // The edit is recorded as a binding for the current activity and persisted.
    QCOMPARE(storePtr->saves, 1);
    QCOMPARE(storePtr->saved.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
             QStringLiteral("chromium.desktop"));

    // The resolver saw the updated profile, not the original one.
    QCOMPARE(
        resolverPtr->lastProfiles.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
        QStringLiteral("chromium.desktop"));
}

void SwitcherTest::captureDoesNotRecordWhenNothingChanged()
{
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    Switcher switcher(std::make_unique<FakeResolver>(), std::make_unique<FakeWriter>(),
                      std::move(store));

    const Activity current{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const std::optional<Activity> baseline{
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}};

    switcher.switchActivity(current, baseline, {},
                            {{Component::Browser, QStringLiteral("firefox.desktop")}});
    const int written = switcher.captureUserEdits(
        current, baseline, {}, {{Component::Browser, QStringLiteral("firefox.desktop")}});

    // Nothing changed, so nothing is recorded and nothing is persisted.
    QCOMPARE(storePtr->saves, 0);
    QCOMPARE(written, 0);
}

void SwitcherTest::captureIgnoresDaemonsOwnWrite()
{
    // Real resolver: switching to Work applies the Work binding (chromium),
    // which itself changes mimeapps.list. The resulting watcher event reports
    // the post-write defaults and must not be mistaken for a user edit.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));
    const QMap<ActivityId, Profile> profiles{{QStringLiteral("work-uuid"), work}};
    const Activity current{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const std::optional<Activity> baseline{
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}};

    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();
    auto writer = std::make_unique<FakeWriter>();

    Switcher switcher(std::make_unique<Resolver>(), std::move(writer), std::move(store));

    // Switch to Work: decision chromium, current default firefox -> written.
    switcher.switchActivity(current, baseline, profiles,
                            {{Component::Browser, QStringLiteral("firefox.desktop")}});

    // The watcher event after the daemon's own write: defaults now chromium.
    const int written = switcher.captureUserEdits(
        current, baseline, profiles, {{Component::Browser, QStringLiteral("chromium.desktop")}});

    // Not a user edit: nothing recorded, and the decision matches the current
    // default so nothing is rewritten either.
    QCOMPARE(storePtr->saves, 0);
    QCOMPARE(written, 0);
}

void SwitcherTest::captureDoesNotTreatDaemonsFirstWriteAsUserEdit()
{
    // Fresh setup: no mimeapps.list entry for the browser yet, so the
    // currentDefaults map is empty. The baseline activity carries the binding
    // and Work has none. Switching to Work makes the daemon write the baseline
    // binding - creating the file. The watcher event that follows must not be
    // captured as a user edit: that would pin the baseline binding into Work's
    // profile and break baseline fallback (user story 7).
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    const QMap<ActivityId, Profile> profiles{{QStringLiteral("default-uuid"), baseline}};
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    Switcher switcher(std::make_unique<Resolver>(), std::make_unique<Writer>(), std::move(store));

    // No mimeapps.list entry at all: currentDefaults is empty. The daemon
    // resolves the baseline binding and writes it, creating the file.
    switcher.switchActivity(workActivity, baselineActivity, profiles, {});

    // The daemon's own write lands in the file; the watcher fires and reads
    // the new state (browser now present in mimeapps.list).
    const int written =
        switcher.captureUserEdits(workActivity, baselineActivity, profiles,
                                  {{Component::Browser, QStringLiteral("firefox.desktop")}});

    // Not a user edit: nothing recorded for Work, and the decision matches the
    // current default so nothing is rewritten either.
    QCOMPARE(storePtr->saves, 0);
    QCOMPARE(written, 0);
    QVERIFY(!storePtr->saved.value(QStringLiteral("work-uuid")).hasBinding(Component::Browser));
}

void SwitcherTest::captureRecordsTerminalEditAsBindingForCurrentActivity()
{
    // Changing the terminal in System Settings while on an activity records a
    // terminal binding for that activity, exactly like a browser edit does.
    Profile baseline;
    baseline.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));
    const QMap<ActivityId, Profile> profiles{{QStringLiteral("default-uuid"), baseline}};
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    Switcher switcher; // real everything

    // Starting state: the terminal default is konsole (the baseline binding).
    switcher.switchActivity(workActivity, baselineActivity, profiles, {});

    // The user changes the terminal in System Settings while on Work: kdeglobals
    // changes exactly as the component chooser writes it.
    QFile kdeglobals(configDir() + QStringLiteral("/kdeglobals"));
    QVERIFY(kdeglobals.open(QIODevice::WriteOnly));
    kdeglobals.write(
        "[General]\nTerminalService=org.kde.yakuake.desktop\nTerminalApplication=yakuake\n");
    kdeglobals.close();

    const int written = switcher.captureUserEdits(
        workActivity, baselineActivity, profiles,
        {{Component::Terminal, QStringLiteral("org.kde.yakuake.desktop")}});

    // The edit is recorded for the current activity and persisted.
    ProfileStore store;
    const auto stored = store.load();
    QCOMPARE(stored.value(QStringLiteral("work-uuid")).bindingFor(Component::Terminal),
             QStringLiteral("org.kde.yakuake.desktop"));

    // The system default already matches the edit, so nothing needed rewriting.
    QCOMPARE(written, 0);
    QCOMPARE(terminalService(), QStringLiteral("org.kde.yakuake.desktop"));
}

void SwitcherTest::captureDoesNotTreatDaemonsTerminalWriteAsUserEdit()
{
    // The terminal-side equivalent of the 05 m_lastDefaults fix: the daemon's
    // own kdeglobals write must not be captured as a user edit, or a baseline
    // terminal binding would be pinned into a child activity and break
    // baseline fallback (user story 7).
    Profile baseline;
    baseline.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));
    const QMap<ActivityId, Profile> profiles{{QStringLiteral("default-uuid"), baseline}};
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    Switcher switcher(std::make_unique<Resolver>(), std::make_unique<Writer>(), std::move(store));

    // No kdeglobals terminal entry yet: currentDefaults is empty. The daemon
    // resolves the baseline terminal binding and writes it, creating the file.
    switcher.switchActivity(workActivity, baselineActivity, profiles, {});

    // The daemon's own write lands in the file; the watcher fires and reads
    // the new state (terminal now present in kdeglobals).
    const int written = switcher.captureUserEdits(
        workActivity, baselineActivity, profiles,
        {{Component::Terminal, QStringLiteral("org.kde.konsole.desktop")}});

    // Not a user edit: nothing recorded for Work, and the decision matches the
    // current default so nothing is rewritten either.
    QCOMPARE(storePtr->saves, 0);
    QCOMPARE(written, 0);
    QVERIFY(!storePtr->saved.value(QStringLiteral("work-uuid")).hasBinding(Component::Terminal));
}

void SwitcherTest::captureSkipsEmptyDefault()
{
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    Switcher switcher(std::make_unique<FakeResolver>(), std::make_unique<FakeWriter>(),
                      std::move(store));

    const Activity current{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const std::optional<Activity> baseline{
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}};

    switcher.switchActivity(current, baseline, {},
                            {{Component::Browser, QStringLiteral("firefox.desktop")}});

    // A removed default (empty) is not recorded: resolution re-applies the
    // activity's profile, so the system heals back instead of accumulating
    // empty bindings.
    switcher.captureUserEdits(current, baseline, {}, {{Component::Browser, QString()}});
    QCOMPARE(storePtr->saves, 0);
}

void SwitcherTest::captureEditRecordsBindingAndFlipsDefault()
{
    // Real resolver, writer and store: the full capture path at the switcher
    // seam - what the file watcher reports as a System Settings edit records a
    // binding for the current activity, and the default follows.
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    const QMap<ActivityId, Profile> profiles{{QStringLiteral("default-uuid"), baseline}};
    const Activity workActivity{QStringLiteral("work-uuid"), QStringLiteral("Work")};
    const Activity defaultActivity{QStringLiteral("default-uuid"), QStringLiteral("Default")};
    const auto baselineActivity = Resolver::selectBaselineActivity({workActivity, defaultActivity});

    Switcher switcher; // real everything

    // Starting state: the system default is firefox (the baseline binding).
    switcher.switchActivity(workActivity, baselineActivity, profiles,
                            {{Component::Browser, QStringLiteral("firefox.desktop")}});

    // The user edits the browser in System Settings while on Work: the file
    // changes exactly as the component chooser writes it.
    QFile mimeapps(configDir() + QStringLiteral("/mimeapps.list"));
    QVERIFY(mimeapps.open(QIODevice::WriteOnly));
    mimeapps.write("[Default Applications]\n"
                   "text/html=chromium.desktop;\n"
                   "x-scheme-handler/http=chromium.desktop;\n"
                   "x-scheme-handler/https=chromium.desktop;\n");
    mimeapps.close();

    const int written =
        switcher.captureUserEdits(workActivity, baselineActivity, profiles,
                                  {{Component::Browser, QStringLiteral("chromium.desktop")}});

    // The edit is applied (the system default already matches it, so nothing
    // needed rewriting - no churn) ...
    QCOMPARE(written, 0);
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("chromium.desktop"));

    // ...and recorded for the current activity in the store: a fresh store
    // sees the persisted binding.
    ProfileStore store;
    const auto stored = store.load();
    QCOMPARE(stored.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
             QStringLiteral("chromium.desktop"));

    // The binding sticks: switching to Work again resolves chromium, even
    // though the baseline activity still says firefox.
    switcher.switchActivity(workActivity, baselineActivity, stored,
                            {{Component::Browser, QStringLiteral("chromium.desktop")}});
    QCOMPARE(preferredService(QStringLiteral("text/html")), QStringLiteral("chromium.desktop"));
}

int main(int argc, char **argv)
{
    // Point every XDG writable location at throwaway directories so the tests
    // never touch (or write into) the live system config, data, or caches.
    QTemporaryDir configHome;
    QTemporaryDir dataHome;
    QTemporaryDir cacheHome;
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());

    QCoreApplication app(argc, argv);
    SwitcherTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_switcher.moc"
