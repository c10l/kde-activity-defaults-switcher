// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "kopenwithdialog_chooser.h"

#include "writer.h"

#include <KOpenWithDialog>
#include <KService>

#include <QApplication>
#include <QDialog>
#include <QUrl>

namespace ads {

std::optional<Application> KOpenWithDialogChooser::choose(Component component)
{
    // The shape the Default Applications page uses: an empty URL list, the
    // Component's canonical mime type as what the dialog asks about (empty for
    // the terminal, which then lists every Application), and the default caption
    // and entry text.
    KOpenWithDialog dialog(QList<QUrl>(), primaryMimeTypeFor(component), QString(), QString(),
                           QApplication::activeWindow());
    dialog.setSaveNewApplications(true);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    const KService::Ptr service = dialog.service();
    if (!service) {
        return std::nullopt;
    }

    // A bare command has no desktop entry and so no storageId, and a Binding
    // holds a storageId: nothing to record, and the picker re-derives its
    // selection.
    const Application storageId = service->storageId();
    if (storageId.isEmpty()) {
        return std::nullopt;
    }
    return storageId;
}

} // namespace ads
