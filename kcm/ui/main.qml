/*
    SPDX-License-Identifier: LGPL-3.0-or-later
    SPDX-FileCopyrightText: 2026 c10l

    The KCM's view: an activity selector, a per-activity Reset, and one row per
    Component showing its effective application. All logic lives in the C++
    KcmBackend (reached as kcm.backend); this file only draws and forwards
    gestures.
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ads 1.0
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami

KCMUtils.SimpleKCM {
    id: root

    // Apply-button semantics: edits are buffered in the backend and written on
    // Apply (the module pins supportsInstantApply false). Offer the Apply and
    // Close buttons. The built-in Default button is module-wide and not used;
    // Reset is per-activity and lives below (ticket 06).
    KCMUtils.ConfigModule.buttons: KCMUtils.ConfigModule.Apply | KCMUtils.ConfigModule.Help

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.gridUnit * 4
            visible: !kcm.backend.hasActivities
            icon.name: "preferences-desktop-default-applications"
            text: i18n("No activities to configure")
        }

        RowLayout {
            Layout.fillWidth: true
            visible: kcm.backend.hasActivities
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                text: i18n("Activity:")
            }

            QQC2.ComboBox {
                id: activitySelector
                Layout.fillWidth: true
                model: kcm.backend.activities
                textRole: "name"
                valueRole: "id"
                enabled: kcm.backend.hasActivities
                onActivated: kcm.backend.selectedActivityId = currentValue

                // Mark the baseline activity in the closed field and the list.
                displayText: {
                    if (currentIndex < 0) {
                        return ""
                    }
                    const activity = model[currentIndex]
                    return activity.id === kcm.backend.baselineActivityId
                        ? i18n("%1 (baseline)", activity.name)
                        : activity.name
                }

                delegate: QQC2.ItemDelegate {
                    id: activityDelegate
                    required property int index
                    required property var modelData
                    width: activitySelector.width
                    highlighted: activitySelector.highlightedIndex === index
                    text: modelData.id === kcm.backend.baselineActivityId
                        ? i18n("%1 (baseline)", modelData.name)
                        : modelData.name
                    onClicked: activitySelector.currentIndex = index
                }

                function syncFromBackend() {
                    for (let i = 0; i < count; ++i) {
                        if (model[i].id === kcm.backend.selectedActivityId) {
                            currentIndex = i
                            return
                        }
                    }
                }

                Component.onCompleted: syncFromBackend()
                Connections {
                    target: kcm.backend
                    function onSelectedActivityIdChanged() {
                        activitySelector.syncFromBackend()
                    }
                }
            }

            QQC2.Button {
                text: i18n("Reset")
                icon.name: "edit-undo"
                enabled: kcm.backend.hasActivities
                onClicked: kcm.backend.resetActivity()
            }
        }

        Repeater {
            model: kcm.backend.componentRows

            delegate: RowLayout {
                id: componentRow
                required property int component
                required property string name
                required property string effectiveApplication
                required property int bindingSource

                Layout.fillWidth: true
                visible: kcm.backend.hasActivities
                enabled: kcm.backend.hasActivities
                spacing: Kirigami.Units.largeSpacing

                onEffectiveApplicationChanged: componentPicker.syncSelection()

                QQC2.Label {
                    Layout.fillWidth: true
                    text: componentRow.name
                }

                QQC2.ComboBox {
                    id: componentPicker
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                    enabled: kcm.backend.hasActivities
                    // The Inherit entry's label depends on the baseline, so
                    // refresh the list after an edit (the extra reads are the
                    // dependencies that make the binding re-evaluate).
                    model: {
                        componentRow.effectiveApplication
                        componentRow.bindingSource
                        return kcm.backend.pickerEntries(componentRow.component)
                    }
                    textRole: "displayName"
                    valueRole: "storageId"
                    font.italic: componentRow.bindingSource === Ads.BindingSource.Fallback

                    function syncSelection() {
                        if (componentRow.effectiveApplication === "") {
                            currentIndex = 0
                            return
                        }
                        for (let i = 0; i < model.length; ++i) {
                            if (model[i].storageId === componentRow.effectiveApplication) {
                                currentIndex = i
                                return
                            }
                        }
                        currentIndex = 0
                    }

                    Component.onCompleted: syncSelection()

                    onActivated: {
                        const entry = model[currentIndex]
                        if (entry.isInheritEntry) {
                            // Clearing falls back to the baseline (ticket 05).
                            kcm.backend.clearBinding(componentRow.component)
                        } else {
                            kcm.backend.setBinding(componentRow.component, entry.storageId)
                        }
                    }
                }
            }
        }
    }
}
