// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "writer.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KService>
#include <KSharedConfig>
#include <QStandardPaths>
#include <QtGlobal>

namespace ads {

namespace {

// Writes the terminal binding to kdeglobals (ADR-0001): TerminalService holds
// the .desktop storage id and TerminalApplication the executable, exactly as
// the System Settings terminal component chooser writes them. Returns false
// and writes nothing when the storage id resolves to no desktop entry (an
// empty Exec): an empty TerminalApplication= would break "Open Terminal"
// (ADR-0001), so the whole write is skipped.
bool writeTerminal(const Application &application)
{
    KService::Ptr service = KService::serviceByStorageId(application);
    if (!service) {
        KDesktopFile desktopFile(application);
        service = KService::Ptr(new KService(&desktopFile, application));
    }

    if (service->exec().isEmpty()) {
        return false;
    }

    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KConfigGroup general(config, QStringLiteral("General"));
    general.writeEntry(QStringLiteral("TerminalService"), application);
    general.writeEntry(QStringLiteral("TerminalApplication"), service->exec());
    config->sync();
    return true;
}

} // namespace

QStringList mimeKeysFor(Component component)
{
    switch (component) {
    case Component::Browser:
        return {
            QStringLiteral("text/html"),
            QStringLiteral("x-scheme-handler/http"),
            QStringLiteral("x-scheme-handler/https"),
        };
    case Component::Email:
        return {
            QStringLiteral("x-scheme-handler/mailto"),
        };
    case Component::FileManager:
        return {
            QStringLiteral("inode/directory"),
        };
    case Component::TextEditor:
        return {
            QStringLiteral("text/plain"),       QStringLiteral("text/x-cmake"),
            QStringLiteral("text/markdown"),    QStringLiteral("application/x-docbook+xml"),
            QStringLiteral("application/json"), QStringLiteral("application/x-yaml"),
        };
    case Component::Calendar:
        return {
            QStringLiteral("text/calendar"),
        };
    case Component::Geo:
        return {
            QStringLiteral("x-scheme-handler/geo"),
        };
    case Component::ImageViewer:
        return {
            QStringLiteral("image/png"),    QStringLiteral("image/jpeg"),
            QStringLiteral("image/webp"),   QStringLiteral("image/avif"),
            QStringLiteral("image/heif"),   QStringLiteral("image/bmp"),
            QStringLiteral("image/x-icns"),
        };
    case Component::MusicPlayer:
        return {
            QStringLiteral("audio/aac"),       QStringLiteral("audio/mp4"),
            QStringLiteral("audio/mpeg"),      QStringLiteral("audio/mpegurl"),
            QStringLiteral("audio/ogg"),       QStringLiteral("audio/vnd.rn-realaudio"),
            QStringLiteral("audio/vorbis"),    QStringLiteral("audio/x-flac"),
            QStringLiteral("audio/x-mp3"),     QStringLiteral("audio/x-mpegurl"),
            QStringLiteral("audio/x-ms-wma"),  QStringLiteral("audio/x-musepack"),
            QStringLiteral("audio/x-oggflac"), QStringLiteral("audio/x-pn-realaudio"),
            QStringLiteral("audio/x-scpls"),   QStringLiteral("audio/x-speex"),
            QStringLiteral("audio/x-vorbis"),  QStringLiteral("audio/x-vorbis+ogg"),
            QStringLiteral("audio/x-wav"),
        };
    case Component::PdfViewer:
        return {
            QStringLiteral("application/pdf"),
        };
    case Component::Tel:
        return {
            QStringLiteral("x-scheme-handler/tel"),
        };
    case Component::VideoPlayer:
        return {
            QStringLiteral("video/3gp"),
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
            QStringLiteral("application/x-matroska"),
        };
    case Component::ArchiveManager:
        return {
            QStringLiteral("application/x-tar"),
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
            QStringLiteral("application/x-lha"),
        };
    case Component::Terminal:
        // The terminal lives in kdeglobals, not mimeapps.list.
        return {};
    case Component::Count:
        // The trailing count sentinel is never a component.
        break;
    }
    Q_UNREACHABLE();
}

QString primaryMimeTypeFor(Component component)
{
    // The canonical key the System Settings component chooser uses to load the
    // component's current default application.
    switch (component) {
    case Component::Browser:
        return QStringLiteral("x-scheme-handler/http");
    case Component::Email:
        return QStringLiteral("x-scheme-handler/mailto");
    case Component::FileManager:
        return QStringLiteral("inode/directory");
    case Component::TextEditor:
        return QStringLiteral("text/plain");
    case Component::Calendar:
        return QStringLiteral("text/calendar");
    case Component::Geo:
        return QStringLiteral("x-scheme-handler/geo");
    case Component::ImageViewer:
        return QStringLiteral("image/png");
    case Component::MusicPlayer:
        return QStringLiteral("audio/mpeg");
    case Component::PdfViewer:
        return QStringLiteral("application/pdf");
    case Component::Tel:
        return QStringLiteral("x-scheme-handler/tel");
    case Component::VideoPlayer:
        return QStringLiteral("video/mp4");
    case Component::ArchiveManager:
        return QStringLiteral("application/zip");
    case Component::Terminal:
        return QString();
    case Component::Count:
        // The trailing count sentinel is never a component.
        break;
    }
    Q_UNREACHABLE();
}

QMap<Component, Application> readCurrentMimeDefaults()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals,
                                            QStandardPaths::GenericConfigLocation);
    config->reparseConfiguration();
    const KConfigGroup defaultApplications(config, QStringLiteral("Default Applications"));

    QMap<Component, Application> defaults;
    for (const Component component : allComponents()) {
        if (component == Component::Terminal) {
            continue; // kdeglobals
        }
        const QStringList entries =
            defaultApplications.readXdgListEntry(primaryMimeTypeFor(component));
        if (!entries.isEmpty()) {
            defaults.insert(component, entries.first());
        }
    }
    return defaults;
}

Application readCurrentTerminalDefault()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->reparseConfiguration();
    const KConfigGroup general(config, QStringLiteral("General"));
    return general.readEntry(QStringLiteral("TerminalService"), QString());
}

QMap<Component, Application> readCurrentDefaults()
{
    QMap<Component, Application> defaults = readCurrentMimeDefaults();
    const Application terminal = readCurrentTerminalDefault();
    if (!terminal.isEmpty()) {
        defaults.insert(Component::Terminal, terminal);
    }
    return defaults;
}

int Writer::apply(const QMap<Component, Application> &decisions,
                  const QMap<Component, Application> &currentDefaults) const
{
    int written = 0;
    for (auto it = decisions.constBegin(); it != decisions.constEnd(); ++it) {
        const Component component = it.key();
        const Application application = it.value();

        if (application.isEmpty()) {
            continue; // Never write an empty application into the config.
        }

        if (application == currentDefaults.value(component)) {
            continue; // No-op: the decision already matches the current default.
        }

        if (component == Component::Terminal) {
            if (writeTerminal(application)) {
                ++written;
            }
            continue;
        }

        const QList<QString> mimeKeys = mimeKeysFor(component);
        if (mimeKeys.isEmpty()) {
            continue;
        }

        // Resolve the real service by storageId when the system knows it
        // (KService's documented-preferred lookup); fall back to a desktop
        // file constructed from the storageId so an unknown application still
        // writes its storageId without error.
        KService::Ptr service = KService::serviceByStorageId(application);
        if (!service) {
            KDesktopFile desktopFile(application);
            service = KService::Ptr(new KService(&desktopFile, application));
        }
        for (const QString &mimeType : mimeKeys) {
            KApplicationTrader::setPreferredService(mimeType, service);
        }
        ++written;
    }
    return written;
}

} // namespace ads
