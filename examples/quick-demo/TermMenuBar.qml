import QtQuick
import QtQuick.Controls

MenuBar {
    id: root

    // Reflects the currently active variant.
    property bool isDark: true
    property bool isSGRenderer: false

    signal darkThemeRequested()
    signal lightThemeRequested()
    signal sgRendererRequested()
    signal paintedRendererRequested()

    Menu {
        title: qsTr("View")

        Action {
            text: qsTr("Dark Theme")
            checkable: true
            checked: root.isDark
            onTriggered: root.darkThemeRequested()
        }

        Action {
            text: qsTr("Light Theme")
            checkable: true
            checked: !root.isDark
            onTriggered: root.lightThemeRequested()
        }
    }

    Menu {
        title: qsTr("Renderer")

        Action {
            text: qsTr("QPainter (QQuickPaintedItem)")
            checkable: true
            checked: !root.isSGRenderer
            onTriggered: root.paintedRendererRequested()
        }

        Action {
            text: qsTr("Scene Graph (QQuickItem / QSG)")
            checkable: true
            checked: root.isSGRenderer
            onTriggered: root.sgRendererRequested()
        }
    }
}
