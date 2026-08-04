import QtQuick
import QtQuick.Controls.Basic as QQC
import QtQuickTerminal

QQC.ApplicationWindow {
    id: root

    width: 1280
    height: 820
    minimumWidth: 760
    minimumHeight: 480
    visible: true
    color: Theme.background
    title: workspace.activeTab
           ? qsTr("%1 - Qt Quick Terminal").arg(workspace.activeTab.tabTitle)
           : qsTr("Qt Quick Terminal")

    // Keeps the native title bar in step with the in-app palette.
    Component.onCompleted: themeHelper.setColorScheme(Theme.dark)

    Connections {
        target: Theme
        function onDarkChanged() { themeHelper.setColorScheme(Theme.dark) }
    }

    TabWorkspace {
        id: workspace
        anchors.fill: parent
    }
}
