#include "writer.h"

#include <KApplicationTrader>
#include <KDesktopFile>
#include <KService>

namespace ads {

namespace {

QList<QString> mimeKeysFor(Component component)
{
    switch (component) {
    case Component::Browser:
        return {
            QStringLiteral("text/html"),
            QStringLiteral("x-scheme-handler/http"),
            QStringLiteral("x-scheme-handler/https"),
        };
    default:
        // Ticket 05 extends this to the full 13-component map; the terminal
        // component is written to kdeglobals in ticket 06.
        return {};
    }
}

} // namespace

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
