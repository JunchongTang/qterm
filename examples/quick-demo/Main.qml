pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root

    width: 1200
    height: 800
    minimumWidth: 600
    minimumHeight: 400
    visible: true
    color: "#0d1117"
    title: {
        const tab = tabManager.currentIndex >= 0 && tabManager.tabs.length > tabManager.currentIndex
            ? tabManager.tabs[tabManager.currentIndex]
            : null
        return tab ? tab.tabTitle + " \u2014 QTerm" : "QTerm"
    }

    TabManager {
        id: tabManager
        anchors.fill: parent
        anchors.bottomMargin: statusBar.height
    }

    TermStatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        readonly property var activeTab: (tabManager.currentIndex >= 0 && tabManager.tabs.length > tabManager.currentIndex)
            ? tabManager.tabs[tabManager.currentIndex]
            : null

        titleText: activeTab ? activeTab.tabTitle : ""
        sessionStateText: {
            if (!activeTab)
                return "No session"
            switch (activeTab.sessionState) {
            case 0: return "Closed"
            case 1: return "Connecting\u2026"
            case 2: return "Open"
            case 3: return "Closing"
            case 4: return "Error"
            default: return ""
            }
        }
        geometryText: {
            if (!activeTab)
                return ""
            const sm = activeTab.terminal.surfaceModel
            return sm.columns + " \u00d7 " + sm.rows
        }
        cursorText: {
            if (!activeTab)
                return ""
            const sm = activeTab.terminal.surfaceModel
            return "Cursor " + (sm.cursorRow + 1) + ":" + (sm.cursorColumn + 1)
        }
        selectionText: ""
        noteText: ""
    }
}

