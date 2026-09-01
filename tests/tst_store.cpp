#include "store.h"

#include <KSharedConfig>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ads;

namespace {

// The store test runs against a throwaway XDG_CONFIG_HOME, never the live
// system config (see main() below).

QByteArray storeContent()
{
    QFile f(QStringLiteral("%1/activity-defaults-switcherrc").arg(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"))));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

} // namespace

class StoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void savesOneGroupPerComponentKeyedByActivityId();
    void loadRoundTripsSavedProfiles();
    void loadEmptyWhenNoFile();
    void saveReplacesStaleBindings();
    void persistsAcrossStoreInstances();
    void profilesWithoutBindingsLeaveNoGroup();

private:
    static QString configDir();
};

QString StoreTest::configDir()
{
    return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
}

void StoreTest::init()
{
    // Each test starts from a clean config: the previous test's store file is
    // removed and the shared config cache re-read.
    QFile::remove(configDir() + QStringLiteral("/activity-defaults-switcherrc"));
    KSharedConfig::openConfig(QStringLiteral("activity-defaults-switcherrc"), KConfig::SimpleConfig)->reparseConfiguration();
}

void StoreTest::savesOneGroupPerComponentKeyedByActivityId()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    work.setBinding(Component::Email, QStringLiteral("org.kde.kmail2.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));

    const QMap<ActivityId, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };

    ProfileStore store;
    store.save(profiles);

    // Agreed structure (spec): one group per component, activity id as key,
    // storageId as value.
    const QByteArray content = storeContent();
    QVERIFY(content.contains("[Browser]"));
    QVERIFY(content.contains("work-uuid=firefox.desktop"));
    QVERIFY(content.contains("default-uuid=chromium.desktop"));
    QVERIFY(content.contains("[Email]"));
    QVERIFY(content.contains("work-uuid=org.kde.kmail2.desktop"));
    QVERIFY(!content.contains("default-uuid=org.kde.kmail2.desktop"));
}

void StoreTest::loadRoundTripsSavedProfiles()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    work.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Email, QStringLiteral("org.kde.kmail2.desktop"));

    const QMap<ActivityId, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };

    ProfileStore store;
    store.save(profiles);

    const auto loaded = store.load();
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
             QStringLiteral("firefox.desktop"));
    QCOMPARE(loaded.value(QStringLiteral("work-uuid")).bindingFor(Component::Terminal),
             QStringLiteral("org.kde.konsole.desktop"));
    QCOMPARE(loaded.value(QStringLiteral("default-uuid")).bindingFor(Component::Email),
             QStringLiteral("org.kde.kmail2.desktop"));
    QVERIFY(!loaded.value(QStringLiteral("work-uuid")).hasBinding(Component::Email));
}

void StoreTest::loadEmptyWhenNoFile()
{
    ProfileStore store;
    const auto loaded = store.load();
    QVERIFY(loaded.isEmpty());
}

void StoreTest::saveReplacesStaleBindings()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    ProfileStore store;
    store.save({{QStringLiteral("work-uuid"), work}});

    Profile updated;
    updated.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));
    store.save({{QStringLiteral("work-uuid"), updated}});

    const auto loaded = store.load();
    QCOMPARE(loaded.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
             QStringLiteral("chromium.desktop"));
    QVERIFY(!storeContent().contains("work-uuid=firefox.desktop"));
}

void StoreTest::persistsAcrossStoreInstances()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    ProfileStore writerStore;
    writerStore.save({{QStringLiteral("work-uuid"), work}});

    // A second, unrelated store instance sees the same persisted profiles.
    ProfileStore readerStore;
    const auto loaded = readerStore.load();
    QCOMPARE(loaded.value(QStringLiteral("work-uuid")).bindingFor(Component::Browser),
             QStringLiteral("firefox.desktop"));
}

void StoreTest::profilesWithoutBindingsLeaveNoGroup()
{
    ProfileStore store;
    store.save({{QStringLiteral("work-uuid"), Profile{}}});

    const auto loaded = store.load();
    QVERIFY(loaded.isEmpty());
    QVERIFY(!storeContent().contains("[Browser]"));
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
    StoreTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_store.moc"
