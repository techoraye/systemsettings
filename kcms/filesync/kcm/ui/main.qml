import QtCore
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    property string notice: ""
    property bool noticeIsError: false

    Timer { interval: 4000; running: true; repeat: true; onTriggered: kcm.refresh() }

    header: ColumnLayout {
        spacing: 0
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !kcm.nasMounted
            type: Kirigami.MessageType.Warning
            text: i18n("The NAS is not mounted. Syncing is skipped until it comes back.")
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.notice !== ""
            type: root.noticeIsError ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            text: root.notice
            actions: [ Kirigami.Action { icon.name: "dialog-close"; onTriggered: root.notice = "" } ]
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // ---------------- status ----------------
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: kcm.lastError.length > 0
            text: kcm.lastError
        }

        Kirigami.AbstractCard {
            Layout.fillWidth: true
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon {
                    source: kcm.running ? "view-refresh"
                          : kcm.unitsActive ? "folder-sync"
                          : kcm.syncEnabled ? "dialog-warning"
                          : "media-playback-pause"
                    implicitWidth: Kirigami.Units.iconSizes.large
                    implicitHeight: Kirigami.Units.iconSizes.large
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    QQC2.Label {
                        // unitsActive, not syncEnabled: the first asks systemd what
                        // is running, the second only repeats what the config file
                        // says. They disagree whenever enabling failed, and it is
                        // precisely then that the user needs to be told.
                        text: kcm.running ? i18n("Syncing now")
                             : kcm.unitsActive ? i18n("Watching for changes")
                             : kcm.syncEnabled ? i18n("Switched on, but nothing is running")
                             : i18n("Automatic sync is off")
                        font.bold: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: kcm.lastRun
                        elide: Text.ElideMiddle
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }
                QQC2.Switch {
                    id: master
                    checked: kcm.syncEnabled
                    onClicked: { kcm.syncEnabled = checked; kcm.apply() }
                }
            }
        }

        // ---------------- timing ----------------
        Kirigami.FormLayout {
            Layout.fillWidth: true

            Item { Kirigami.FormData.isSection: true; Kirigami.FormData.label: i18n("When to sync") }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18n("After idle for:")
                from: 1; to: 3600; value: kcm.debounce
                onValueModified: kcm.debounce = value
                textFromValue: function(v) { return i18np("%1 second", "%1 seconds", v) }
                valueFromText: function(t) { return parseInt(t) || 5 }
            }
            QQC2.Label {
                text: i18n("A sync runs once nothing has changed for this long.")
                font: Kirigami.Theme.smallFont; opacity: 0.7
            }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18n("Full sweep every:")
                from: 1; to: 1440; value: kcm.periodicMinutes
                onValueModified: kcm.periodicMinutes = value
                textFromValue: function(v) { return i18np("%1 minute", "%1 minutes", v) }
                valueFromText: function(t) { return parseInt(t) || 15 }
            }
            QQC2.Label {
                text: i18n("Changes made on the NAS by another machine are invisible to the file\nwatcher, so a full comparison runs on this interval as well.")
                font: Kirigami.Theme.smallFont; opacity: 0.7
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("If both sides changed:")
                model: [ i18n("Keep the newer copy"), i18n("Leave both and report a conflict") ]
                currentIndex: kcm.conflictPolicy === "newer" ? 0 : 1
                onActivated: kcm.conflictPolicy = (currentIndex === 0 ? "newer" : "none")
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18n("Notifications:")
                text: i18n("Notify when a sync finishes")
                checked: kcm.notifyOnSync
                onToggled: kcm.notifyOnSync = checked
            }
        }

        // ---------------- pairs ----------------
        Kirigami.Separator { Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading { level: 3; text: i18n("Folder pairs") }
            Item { Layout.fillWidth: true }
            QQC2.Button {
                text: i18n("Add pair"); icon.name: "list-add"
                onClicked: addSheet.open()
            }
        }

        Repeater {
            model: kcm.pairs
            delegate: Kirigami.AbstractCard {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        QQC2.CheckBox {
                            checked: modelData.enabled
                            onToggled: kcm.setPairValue(modelData.key, "enabled", checked ? "true" : "false")
                        }
                        Kirigami.Heading { level: 4; text: modelData.name }
                        Item { Layout.fillWidth: true }
                        QQC2.ToolButton {
                            icon.name: "view-preview"
                            text: i18n("Preview")
                            display: QQC2.AbstractButton.TextBesideIcon
                            onClicked: kcm.previewPair(modelData.key)
                        }
                        QQC2.ToolButton {
                            icon.name: "folder-sync"
                            text: i18n("Sync")
                            display: QQC2.AbstractButton.TextBesideIcon
                            enabled: kcm.nasMounted
                            onClicked: kcm.syncPair(modelData.key)
                        }
                        QQC2.ToolButton {
                            icon.name: "list-remove"
                            onClicked: kcm.removePair(modelData.key)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: modelData.local
                            onEditingFinished: kcm.setPairValue(modelData.key, "local", text)
                        }
                        QQC2.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 11
                            model: [ i18n("Two-way"), i18n("Local → NAS"), i18n("NAS → Local") ]
                            currentIndex: modelData.direction === "push" ? 1
                                        : modelData.direction === "pull" ? 2 : 0
                            onActivated: kcm.setPairValue(modelData.key, "direction",
                                          currentIndex === 1 ? "push" : currentIndex === 2 ? "pull" : "twoway")
                        }
                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: modelData.remote
                            onEditingFinished: kcm.setPairValue(modelData.key, "remote", text)
                        }
                    }
                }
            }
        }

        // ---------------- filters ----------------
        Kirigami.Separator { Layout.fillWidth: true }
        Kirigami.Heading { level: 3; text: i18n("Never sync") }
        QQC2.Label {
            text: i18n("One pattern per line, for example  *.iso  or  node_modules")
            font: Kirigami.Theme.smallFont; opacity: 0.7
        }
        QQC2.TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 5
            text: kcm.ignorePatterns
            onEditingFinished: kcm.ignorePatterns = text
        }

        // ---------------- actions ----------------
        Kirigami.Separator { Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            QQC2.Button {
                text: i18n("Preview everything"); icon.name: "view-preview"
                enabled: kcm.nasMounted
                onClicked: kcm.previewAll()
            }
            QQC2.Button {
                text: i18n("Sync now"); icon.name: "folder-sync"
                enabled: kcm.nasMounted
                onClicked: kcm.syncNow()
            }
            QQC2.Button {
                text: i18n("Open log"); icon.name: "text-x-log"
                onClicked: kcm.openLog()
            }
            Item { Layout.fillWidth: true }
            QQC2.Button {
                text: i18n("Export preset…"); icon.name: "document-save-as"
                onClicked: exportDialog.open()
            }
            QQC2.Button {
                text: i18n("Import preset…"); icon.name: "document-open"
                onClicked: importDialog.open()
            }
        }
        QQC2.Label {
            text: i18n("A preset is the whole configuration in one file. Import it on another\nmachine to recreate these pairs. It always arrives switched off.")
            font: Kirigami.Theme.smallFont; opacity: 0.7
        }
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Save preset")
        fileMode: Dialogs.FileDialog.SaveFile
        currentFile: "file://" + StandardPaths.writableLocation(StandardPaths.HomeLocation) + "/filesync-preset.conf"
        nameFilters: [ i18n("Sync preset (*.conf)") ]
        onAccepted: {
            var err = kcm.exportPreset(selectedFile.toString())
            root.noticeIsError = err !== ""
            root.notice = err !== "" ? err : i18n("Preset saved.")
        }
    }
    Dialogs.FileDialog {
        id: importDialog
        title: i18n("Open preset")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: [ i18n("Sync preset (*.conf)"), i18n("All files (*)") ]
        onAccepted: {
            var err = kcm.importPreset(selectedFile.toString())
            root.noticeIsError = err !== ""
            root.notice = err !== "" ? err : i18n("Preset imported. Check the paths, then switch sync on.")
        }
    }

    Kirigami.OverlaySheet {
        id: addSheet
        title: i18n("Add a folder pair")

        // A FormLayout handed straight to an OverlaySheet contributes almost no
        // implicit width of its own, so the sheet collapsed to a bare vertical
        // strip showing only its close button. Wrap it and state a width.
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            implicitWidth: Kirigami.Units.gridUnit * 28

            Kirigami.FormLayout {
                Layout.fillWidth: true
                QQC2.TextField {
                    id: pName
                    Kirigami.FormData.label: i18n("Name:")
                    Layout.fillWidth: true
                    placeholderText: i18n("Documents")
                }
                QQC2.TextField {
                    id: pLocal
                    Kirigami.FormData.label: i18n("Local folder:")
                    Layout.fillWidth: true
                    placeholderText: "/home/…/Documents"
                }
                QQC2.TextField {
                    id: pRemote
                    Kirigami.FormData.label: i18n("Other folder:")
                    Layout.fillWidth: true
                    placeholderText: "/mnt/nas/…/Documents"
                }
            }
        }

        // The confirm button belongs in the footer, not inside the form: in the
        // form it inherited the label column and sat off to one side.
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            QQC2.Button {
                text: i18n("Add")
                icon.name: "list-add"
                // Adding a nameless pair writes a section called "Pair_" that
                // nothing can address afterwards.
                enabled: pName.text.trim().length > 0
                      && pLocal.text.trim().length > 0
                      && pRemote.text.trim().length > 0
                onClicked: {
                    kcm.addPair(pName.text.trim(), pLocal.text.trim(), pRemote.text.trim())
                    pName.text = ""; pLocal.text = ""; pRemote.text = ""
                    addSheet.close()
                }
            }
        }
    }
}
