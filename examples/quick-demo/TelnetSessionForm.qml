pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// TelnetSessionForm — configuration form for a Telnet session.
Item {
    id: root

    implicitHeight: column.implicitHeight

    readonly property var sessionConfig: ({
        type: "telnet",
        label: "Telnet \u2014 " + (hostField.text || "?"),
        host: hostField.text,
        port: parseInt(portField.text) || 23
    })

    function isValid() {
        return hostField.text.trim().length > 0
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        // Host
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Host")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            TextField {
                id: hostField
                Layout.fillWidth: true
                placeholderText: qsTr("hostname or IP address")
                color: "#c9d1d9"
                font.pixelSize: 13
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                background: Rectangle {
                    color: "#21262d"
                    border.color: hostField.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }

                Keys.onReturnPressed: portField.forceActiveFocus()
            }
        }

        // Port
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Port")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            TextField {
                id: portField
                Layout.preferredWidth: 100
                text: "23"
                color: "#c9d1d9"
                font.pixelSize: 13
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
                background: Rectangle {
                    color: "#21262d"
                    border.color: portField.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
            }
        }
    }
}
