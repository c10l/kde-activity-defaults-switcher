/*
    SPDX-License-Identifier: LGPL-3.0-or-later
    SPDX-FileCopyrightText: 2026 c10l

    One Component's picker: shaped like the combo box of System Settings'
    Default Applications page, so the closed field and every list entry show the
    Application's icon beside its name. The entry list is the caller's
    (KcmBackend::pickerEntries) and the selection gesture stays in main.qml, so
    this file only draws the picker and keeps its selection on the row's
    Application.
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.ComboBox {
    id: comboBox

    // The row's effective Application - the entry the picker shows.
    property string application

    textRole: "displayName"
    valueRole: "storageId"

    // Re-derive the selection from both inputs. The row's Application and the
    // entry list are recomputed in the same update but not in a helpful order
    // (a switch onto a profile bound to an Application the picker does not
    // enumerate grows the list after the Application lands), so syncing on the
    // list change as well as the Application change means whichever arrives
    // last still leaves the picker on the entry for the Application.
    onApplicationChanged: syncSelection()
    onModelChanged: syncSelection()
    Component.onCompleted: syncSelection()

    function syncSelection() {
        if (application === "") {
            currentIndex = 0
            return
        }
        for (let i = 0; i < count; ++i) {
            if (model[i].storageId === application) {
                currentIndex = i
                return
            }
        }
        currentIndex = 0
    }

    // The closed field carries the selected Application's icon, as the Default
    // Applications combo does. The org.kde.desktop style reads this hint; other
    // styles simply draw no icon.
    Kirigami.StyleHints.iconName: (currentIndex >= 0 && currentIndex < count) ? (model[currentIndex].icon ?? "") : ""

    // The entry rendering. The KDE styles host the entries in a Menu (a
    // Repeater over the delegate model, or a Menu whose delegate is this one),
    // not in the control's own ListView, so the entry must not size itself from
    // ListView.view - that is null there, and the resulting TypeError left every
    // entry empty and the popup 28px wide. Bind the model item as the styles'
    // own delegates do, and follow the parent the style gives the entry (the
    // Breeze delegate's idiom, which also works in Qt's Basic style, where the
    // parent is the popup's ListView, already as wide as the control).
    delegate: QQC2.MenuItem {
        required property var model
        required property int index

        width: parent ? parent.width : implicitWidth
        text: model[comboBox.textRole]
        icon.name: model.icon
        highlighted: comboBox.highlightedIndex === index
    }
}
