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
    QFile f(QStringLiteral("%1/mimeapps.list").arg(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"))));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

QString preferredBrowser()
{
    // The browser default is the first service after the mimeapps.list
    // `text/html=` key — the same config file the writer writes (ADR-0001).
    const QString text = QString::fromUtf8(mimeappsContent());
    const QString marker = QStringLiteral("text/html=");
    const int at = text.indexOf(marker);
    if (at < 0) {
        return {};
    }
    const int valueStart = at + marker.size();
    const int valueEnd = text.indexOf(QLatin1Char('\n'), valueStart);
    const QString line = valueEnd < 0 ? text.mid(valueStart) : text.mid(valueStart, valueEnd - valueStart);
    return line.section(QLatin1Char(';'), 0, 0).trimmed();
}

// A fake resolver records every call and returns a scripted decision set.
class FakeResolver : public Resolver
{
public:
    QMap<Component, Application> resolve(
        const Activity &currentActivity,
        const std::optional<Activity> &baselineActivity,
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

} // namespace

class SwitcherTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void appliesOnlyBrowserDecisionToWriter();
    void passesCurrentStateToResolver();
    void forwardsWriterResult();
    void switchingActivityFlipsBrowserDefault();
    void fallsBackToBaselineBrowserBinding();

private:
    static QString configDir();
    static QString dataDir();
};

void SwitcherTest::initTestCase()
{
    // Install real desktop entries so KApplicationTrader resolves the storage
    // ids exactly as in a live session.
    QDir().mkpath(dataDir() + QStringLiteral("/applications"));
    for (const QString &id : {QStringLiteral("chromium.desktop"), QStringLiteral("firefox.desktop")}) {
        QFile desktopFile(dataDir() + QStringLiteral("/applications/") + id);
        QVERIFY(desktopFile.open(QIODevice::WriteOnly));
        desktopFile.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%1\n").arg(id).toUtf8());
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
    // is removed and the shared config cache re-read.
    QFile::remove(configDir() + QStringLiteral("/mimeapps.list"));
    KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::SimpleConfig)->reparseConfiguration();
}

void SwitcherTest::appliesOnlyBrowserDecisionToWriter()
{
    auto resolver = std::make_unique<FakeResolver>();
    resolver->decisions = {
        {Component::Browser, QStringLiteral("firefox.desktop")},
        {Component::Email, QStringLiteral("org.kde.kmail.desktop")},
    };
    auto *resolverPtr = resolver.get();
    auto writer = std::make_unique<FakeWriter>();
    auto *writerPtr = writer.get();

    Switcher switcher(std::move(resolver), std::move(writer));
    switcher.switchActivity(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        {},
        {});

    QCOMPARE(resolverPtr->calls, 1);
    QCOMPARE(writerPtr->calls, 1);
    // Only the browser component reaches the writer for this tracer bullet.
    QCOMPARE(writerPtr->lastDecisions.size(), 1);
    QCOMPARE(writerPtr->lastDecisions.value(Component::Browser), QStringLiteral("firefox.desktop"));
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
    const QMap<Component, Application> currentDefaults{{Component::Browser, QStringLiteral("firefox.desktop")}};

    Switcher switcher(std::move(resolver), std::move(writer));
    switcher.switchActivity(current, baseline, profiles, currentDefaults);

    QCOMPARE(resolverPtr->lastCurrent.id, QStringLiteral("work-uuid"));
    QCOMPARE(resolverPtr->lastCurrent.name, QStringLiteral("Work"));
    QVERIFY(resolverPtr->lastBaseline.has_value());
    QCOMPARE(resolverPtr->lastBaseline->id, QStringLiteral("default-uuid"));
    QCOMPARE(resolverPtr->lastProfiles.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
             QStringLiteral("chromium.desktop"));
    QCOMPARE(resolverPtr->lastDefaults.value(Component::Browser), QStringLiteral("firefox.desktop"));
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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        {},
        {});

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
    QCOMPARE(preferredBrowser(), QStringLiteral("chromium.desktop"));

    const int defaultWritten = switcher.switchActivity(defaultActivity, baselineActivity, profiles, {});
    QCOMPARE(defaultWritten, 1);
    QCOMPARE(preferredBrowser(), QStringLiteral("firefox.desktop"));
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
    QCOMPARE(preferredBrowser(), QStringLiteral("firefox.desktop"));
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
