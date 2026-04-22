import QtQuick
import QtQuick.Window

Window {
    id: root

    required property var terminal

    width: 960
    height: 640
    visible: true
    color: "#10151c"
    title: root.terminal.title

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#182432" }
            GradientStop { position: 1.0; color: "#0b0f14" }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 28
        radius: 18
        color: "#0f1722"
        border.width: 1
        border.color: "#2b3a4b"
        opacity: 0.96
    }

    Text {
        id: headline

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 44
        color: "#dce7f3"
        font.family: "Helvetica Neue"
        font.pixelSize: 28
        font.weight: Font.DemiBold
        text: "QTerm Architecture Baseline"
    }

    Text {
        anchors.left: headline.left
        anchors.top: headline.bottom
        anchors.topMargin: 10
        color: "#8ea2b8"
        font.family: "Helvetica Neue"
        font.pixelSize: 15
        text: "Qt-native terminal stack, structured around xterm.js-inspired layering."
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headline.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 44
        anchors.topMargin: 56
        radius: 14
        color: "#0a0f15"
        border.width: 1
        border.color: "#1f2c3a"
    }

    TextEdit {
        anchors.fill: parent
        anchors.margins: 64
        readOnly: true
        selectByMouse: true
        color: "#d2f7d0"
        text: root.terminal.surfaceModel.plainText
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.PlainText
        font.family: "Menlo"
        font.pixelSize: 18
        selectionColor: "#2d5f4d"
        selectedTextColor: "#f1fff1"
    }
}