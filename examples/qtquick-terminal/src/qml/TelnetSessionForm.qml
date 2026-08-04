pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuickTerminal

ColumnLayout {
    id: root

    property bool showErrors: false

    readonly property bool valid: hostField.text.trim().length > 0
    readonly property string errorMessage: qsTr("Host is required.")

    readonly property var sessionConfig: ({
        type: "telnet",
        label: qsTr("Telnet - %1").arg(hostField.text.trim()),
        host: hostField.text.trim(),
        port: parseInt(portField.text) || 23
    })

    spacing: Theme.space4

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1_5

        Label { text: qsTr("Host") }
        Input {
            id: hostField
            Layout.fillWidth: true
            // Public telnet playground, handy for exercising the renderer.
            text: "telehack.com"
            placeholderText: qsTr("hostname or IP")
            invalid: root.showErrors && !root.valid
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1_5

        Label { text: qsTr("Port") }
        Input {
            id: portField
            Layout.preferredWidth: 120
            text: "23"
            validator: IntValidator { bottom: 1; top: 65535 }
        }
    }

    // Telnet only needs two rows; this keeps them top-aligned inside the
    // fixed-height parameter area.
    Item { Layout.fillHeight: true }
}
