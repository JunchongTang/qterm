pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// NewSessionDialog — lets the user pick a session type and fill its parameters,
// then emits sessionRequested(config) with the JS config object.
Popup {
    id: root

    signal sessionRequested(var config)

    property string sessionTypeId: "pty"

    anchors.centerIn: parent
    width: 520
    implicitHeight: contentColumn.implicitHeight + 48
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 24

    background: Rectangle {
        color: "#161b22"
        border.color: "#30363d"
        radius: 12
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 20

        // Title
        Text {
            text: qsTr("New Session")
            color: "#c9d1d9"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        // Type selector — independent buttons with spacing
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { typeId: "pty",    label: qsTr("Shell") },
                    { typeId: "serial", label: qsTr("Serial Port") },
                    { typeId: "telnet", label: qsTr("Telnet") }
                ]

                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    color: root.sessionTypeId === modelData.typeId
                        ? "#388bfd"
                        : (typeBtnMouse.containsMouse ? "#2d333b" : "#21262d")
                    border.color: root.sessionTypeId === modelData.typeId ? "#388bfd" : "#30363d"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: parent.modelData.label
                        color: "#c9d1d9"
                        font.pixelSize: 13
                    }

                    MouseArea {
                        id: typeBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.sessionTypeId = parent.modelData.typeId
                    }
                }
            }
        }

        // Form area — swaps content based on selected type
        Loader {
            id: formLoader
            Layout.fillWidth: true
            sourceComponent: {
                if (root.sessionTypeId === "serial")
                    return serialFormComponent
                if (root.sessionTypeId === "telnet")
                    return telnetFormComponent
                return ptyFormComponent
            }
        }

        // Validation message
        Text {
            id: validationMsg
            Layout.fillWidth: true
            visible: text.length > 0
            color: "#f85149"
            font.pixelSize: 12
            text: ""
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Cancel")
                font.pixelSize: 13
                padding: 10
                onClicked: root.close()
                background: Rectangle {
                    color: parent.pressed ? "#2d333b" : "#21262d"
                    border.color: "#30363d"
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#c9d1d9"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: qsTr("Connect")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                padding: 10
                onClicked: {
                    const form = formLoader.item
                    if (!form || !form.isValid()) {
                        validationMsg.text = qsTr("Please fill in the required fields.")
                        return
                    }
                    validationMsg.text = ""
                    root.sessionRequested(form.sessionConfig)
                    root.close()
                }
                background: Rectangle {
                    color: parent.pressed ? "#1f6feb" : "#388bfd"
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // Form components
    ButtonGroup { id: typeGroup }

    Component { id: ptyFormComponent;    PtySessionForm { } }
    Component { id: serialFormComponent; SerialSessionForm { } }
    Component { id: telnetFormComponent; TelnetSessionForm { } }
}
