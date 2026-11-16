import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The laptop that sends: who to send to, what to send, and how it's going.
Item {
    id: panel

    Material.roundedScale: theme.radius

    property int selected: 0
    readonly property int count: backend.sources.length

    onCountChanged: selected = Math.max(0, Math.min(selected, count - 1))

    function focusTarget() { targetField.forceActiveFocus(); targetField.selectAll() }
    function move(step) { selected = Math.max(0, Math.min(count - 1, selected + step)) }
    function removeSelected() { if (count > 0) backend.removeAt(selected) }
    function shorten(path) {
        return path.indexOf(backend.home + "/") === 0 ? "~" + path.substring(backend.home.length) : path
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            TextField {
                id: targetField
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                placeholderText: qsTr("Other laptop (user@address)")
                text: backend.target
                font.family: "monospace"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onTextEdited: backend.target = text
                onAccepted: passwordField.forceActiveFocus()
            }
            TextField {
                id: passwordField
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                placeholderText: qsTr("Its password")
                echoMode: TextInput.Password
                onTextEdited: backend.password = text
                onAccepted: { panel.forceActiveFocus(); backend.checkConnection() }
            }
            ActionButton {
                text: qsTr("Check")
                key: "C"
                enabled: !backend.running && !backend.checking
                onClicked: backend.checkConnection()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: theme.radius
            color: "#141417"
            border.color: drop.containsDrag ? theme.accent : "#26262b"

            DropArea {
                id: drop
                anchors.fill: parent
                onDropped: (event) => { if (event.hasUrls) backend.addUrls(event.urls) }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 80
                visible: panel.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: "#7d7d86"
                font.pixelSize: 15
                lineHeight: 1.4
                text: qsTr("Nothing to send yet.\nPress A to add folders, F to add files, or drop them here.")
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                model: backend.sources
                currentIndex: panel.selected
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    id: row
                    required property int index
                    required property string modelData
                    width: list.width
                    height: 38

                    Rectangle {
                        anchors.fill: parent
                        radius: Math.min(4, theme.radius)
                        color: row.index === panel.selected ? Qt.alpha(theme.accent, 0.16) : "transparent"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: panel.selected = row.index
                    }
                    Rectangle {
                        visible: row.index === panel.selected
                        width: 3
                        height: parent.height - 14
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        color: theme.accent
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.right: remove.left
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: panel.shorten(row.modelData)
                        elide: Text.ElideMiddle
                        color: "#e8e8ea"
                        font.pixelSize: 14
                        font.family: "monospace"
                    }
                    Text {
                        id: remove
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "×"
                        color: removeArea.containsMouse ? "#e8e8ea" : "#6c6c75"
                        font.pixelSize: 20
                        MouseArea {
                            id: removeArea
                            anchors.fill: parent
                            anchors.margins: -8
                            hoverEnabled: true
                            onClicked: backend.removeAt(row.index)
                        }
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 4

            RadioButton {
                focusPolicy: Qt.NoFocus
                text: qsTr("Same place in the other home folder")
                checked: backend.mirrorHome
                onClicked: backend.mirrorHome = true
            }
            RadioButton {
                focusPolicy: Qt.NoFocus
                text: qsTr("Into the folder")
                checked: !backend.mirrorHome
                onClicked: { backend.mirrorHome = false; intoField.forceActiveFocus() }
            }
            TextField {
                id: intoField
                width: 180
                enabled: !backend.mirrorHome
                text: backend.into
                font.family: "monospace"
                onTextEdited: backend.into = text
                onAccepted: panel.forceActiveFocus()
            }
            CheckBox {
                focusPolicy: Qt.NoFocus
                text: qsTr("Keep files that are newer there")
                checked: backend.keepNewer
                onClicked: backend.keepNewer = checked
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 100
                value: backend.percent
                indeterminate: (backend.running && backend.percent === 0) || backend.checking
                opacity: backend.state === "idle" ? 0.25 : 1
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    text: backend.message !== "" ? backend.message : qsTr("Press Space to send.")
                    color: backend.state === "failed" ? "#ff6b6b" : backend.state === "done" || backend.state === "checked" ? "#e8e8ea" : "#a4a4ad"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
                Text {
                    // Reads the language so the numbers re-render on a switch.
                    visible: backend.running && backend.speed !== ""
                    text: translator.language, qsTr("%1% · %2 · %3 left").arg(backend.percent).arg(backend.speed).arg(backend.remaining)
                    color: "#a4a4ad"
                    font.pixelSize: 13
                    font.family: "monospace"
                }
            }
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: backend.running ? backend.currentFile : backend.detail
                color: "#6c6c75"
                font.pixelSize: 12
                font.family: "monospace"
                elide: Text.ElideMiddle
                maximumLineCount: 3
                wrapMode: backend.running ? Text.NoWrap : Text.WrapAnywhere
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ActionButton { text: qsTr("Add folders"); key: "A"; enabled: !backend.running; onClicked: backend.addFolders() }
            ActionButton { text: qsTr("Add files"); key: "F"; enabled: !backend.running; onClicked: backend.addFiles() }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: qsTr("Preview")
                key: "P"
                enabled: !backend.running && !backend.checking
                onClicked: backend.preview()
            }
            ActionButton {
                primary: !backend.running
                text: backend.running ? qsTr("Stop") : qsTr("Send")
                key: backend.running ? "Esc" : "Space"
                enabled: !backend.checking
                onClicked: backend.running ? backend.cancel() : backend.send()
            }
        }
    }
}
