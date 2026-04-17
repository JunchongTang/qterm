import QtQuick
import QtQuick.Window
import QTerm

Window {
    width: 960
    height: 640
    visible: true
    title: "QTerm Quick Demo"
    color: "#111111"

    Rectangle {
        anchors.fill: parent
        anchors.margins: 24
        radius: 12
        color: "#1a1a1a"
        border.width: 1
        border.color: "#333333"

        TerminalView {
            id: terminalView
            anchors.fill: parent
            anchors.margins: 16
            backgroundColor: "#101010"
            model: terminalSurfaceModel
            session: terminalSession
        }
    }
}