import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: statusBar

    required property string titleText
    required property string sessionStateText
    required property string geometryText
    required property string cursorText
    required property string selectionText
    required property string noteText

    implicitHeight: 42
    color: "#fbfbfd"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: "#d7dbe2"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 10

        Text {
            Layout.preferredWidth: 220
            Layout.alignment: Qt.AlignVCenter
            elide: Text.ElideRight
            color: "#111827"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            text: statusBar.titleText
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            color: "#6b7280"
            font.pixelSize: 12
            text: statusBar.sessionStateText
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            color: "#6b7280"
            font.pixelSize: 12
            text: statusBar.geometryText
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            color: "#6b7280"
            font.pixelSize: 12
            text: statusBar.cursorText
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            color: "#6b7280"
            font.pixelSize: 12
            text: statusBar.selectionText
        }

        Item {
            Layout.fillWidth: true
        }

        Text {
            Layout.maximumWidth: 420
            Layout.alignment: Qt.AlignVCenter
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            color: "#4b5563"
            font.pixelSize: 12
            text: statusBar.noteText
        }
    }
}
