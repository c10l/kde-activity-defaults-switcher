// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

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
    QFile f(
        QStringLiteral("%1/mimeapps.list").arg(QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"))));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
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
    void writesTerminalToKdeglobals();
    void terminalNoOpSkipsRewrite();
    void invalidTerminalStorageIdWritesNothing();
    void writesEveryMimeComponentToItsPrimaryKey();
    void returnsNumberOfComponentsWritten();
    void skipsEmptyApplication();
    void mimeKeyListsPinAgainstDrift();
    void everyNonTerminalComponentHasMimeKeys();
    void everyNonTerminalComponentHasAPrimaryMimeType();
    void primaryMimeTypeIsTheComponentChooserCanonicalKey();
    void readCurrentMimeDefaultsReadsPrimaryKeys();
    void readCurrentMimeDefaultsIsEmptyWithoutFile();
    void readCurrentTerminalDefaultReadsKdeglobals();
    void readCurrentDefaultsIncludesTerminalAndMime();

private:
    static QString configDir();
    static QString dataDir();
};

void WriterTest::initTestCase()
{
    // Install real desktop entries so serviceByStorageId resolves the storage
    // ids exactly as it does in a live session, exercising the writer's
    // primary path rather than its fallback. One entry per component (named
    // app-<Component>.desktop) plus firefox.desktop for the browser tests and
    // konsole for the terminal tests.
    QDir().mkpath(dataDir() + QStringLiteral("/applications"));
    QStringList ids{QStringLiteral("firefox.desktop")};
    for (const Component component : allComponents()) {
        if (component != Component::Terminal) {
            ids << QStringLiteral("app-%1.desktop").arg(componentKey(component));
        }
    }
    for (const QString &id : ids) {
        QFile desktopFile(dataDir() + QStringLiteral("/applications/") + id);
        QVERIFY(desktopFile.open(QIODevice::WriteOnly));
        desktopFile.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%1\n")
                              .arg(id)
                              .toUtf8());
        desktopFile.close();
    }

    // The terminal's Exec differs from its storage id, so the writer's
    // TerminalApplication entry can be asserted against the real exec name.
    QFile konsole(dataDir() + QStringLiteral("/applications/org.kde.konsole.desktop"));
    QVERIFY(konsole.open(QIODevice::WriteOnly));
    konsole.write("[Desktop Entry]\nType=Application\nName=Konsole\nExec=konsole\n");
    konsole.close();
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
    // Each test starts from a clean config: the previous test's mimeapps.list
    // is removed and the shared config cache re-read. The writer writes
    // through KConfig::NoGlobals (KApplicationTrader) while other code reads
    // through SimpleConfig, so both shared instances are reparsed.
    QFile::remove(configDir() + QStringLiteral("/mimeapps.list"));
    KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals,
                              QStandardPaths::GenericConfigLocation)
        ->reparseConfiguration();
    KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::SimpleConfig)
        ->reparseConfiguration();
    QFile::remove(configDir() + QStringLiteral("/kdeglobals"));
    KSharedConfig::openConfig(QStringLiteral("kdeglobals"))->reparseConfiguration();
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

void WriterTest::writesTerminalToKdeglobals()
{
    // The terminal flips via kdeglobals (ADR-0001): TerminalService holds the
    // storage id and TerminalApplication the executable, never mimeapps.list.
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };

    const int written = writer.apply(decisions, {});

    QCOMPARE(written, 1);
    const QByteArray content = kdeglobalsContent();
    QVERIFY(content.contains("[General]"));
    QVERIFY(content.contains("TerminalService=org.kde.konsole.desktop"));
    QVERIFY(content.contains("TerminalApplication=konsole"));
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/mimeapps.list")));
}

void WriterTest::invalidTerminalStorageIdWritesNothing()
{
    // A terminal storage id that resolves to no desktop entry must not write
    // an empty TerminalApplication= into kdeglobals (that would break "Open
    // Terminal", ADR-0001). The whole terminal write is skipped, nothing is
    // written, and the id does not count as a component written.
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Terminal, QStringLiteral("no.such.terminal.desktop")},
    };

    const int written = writer.apply(decisions, {});

    QCOMPARE(written, 0);
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/kdeglobals")));
}

void WriterTest::terminalNoOpSkipsRewrite()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };
    const QMap<Component, Application> currentDefaults{
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };

    const int written = writer.apply(decisions, currentDefaults);

    QCOMPARE(written, 0);
    QVERIFY(!QFile::exists(configDir() + QStringLiteral("/kdeglobals")));
}

void WriterTest::writesEveryMimeComponentToItsPrimaryKey()
{
    // Every non-terminal component writes at least its canonical mime key, so
    // all 12 mime components flip per activity (spec: "the full component to
    // mime map for all 13 components"; the terminal flips via kdeglobals and
    // stays out of mimeapps.list).
    const Writer writer;
    for (const Component component : allComponents()) {
        if (component == Component::Terminal) {
            continue;
        }
        const Application app = QStringLiteral("app-%1.desktop").arg(componentKey(component));
        const QMap<Component, Application> decisions{{component, app}};
        const int written = writer.apply(decisions, {});

        QCOMPARE(written, 1);
        const QString expectedLine =
            QStringLiteral("%1=%2;").arg(primaryMimeTypeFor(component), app);
        QVERIFY2(
            mimeappsContent().contains(expectedLine.toUtf8()),
            qPrintable(
                QStringLiteral("%1 did not write %2").arg(componentKey(component), expectedLine)));
    }
}

void WriterTest::mimeKeyListsPinAgainstDrift()
{
    // This test pins each component's mime-key list against drift. The lists
    // were drafted from the System Settings component chooser's entries
    // (plasma-workspace, kcms/componentchooser/components) and live in
    // writer.cpp's mimeKeysFor(); the copy asserted here is intentionally
    // identical to that function's return values, so any accidental change to
    // them that would silently drop or garble a key the writer applies fails.
    // This guards writer.cpp against drift; it does not independently verify
    // the values against an upstream plasma-workspace source, so treat the
    // lists as a snapshot, not a live check of the component chooser.
    //
    // Two deviations from a pure component-chooser copy are deliberate and
    // pinned here too: the browser also carries text/html so .html files and
    // links both follow the activity (user story 1), and the terminal has no
    // mime keys because it is stored in kdeglobals, never mimeapps.list.
    QCOMPARE(mimeKeysFor(Component::Browser),
             (QStringList{QStringLiteral("text/html"), QStringLiteral("x-scheme-handler/http"),
                          QStringLiteral("x-scheme-handler/https")}));
    QCOMPARE(mimeKeysFor(Component::Email),
             (QStringList{QStringLiteral("x-scheme-handler/mailto")}));
    QCOMPARE(mimeKeysFor(Component::FileManager), (QStringList{QStringLiteral("inode/directory")}));
    QCOMPARE(
        mimeKeysFor(Component::TextEditor),
        (QStringList{QStringLiteral("text/plain"), QStringLiteral("text/x-cmake"),
                     QStringLiteral("text/markdown"), QStringLiteral("application/x-docbook+xml"),
                     QStringLiteral("application/json"), QStringLiteral("application/x-yaml")}));
    QCOMPARE(mimeKeysFor(Component::Calendar), (QStringList{QStringLiteral("text/calendar")}));
    QCOMPARE(mimeKeysFor(Component::Geo), (QStringList{QStringLiteral("x-scheme-handler/geo")}));
    QCOMPARE(mimeKeysFor(Component::ImageViewer),
             (QStringList{QStringLiteral("image/png"), QStringLiteral("image/jpeg"),
                          QStringLiteral("image/webp"), QStringLiteral("image/avif"),
                          QStringLiteral("image/heif"), QStringLiteral("image/bmp"),
                          QStringLiteral("image/x-icns")}));
    QCOMPARE(mimeKeysFor(Component::MusicPlayer),
             (QStringList{QStringLiteral("audio/aac"), QStringLiteral("audio/mp4"),
                          QStringLiteral("audio/mpeg"), QStringLiteral("audio/mpegurl"),
                          QStringLiteral("audio/ogg"), QStringLiteral("audio/vnd.rn-realaudio"),
                          QStringLiteral("audio/vorbis"), QStringLiteral("audio/x-flac"),
                          QStringLiteral("audio/x-mp3"), QStringLiteral("audio/x-mpegurl"),
                          QStringLiteral("audio/x-ms-wma"), QStringLiteral("audio/x-musepack"),
                          QStringLiteral("audio/x-oggflac"), QStringLiteral("audio/x-pn-realaudio"),
                          QStringLiteral("audio/x-scpls"), QStringLiteral("audio/x-speex"),
                          QStringLiteral("audio/x-vorbis"), QStringLiteral("audio/x-vorbis+ogg"),
                          QStringLiteral("audio/x-wav")}));
    QCOMPARE(mimeKeysFor(Component::PdfViewer), (QStringList{QStringLiteral("application/pdf")}));
    QCOMPARE(mimeKeysFor(Component::Tel), (QStringList{QStringLiteral("x-scheme-handler/tel")}));
    QCOMPARE(mimeKeysFor(Component::VideoPlayer),
             (QStringList{QStringLiteral("video/3gp"),
                          QStringLiteral("video/3gpp"),
                          QStringLiteral("video/3gpp2"),
                          QStringLiteral("video/avi"),
                          QStringLiteral("video/divx"),
                          QStringLiteral("video/dv"),
                          QStringLiteral("video/fli"),
                          QStringLiteral("video/flv"),
                          QStringLiteral("video/mp2t"),
                          QStringLiteral("video/mp4"),
                          QStringLiteral("video/mp4v-es"),
                          QStringLiteral("video/mpeg"),
                          QStringLiteral("video/msvideo"),
                          QStringLiteral("video/ogg"),
                          QStringLiteral("video/quicktime"),
                          QStringLiteral("video/vnd.divx"),
                          QStringLiteral("video/vnd.mpegurl"),
                          QStringLiteral("video/vnd.rn-realvideo"),
                          QStringLiteral("video/webm"),
                          QStringLiteral("video/x-avi"),
                          QStringLiteral("video/x-flv"),
                          QStringLiteral("video/x-m4v"),
                          QStringLiteral("video/x-matroska"),
                          QStringLiteral("video/x-mpeg2"),
                          QStringLiteral("video/x-ms-asf"),
                          QStringLiteral("video/x-msvideo"),
                          QStringLiteral("video/x-ms-wmv"),
                          QStringLiteral("video/x-ms-wmx"),
                          QStringLiteral("video/x-ogm"),
                          QStringLiteral("video/x-ogm+ogg"),
                          QStringLiteral("video/x-theora"),
                          QStringLiteral("video/x-theora+ogg"),
                          QStringLiteral("application/x-matroska")}));
    QCOMPARE(mimeKeysFor(Component::ArchiveManager),
             (QStringList{QStringLiteral("application/x-tar"),
                          QStringLiteral("application/x-compressed-tar"),
                          QStringLiteral("application/x-bzip-compressed-tar"),
                          QStringLiteral("application/x-tarz"),
                          QStringLiteral("application/x-xz-compressed-tar"),
                          QStringLiteral("application/x-lzma-compressed-tar"),
                          QStringLiteral("application/x-lzip-compressed-tar"),
                          QStringLiteral("application/x-tzo"),
                          QStringLiteral("application/x-lrzip-compressed-tar"),
                          QStringLiteral("application/x-lz4-compressed-tar"),
                          QStringLiteral("application/x-zstd-compressed-tar"),
                          QStringLiteral("application/x-cd-image"),
                          QStringLiteral("application/x-bcpio"),
                          QStringLiteral("application/x-cpio"),
                          QStringLiteral("application/x-cpio-compressed"),
                          QStringLiteral("application/x-sv4cpio"),
                          QStringLiteral("application/x-sv4crc"),
                          QStringLiteral("application/x-source-rpm"),
                          QStringLiteral("application/vnd.ms-cab-compressed"),
                          QStringLiteral("application/x-xar"),
                          QStringLiteral("application/x-iso9660-appimage"),
                          QStringLiteral("application/x-archive"),
                          QStringLiteral("application/vnd.rar"),
                          QStringLiteral("application/x-rar"),
                          QStringLiteral("application/x-7z-compressed"),
                          QStringLiteral("application/zip"),
                          QStringLiteral("application/x-compress"),
                          QStringLiteral("application/gzip"),
                          QStringLiteral("application/x-bzip"),
                          QStringLiteral("application/x-lzma"),
                          QStringLiteral("application/x-xz"),
                          QStringLiteral("application/zstd"),
                          QStringLiteral("application/x-lha")}));
    // The terminal component has no mime keys: it is stored in kdeglobals,
    // so the writer must never write it to mimeapps.list.
    QVERIFY(mimeKeysFor(Component::Terminal).isEmpty());
}

void WriterTest::everyNonTerminalComponentHasMimeKeys()
{
    // Writer::apply silently skips a component whose mimeKeysFor() is empty
    // (the `continue`). A non-terminal Component whose keys fell through to the
    // empty-return default would never flip, silently. So every non-terminal
    // component that allComponents() drives must map to at least one mime key;
    // only the terminal (stored in kdeglobals, ADR-0001) is legitimately empty.
    for (const Component component : allComponents()) {
        if (component == Component::Terminal) {
            continue;
        }
        QVERIFY2(
            !mimeKeysFor(component).isEmpty(),
            qPrintable(QStringLiteral("mimeKeysFor(%%1) is empty").arg(componentKey(component))));
    }
    QVERIFY(mimeKeysFor(Component::Terminal).isEmpty());
}

void WriterTest::everyNonTerminalComponentHasAPrimaryMimeType()
{
    // readCurrentMimeDefaults() reads each component through its canonical
    // primary mime type. A Component whose primaryMimeTypeFor() fell through to
    // empty would never be read back as a current default - silently missing
    // from the capture/no-op logic. Every non-terminal component must have one;
    // the terminal has none (it lives in kdeglobals, ADR-0001).
    for (const Component component : allComponents()) {
        if (component == Component::Terminal) {
            continue;
        }
        QVERIFY2(
            !primaryMimeTypeFor(component).isEmpty(),
            qPrintable(
                QStringLiteral("primaryMimeTypeFor(%%1) is empty").arg(componentKey(component))));
    }
    QVERIFY(primaryMimeTypeFor(Component::Terminal).isEmpty());
}

void WriterTest::primaryMimeTypeIsTheComponentChooserCanonicalKey()
{
    // The "current default" of a component is read through the same canonical
    // mime type the System Settings component chooser uses to load it.
    QCOMPARE(primaryMimeTypeFor(Component::Browser), QStringLiteral("x-scheme-handler/http"));
    QCOMPARE(primaryMimeTypeFor(Component::Email), QStringLiteral("x-scheme-handler/mailto"));
    QCOMPARE(primaryMimeTypeFor(Component::FileManager), QStringLiteral("inode/directory"));
    QCOMPARE(primaryMimeTypeFor(Component::TextEditor), QStringLiteral("text/plain"));
    QCOMPARE(primaryMimeTypeFor(Component::Calendar), QStringLiteral("text/calendar"));
    QCOMPARE(primaryMimeTypeFor(Component::Geo), QStringLiteral("x-scheme-handler/geo"));
    QCOMPARE(primaryMimeTypeFor(Component::ImageViewer), QStringLiteral("image/png"));
    QCOMPARE(primaryMimeTypeFor(Component::MusicPlayer), QStringLiteral("audio/mpeg"));
    QCOMPARE(primaryMimeTypeFor(Component::PdfViewer), QStringLiteral("application/pdf"));
    QCOMPARE(primaryMimeTypeFor(Component::Tel), QStringLiteral("x-scheme-handler/tel"));
    QCOMPARE(primaryMimeTypeFor(Component::VideoPlayer), QStringLiteral("video/mp4"));
    QCOMPARE(primaryMimeTypeFor(Component::ArchiveManager), QStringLiteral("application/zip"));
}

void WriterTest::readCurrentMimeDefaultsReadsPrimaryKeys()
{
    // Simulate what the System Settings component chooser writes to
    // mimeapps.list: one preferred application per component under
    // [Default Applications]. The secondary text/x-cmake entry must not change
    // the TextEditor default (only the canonical key is consulted).
    QFile file(configDir() + QStringLiteral("/mimeapps.list"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[Default Applications]\n"
               "x-scheme-handler/http=firefox.desktop;\n"
               "x-scheme-handler/https=firefox.desktop;\n"
               "text/html=firefox.desktop;\n"
               "x-scheme-handler/mailto=org.kde.kmail2.desktop;\n"
               "inode/directory=org.kde.dolphin.desktop;\n"
               "text/plain=org.kde.kate.desktop;\n"
               "text/x-cmake=some-other-editor.desktop;\n"
               "text/calendar=org.kde.merkuro.desktop;\n"
               "x-scheme-handler/geo=marble_geo.desktop;\n"
               "image/png=org.kde.gwenview.desktop;\n"
               "audio/mpeg=org.kde.elisa.desktop;\n"
               "application/pdf=okularApplication_pdf.desktop;\n"
               "x-scheme-handler/tel=org.kde.kdeconnect.handler.desktop;\n"
               "video/mp4=org.kde.haruna.desktop;\n"
               "application/zip=org.kde.ark.desktop;\n");
    file.close();

    const auto defaults = readCurrentMimeDefaults();

    QCOMPARE(defaults.size(), 12);
    QCOMPARE(defaults.value(Component::Browser), QStringLiteral("firefox.desktop"));
    QCOMPARE(defaults.value(Component::Email), QStringLiteral("org.kde.kmail2.desktop"));
    QCOMPARE(defaults.value(Component::FileManager), QStringLiteral("org.kde.dolphin.desktop"));
    QCOMPARE(defaults.value(Component::TextEditor), QStringLiteral("org.kde.kate.desktop"));
    QCOMPARE(defaults.value(Component::Calendar), QStringLiteral("org.kde.merkuro.desktop"));
    QCOMPARE(defaults.value(Component::Geo), QStringLiteral("marble_geo.desktop"));
    QCOMPARE(defaults.value(Component::ImageViewer), QStringLiteral("org.kde.gwenview.desktop"));
    QCOMPARE(defaults.value(Component::MusicPlayer), QStringLiteral("org.kde.elisa.desktop"));
    QCOMPARE(defaults.value(Component::PdfViewer), QStringLiteral("okularApplication_pdf.desktop"));
    QCOMPARE(defaults.value(Component::Tel), QStringLiteral("org.kde.kdeconnect.handler.desktop"));
    QCOMPARE(defaults.value(Component::VideoPlayer), QStringLiteral("org.kde.haruna.desktop"));
    QCOMPARE(defaults.value(Component::ArchiveManager), QStringLiteral("org.kde.ark.desktop"));
    QVERIFY(!defaults.contains(Component::Terminal));
}

void WriterTest::readCurrentMimeDefaultsIsEmptyWithoutFile()
{
    QCOMPARE(readCurrentMimeDefaults().size(), 0);
}

void WriterTest::readCurrentTerminalDefaultReadsKdeglobals()
{
    // The current terminal default is the kdeglobals [General] TerminalService
    // entry (ADR-0001) - the same key the writer writes.
    QFile file(configDir() + QStringLiteral("/kdeglobals"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[General]\nTerminalService=org.kde.konsole.desktop\nTerminalApplication=konsole\n");
    file.close();

    QCOMPARE(readCurrentTerminalDefault(), QStringLiteral("org.kde.konsole.desktop"));
}

void WriterTest::readCurrentDefaultsIncludesTerminalAndMime()
{
    // The combined read feeds the switcher one defaults map covering both the
    // mime components (mimeapps.list) and the terminal (kdeglobals).
    QFile mimeapps(configDir() + QStringLiteral("/mimeapps.list"));
    QVERIFY(mimeapps.open(QIODevice::WriteOnly));
    mimeapps.write("[Default Applications]\nx-scheme-handler/http=firefox.desktop;\n");
    mimeapps.close();

    QFile kdeglobals(configDir() + QStringLiteral("/kdeglobals"));
    QVERIFY(kdeglobals.open(QIODevice::WriteOnly));
    kdeglobals.write(
        "[General]\nTerminalService=org.kde.konsole.desktop\nTerminalApplication=konsole\n");
    kdeglobals.close();

    const auto defaults = readCurrentDefaults();

    QCOMPARE(defaults.size(), 2);
    QCOMPARE(defaults.value(Component::Browser), QStringLiteral("firefox.desktop"));
    QCOMPARE(defaults.value(Component::Terminal), QStringLiteral("org.kde.konsole.desktop"));
}

void WriterTest::returnsNumberOfComponentsWritten()
{
    const Writer writer;
    const QMap<Component, Application> decisions{
        {Component::Browser, QStringLiteral("firefox.desktop")},
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };

    const int written = writer.apply(decisions, {});

    QCOMPARE(written, 2);
    QVERIFY(mimeappsContent().contains("text/html=firefox.desktop;"));
    QVERIFY(kdeglobalsContent().contains("TerminalService=org.kde.konsole.desktop"));
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
