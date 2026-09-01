#include "writer.h"

#include <KSharedConfig>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ads;

namespace {

// The Writer test runs against a throwaway XDG_CONFIG_HOME, never the live
// system config. The main() below points XDG_CONFIG_HOME/XDG_DATA_HOME at
// temporary directories before the first Qt/KDE access.

QByteArray mimeappsContent()
{
    QFile f(QStringLiteral("%1/mimeapps.list").arg(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"))));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

} // namespace

class WriterTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void writesBrowserDecisionToMimeapps();
    void writesWhenDecisionDiffersFromCurrentDefault();
    void skipsNoOpWhenDecisionMatchesCurrentDefault();
    void noOpLeavesExistingConfigUntouched();
    void skipsTerminalComponent();
    void skipsComponentWithoutKnownMimeKeys();
    void returnsNumberOfComponentsWritten();
    void skipsEmptyApplication();

private:
    static QString configDir();
    static QString dataDir();
};

void WriterTest::initTestCase()
{
    // Install a real desktop entry for the browser application so that
    // serviceByStorageId resolves it exactly as it does in a live session,
    // exercising the writer's primary path rather than its fallback.
    QDir().mkpath(dataDir() + QStringLiteral("/applications"));
    QFile desktopFile(dataDir() + QStringLiteral("/applications/firefox.desktop"));
    QVERIFY(desktopFile.open(QIODevice::WriteOnly));
    desktopFile.write("[Desktop Entry]\nType=Application\nName=Firefox\nExec=firefox\n");
    desktopFile.close();
}

QString WriterTest::configDir()
{
    return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
}

QString WriterTest::dataDir()
{
    return QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
}

void WriterTest::init()
{
    // Each test starts from a clean config directory: the previous test's
    // mimeapps.list is removed and the shared config cache re-read.
    QFile::remove(configDir() + QStringLiteral("/mimeapps.list"));
    KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::SimpleConfig)->reparseConfiguration();
}

void WriterTest::writesBrowserDecisionToMimeapps()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };

    writer.apply(decisions, {});

    const QByteArray content = mimeappsContent();
    QVERIFY(content.contains("text/html=firefox.desktop;"));
    QVERIFY(content.contains("x-scheme-handler/http=firefox.desktop;"));
    QVERIFY(content.contains("x-scheme-handler/https=firefox.desktop;"));
}

void WriterTest::writesWhenDecisionDiffersFromCurrentDefault()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };
    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("chromium.desktop")},
    };

    writer.apply(decisions, currentDefaults);

    QVERIFY(mimeappsContent().contains("text/html=firefox.desktop;"));
}

void WriterTest::skipsNoOpWhenDecisionMatchesCurrentDefault()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };
    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };

    const int written = writer.apply(decisions, currentDefaults);

    QCOMPARE(written, 0);
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/mimeapps.list")));
}

void WriterTest::noOpLeavesExistingConfigUntouched()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };

    writer.apply(decisions, {});
    const QByteArray before = mimeappsContent();
    QVERIFY(!before.isEmpty());

    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };
    const int written = writer.apply(decisions, currentDefaults);

    QCOMPARE(written, 0);
    QCOMPARE(mimeappsContent(), before);
}

void WriterTest::skipsTerminalComponent()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };

    const int written = writer.apply(decisions, {});

    QCOMPARE(written, 0);
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/mimeapps.list")));
}

void WriterTest::skipsComponentWithoutKnownMimeKeys()
{
    // Only the browser component has mime keys in this ticket; the full
    // 13-component map lands in ticket 05. An unknown component must not be
    // written silently.
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Email, QStringLiteral("org.kde.kmail.desktop")},
    };

    const int written = writer.apply(decisions, {});

    QCOMPARE(written, 0);
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/mimeapps.list")));
}

void WriterTest::returnsNumberOfComponentsWritten()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("firefox.desktop")},
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };

    const int written = writer.apply(decisions, {});

    QCOMPARE(written, 1);
    QVERIFY(mimeappsContent().contains("text/html=firefox.desktop;"));
}

void WriterTest::skipsEmptyApplication()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("")},
    };
    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("firefox.desktop")},
    };

    const int written = writer.apply(decisions, currentDefaults);

    QCOMPARE(written, 0);
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/mimeapps.list")));
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
    WriterTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_writer.moc"
