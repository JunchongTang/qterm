import QtQuick
import QtQuick.Controls

MenuBar {
    id: root

    // Reflects the currently active variant.
    property bool isDark: true

    signal darkThemeRequested()
    signal lightThemeRequested()

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
}
