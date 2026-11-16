import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A button that names the key that does the same thing.
AbstractButton {
    id: button

    property string key: ""
    property bool primary: false

    focusPolicy: Qt.NoFocus
    padding: 10
    leftPadding: 14
    rightPadding: 14
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: theme.radius
        color: button.primary ? theme.accent : button.hovered ? "#26262b" : "#1b1b1f"
        border.color: button.primary ? theme.accent : "#2e2e34"
    }

    contentItem: Row {
        spacing: 10
        Text {
            text: button.text
            color: button.primary ? theme.accentForeground : "#e8e8ea"
            font.pixelSize: 14
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            visible: button.key !== ""
            anchors.verticalCenter: parent.verticalCenter
            width: keyText.implicitWidth + 10
            height: keyText.implicitHeight + 4
            radius: Math.min(4, theme.radius)
            color: button.primary ? Qt.rgba(0, 0, 0, 0.18) : "#2a2a30"
            Text {
                id: keyText
                anchors.centerIn: parent
                text: button.key
                color: button.primary ? theme.accentForeground : "#9a9aa3"
                font.pixelSize: 11
                font.family: "monospace"
            }
        }
    }
}
