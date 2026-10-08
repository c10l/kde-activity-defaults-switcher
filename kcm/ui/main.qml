/*
    SPDX-License-Identifier: LGPL-3.0-or-later
    SPDX-FileCopyrightText: 2026 c10l

    The KCM's view: an activity selector, a per-activity Reset, and the
    Components in a form laid out like System Settings' Default Applications
    page - category sections, right-aligned labels, and one icon picker per
    Component. All logic lives in the C++ KcmBackend (reached as kcm.backend);
    this file only draws and forwards gestures.
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
                objectName: "activitySelector"
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

        // The same two-column form the Default Applications page uses: a
        // right-aligned label per Component, under category headings. The rows
        // come from the backend's model, section-header rows included, so the
        // grouping and the reading order live in one place.
        Kirigami.FormLayout {
            id: form

            Repeater {
                model: kcm.backend.componentRows

                delegate: Item {
                    id: formRow
                    required property bool isSection
                    required property string section
                    required property int component
                    required property string name
                    required property string effectiveApplication
                    required property int bindingSource

                    // A section-header row is an empty item: FormLayout renders
                    // its label as the heading spanning both columns.
                    Kirigami.FormData.isSection: isSection
                    Kirigami.FormData.label: isSection ? section : name
                    Layout.preferredWidth: isSection ? 0 : Kirigami.Units.gridUnit * 18
                    implicitWidth: isSection ? 0 : picker.implicitWidth
                    implicitHeight: isSection ? 0 : picker.implicitHeight
                    visible: kcm.backend.hasActivities
                    enabled: kcm.backend.hasActivities

                    ComponentComboBox {
                        id: picker
                        objectName: "componentPicker"
                        anchors.left: parent.left
                        width: parent.width
                        visible: !formRow.isSection
                        enabled: formRow.enabled
                        // The picker shows (and re-derives) the row's effective
                        // Application.
                        application: formRow.effectiveApplication
                        // Italic marks a fallback binding (the Application comes
                        // from the baseline activity, not this one).
                        font.italic: formRow.bindingSource === Ads.BindingSource.Fallback
                        // The Inherit entry's label depends on the baseline, so
                        // refresh the list after an edit (the extra reads are the
                        // dependencies that make the binding re-evaluate).
                        model: {
                            formRow.effectiveApplication
                            formRow.bindingSource
                            return kcm.backend.pickerEntries(formRow.component)
                        }

                        onActivated: {
                            const entry = model[currentIndex]
                            if (entry.isInheritEntry) {
                                // Clearing falls back to the baseline (ticket 05).
                                kcm.backend.clearBinding(formRow.component)
                            } else {
                                kcm.backend.setBinding(formRow.component, entry.storageId)
                            }
                        }
                    }
                }
            }
        }
    }
}
