import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 900
    height: 660
    minimumWidth: 680
    minimumHeight: 520
    visible: true
    title: "Handoff"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool quitVisible: false
    property bool quitting: false
    // 0 is the laptop that sends, 1 the one that receives.
    property int side: 0
    readonly property bool overlay: helpVisible || quitVisible
    readonly property bool sending: side === 0 && !overlay
    readonly property bool receiving: side === 1 && !overlay

    function dropFocus() { root.forceActiveFocus() }
    function askToQuit() {
        // A transfer in flight or a laptop left open is worth a question.
        if (backend.running || receiver.opened)
            quitVisible = true
        else
            Qt.quit()
    }
    function leave() { quitting = true; Qt.quit() }

    onClosing: (close) => {
        if (!quitting && (backend.running || receiver.opened)) {
            close.accepted = false
            quitVisible = true
        }
    }

    Connections {
        target: receiver
        function onChanged() {
            // "Stop receiving and quit" leaves once the door is really shut.
            if (win.quitting && !receiver.opened && !receiver.busy)
                Qt.quit()
            else if (win.quitting && !receiver.busy && receiver.error !== "") {
                win.quitting = false
                win.quitVisible = false
                win.side = 1
            }
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.overlay; onActivated: win.askToQuit() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: !win.overlay; onActivated: win.side = 0 }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: !win.overlay; onActivated: win.side = 1 }
    Shortcut {
        sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.overlay
        onActivated: win.side === 0 ? backend.send() : receiver.toggle()
    }
    Shortcut {
        sequence: "Escape"; context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (win.quitVisible) win.quitVisible = false
            else if (win.activeFocusItem !== root) win.dropFocus()
            else backend.cancel()
        }
    }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: win.sending && !backend.running; onActivated: backend.addFolders() }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; enabled: win.sending && !backend.running; onActivated: backend.addFiles() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.sending; onActivated: sendPanel.focusTarget() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.sending; onActivated: backend.checkConnection() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: win.sending; onActivated: backend.preview() }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: win.sending; onActivated: sendPanel.move(-1) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: win.sending; onActivated: sendPanel.move(1) }
    Shortcut { sequences: ["Delete", "Backspace"]; context: Qt.ApplicationShortcut; enabled: win.sending && !backend.running; onActivated: sendPanel.removeSelected() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: win.sending && !backend.running; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: win.sending && !backend.running; onActivated: backend.redo() }
    Shortcut { sequence: "Y"; context: Qt.ApplicationShortcut; enabled: win.receiving; onActivated: receivePanel.copy() }
    Shortcut {
        sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; enabled: win.quitVisible
        onActivated: {
            if (receiver.opened && !backend.running) { win.quitting = true; receiver.close() }
            else win.leave()
        }
    }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: win.quitVisible && receiver.opened; onActivated: win.leave() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        id: root
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        // A click on empty space hands the keyboard back to the shortcuts.
        MouseArea { anchors.fill: parent; onClicked: win.dropFocus() }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [{ label: qsTr("Send"), key: "S" }, { label: qsTr("Receive"), key: "R" }]
                    delegate: Item {
                        id: tab
                        required property int index
                        required property var modelData
                        implicitWidth: tabRow.implicitWidth + 24
                        implicitHeight: 38

                        Rectangle {
                            anchors.fill: parent
                            radius: theme.radius
                            color: win.side === tab.index ? "#1f1f24" : "transparent"
                            border.color: win.side === tab.index ? "#34343b" : "transparent"
                        }
                        Row {
                            id: tabRow
                            anchors.centerIn: parent
                            spacing: 8
                            Text {
                                text: tab.modelData.label
                                color: win.side === tab.index ? "#ffffff" : "#8b8b94"
                                font.pixelSize: 15
                                font.weight: Font.Medium
                            }
                            Text {
                                text: tab.modelData.key
                                color: win.side === tab.index ? theme.accent : "#5c5c65"
                                font.pixelSize: 11
                                font.family: "monospace"
                                anchors.baseline: parent.children[0].baseline
                            }
                        }
                        MouseArea { anchors.fill: parent; onClicked: { win.side = tab.index; win.dropFocus() } }
                    }
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: receiver.opened
                    width: 8; height: 8; radius: 4
                    color: theme.accent
                }
                Text {
                    visible: receiver.opened
                    text: qsTr("Receiving")
                    color: "#a4a4ad"
                    font.pixelSize: 12
                }
                Text {
                    text: qsTr("? for keys")
                    color: "#5c5c65"
                    font.pixelSize: 12
                    Layout.leftMargin: 10
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.side
                SendPanel { id: sendPanel }
                ReceivePanel { id: receivePanel }
            }
        }

        // Every key, on one sheet.
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: Qt.rgba(0, 0, 0, 0.72)
            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 60, 520)
                height: keys.implicitHeight + 44
                radius: theme.radius
                color: "#17171a"
                border.color: "#2e2e34"

                GridLayout {
                    id: keys
                    anchors.centerIn: parent
                    width: parent.width - 48
                    columns: 2
                    columnSpacing: 22
                    rowSpacing: 7

                    Repeater {
                        model: [
                            ["S / R", qsTr("Send or receive")],
                            ["Space", qsTr("Send, or start and stop receiving")],
                            ["Esc", qsTr("Stop the transfer, or leave a text field")],
                            ["T", qsTr("Type the other laptop's address")],
                            ["C", qsTr("Check the connection")],
                            ["A / F", qsTr("Add folders or files")],
                            ["↑ / ↓", qsTr("Move through the list")],
                            ["Delete", qsTr("Take the selected one off the list")],
                            ["Ctrl+Z / Ctrl+Shift+Z", qsTr("Undo and redo")],
                            ["P", qsTr("Preview what would be sent")],
                            ["Y", qsTr("Copy this laptop's address")],
                            ["?", qsTr("Show or hide this")],
                            ["Q", qsTr("Quit")]
                        ]
                        delegate: Item {
                            id: entry
                            required property var modelData
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            implicitHeight: Math.max(keyLabel.implicitHeight, what.implicitHeight)
                            Text {
                                id: keyLabel
                                width: 190
                                text: entry.modelData[0]
                                color: theme.accent
                                font.pixelSize: 13
                                font.family: "monospace"
                            }
                            Text {
                                id: what
                                anchors.left: keyLabel.right
                                anchors.right: parent.right
                                text: entry.modelData[1]
                                color: "#e8e8ea"
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

        // Asked before leaving work or an open door behind.
        Rectangle {
            anchors.fill: parent
            visible: win.quitVisible
            color: Qt.rgba(0, 0, 0, 0.72)
            MouseArea { anchors.fill: parent }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 60, 520)
                height: quitColumn.implicitHeight + 48
                radius: theme.radius
                color: "#17171a"
                border.color: "#2e2e34"

                ColumnLayout {
                    id: quitColumn
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 18

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#e8e8ea"
                        font.pixelSize: 15
                        lineHeight: 1.3
                        text: backend.running ? qsTr("A transfer is still running. Quitting stops it; sending again later carries on where it left off.")
                                              : qsTr("This laptop is still open to receive.")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        ActionButton {
                            primary: true
                            key: "Enter"
                            enabled: !receiver.busy
                            text: backend.running ? qsTr("Stop and quit") : qsTr("Stop receiving and quit")
                            onClicked: {
                                if (receiver.opened && !backend.running) { win.quitting = true; receiver.close() }
                                else win.leave()
                            }
                        }
                        ActionButton {
                            visible: receiver.opened && !backend.running
                            key: "L"
                            text: qsTr("Leave it open and quit")
                            onClicked: win.leave()
                        }
                        ActionButton { key: "Esc"; text: qsTr("Stay"); onClicked: win.quitVisible = false }
                    }
                }
            }
        }
    }
}
