pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTerm

// SerialSessionForm — configuration form for a serial port session.
Item {
    id: root

    implicitHeight: column.implicitHeight

    readonly property var sessionConfig: ({
        type: "serial",
        label: "Serial \u2014 " + (portCombo.currentText || "?"),
        portName: portCombo.currentText,
        baudRate: parseInt(baudCombo.editText) || 9600,
        dataBits: parseInt(dataBitsCombo.currentText) || 8,
        parity: parityCombo.currentValue,
        stopBits: parseInt(stopBitsCombo.currentText) || 1,
        flowControl: flowCombo.currentValue
    })

    function isValid() {
        return portCombo.currentText.length > 0
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        // Serial port selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Port")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            ComboBox {
                id: portCombo
                Layout.fillWidth: true
                editable: true
                font.pixelSize: 13
                model: {
                    const infos = QTermSerialPortScanner.availablePorts()
                    const names = []
                    for (let i = 0; i < infos.length; i++)
                        names.push(infos[i].portName)
                    return names
                }
                contentItem: TextField {
                    text: portCombo.editText
                    font: portCombo.font
                    color: "#c9d1d9"
                    background: null
                    leftPadding: 8
                }
                background: Rectangle {
                    color: "#21262d"
                    border.color: portCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
            }

            Button {
                text: qsTr("Refresh")
                font.pixelSize: 12
                padding: 6
                onClicked: {
                    const infos = QTermSerialPortScanner.availablePorts()
                    const names = []
                    for (let i = 0; i < infos.length; i++)
                        names.push(infos[i].portName)
                    portCombo.model = names
                }
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
        }

        // Baud rate
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Baud rate")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            ComboBox {
                id: baudCombo
                Layout.fillWidth: true
                editable: true
                font.pixelSize: 13
                model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
                currentIndex: 4  // 115200
                contentItem: TextField {
                    text: baudCombo.editText
                    font: baudCombo.font
                    color: "#c9d1d9"
                    background: null
                    leftPadding: 8
                }
                background: Rectangle {
                    color: "#21262d"
                    border.color: baudCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
            }
        }

        // Data bits / Parity / Stop bits (single row)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Data / Parity / Stop")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            ComboBox {
                id: dataBitsCombo
                Layout.preferredWidth: 70
                font.pixelSize: 13
                model: ["5", "6", "7", "8"]
                currentIndex: 3  // 8
                background: Rectangle {
                    color: "#21262d"
                    border.color: dataBitsCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
                contentItem: Text {
                    text: dataBitsCombo.displayText
                    color: "#c9d1d9"
                    font: dataBitsCombo.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ComboBox {
                id: parityCombo
                Layout.preferredWidth: 90
                font.pixelSize: 13
                model: ListModel {
                    ListElement { text: qsTr("None");  value: "N" }
                    ListElement { text: qsTr("Even");  value: "E" }
                    ListElement { text: qsTr("Odd");   value: "O" }
                    ListElement { text: qsTr("Mark");  value: "M" }
                    ListElement { text: qsTr("Space"); value: "S" }
                }
                textRole: "text"
                valueRole: "value"
                background: Rectangle {
                    color: "#21262d"
                    border.color: parityCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
                contentItem: Text {
                    text: parityCombo.displayText
                    color: "#c9d1d9"
                    font: parityCombo.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ComboBox {
                id: stopBitsCombo
                Layout.preferredWidth: 70
                font.pixelSize: 13
                model: ["1", "2"]
                background: Rectangle {
                    color: "#21262d"
                    border.color: stopBitsCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
                contentItem: Text {
                    text: stopBitsCombo.displayText
                    color: "#c9d1d9"
                    font: stopBitsCombo.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Flow control
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Flow control")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            ComboBox {
                id: flowCombo
                Layout.fillWidth: true
                font.pixelSize: 13
                model: ListModel {
                    ListElement { text: qsTr("None");     value: "none" }
                    ListElement { text: qsTr("Hardware"); value: "hardware" }
                    ListElement { text: qsTr("Software"); value: "software" }
                }
                textRole: "text"
                valueRole: "value"
                background: Rectangle {
                    color: "#21262d"
                    border.color: flowCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
                contentItem: Text {
                    text: flowCombo.displayText
                    color: "#c9d1d9"
                    font: flowCombo.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
