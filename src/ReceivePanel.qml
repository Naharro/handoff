import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The laptop that receives: the address to type on the other one, and the
// switch that lets it in.
Item {
    id: panel

    Material.roundedScale: theme.radius

    property bool copied: false
    function copy() {
        if (receiver.target === "")
            return
        receiver.copyTarget()
        copied = true
        copiedTimer.restart()
    }
    Timer { id: copiedTimer; interval: 1600; onTriggered: panel.copied = false }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, 620)
        spacing: 18

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: receiver.ready ? qsTr("Ready to receive") : qsTr("Not receiving")
            color: receiver.ready ? theme.accent : "#a4a4ad"
            font.pixelSize: 16
            font.weight: Font.Medium
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: addressColumn.implicitHeight + 44
            radius: theme.radius
            color: "#141417"
            border.color: receiver.ready ? theme.accent : "#26262b"

            Column {
                id: addressColumn
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 10
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: receiver.target !== "" ? receiver.target : qsTr("No network")
                    color: receiver.ready ? "#ffffff" : "#7d7d86"
                    font.pixelSize: 34
                    font.family: "monospace"
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 16
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: panel.copied ? qsTr("Copied.")
                        : receiver.ready ? qsTr("Type this address in Handoff on the laptop that sends, with this laptop's password.")
                        : qsTr("This will be the address once the laptop is receiving.")
                    color: "#8b8b94"
                    font.pixelSize: 13
                }
            }
        }

        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 2
            columnSpacing: 16
            rowSpacing: 6

            Text { text: qsTr("SSH server"); color: "#8b8b94"; font.pixelSize: 13 }
            Text {
                text: receiver.sshRunning ? qsTr("Running") : qsTr("Stopped")
                color: "#e8e8ea"; font.pixelSize: 13
            }
            Text { text: qsTr("Firewall"); color: "#8b8b94"; font.pixelSize: 13 }
            Text {
                text: !receiver.firewallActive ? qsTr("Off")
                    : receiver.firewallOpen ? qsTr("Lets SSH in") : qsTr("Blocks SSH")
                color: "#e8e8ea"; font.pixelSize: 13
            }
            Text { text: qsTr("Network"); color: "#8b8b94"; font.pixelSize: 13 }
            Text {
                text: receiver.subnet !== "" ? receiver.subnet : "—"
                color: "#e8e8ea"; font.pixelSize: 13; font.family: "monospace"
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            ActionButton {
                visible: receiver.opened || !receiver.ready
                primary: !receiver.opened
                enabled: !receiver.busy
                text: receiver.opened ? qsTr("Stop receiving") : qsTr("Start receiving")
                key: "Space"
                onClicked: receiver.toggle()
            }
            ActionButton {
                visible: receiver.target !== ""
                text: qsTr("Copy address")
                key: "Y"
                onClicked: panel.copy()
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: receiver.error !== "" ? "#ff6b6b" : "#6c6c75"
            font.pixelSize: 13
            lineHeight: 1.3
            text: receiver.error !== "" ? receiver.error
                : receiver.busy ? qsTr("Waiting for permission…")
                : receiver.opened ? qsTr("Stopping closes the firewall again and turns the SSH server off.")
                : receiver.ready ? qsTr("SSH was already on before Handoff, so it is left as it is.")
                : qsTr("Starting asks for your password, turns the SSH server on until you stop it, and lets only this network through the firewall.")
        }
    }
}
