pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QTerm
import QtQuickTerminal

ColumnLayout {
    id: root

    property bool showErrors: false

    readonly property bool valid: portSelect.currentText.length > 0
    readonly property string errorMessage: qsTr("Select a serial port.")

    readonly property var sessionConfig: ({
        type: "serial",
        label: qsTr("Serial - %1").arg(portSelect.currentText),
        portName: portSelect.currentText,
        baudRate: parseInt(baudSelect.currentText) || 9600,
        dataBits: parseInt(dataBitsSelect.currentText) || 8,
        parity: paritySelect.currentValue,
        stopBits: parseInt(stopBitsSelect.currentText) || 1,
        flowControl: flowSelect.currentValue
    })

    function refreshPorts() {
        const infos = QTermSerialPortScanner.availablePorts()
        portSelect.model = infos.map(i => i.portName)
        if (portSelect.currentIndex < 0 && infos.length > 0)
            portSelect.currentIndex = 0
    }

    spacing: Theme.space4

    Component.onCompleted: refreshPorts()

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1_5

        Label { text: qsTr("Port") }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Select {
                id: portSelect
                Layout.fillWidth: true
                placeholder: qsTr("No ports found")
                invalid: root.showErrors && !root.valid
            }
            IconButton {
                iconName: "refresh-cw"
                variant: IconButton.Outline
                onClicked: root.refreshPorts()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space3

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1_5

            Label { text: qsTr("Baud rate") }
            Select {
                id: baudSelect
                Layout.fillWidth: true
                model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
                currentIndex: 4
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1_5

            Label { text: qsTr("Flow control") }
            Select {
                id: flowSelect
                Layout.fillWidth: true
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: qsTr("None"), value: "none" },
                    { text: qsTr("Hardware"), value: "hardware" },
                    { text: qsTr("Software"), value: "software" }
                ]
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space3

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1_5

            Label { text: qsTr("Data bits") }
            Select {
                id: dataBitsSelect
                Layout.fillWidth: true
                model: ["5", "6", "7", "8"]
                currentIndex: 3
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1_5

            Label { text: qsTr("Parity") }
            Select {
                id: paritySelect
                Layout.fillWidth: true
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: qsTr("None"), value: "N" },
                    { text: qsTr("Even"), value: "E" },
                    { text: qsTr("Odd"), value: "O" },
                    { text: qsTr("Mark"), value: "M" },
                    { text: qsTr("Space"), value: "S" }
                ]
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1_5

            Label { text: qsTr("Stop bits") }
            Select {
                id: stopBitsSelect
                Layout.fillWidth: true
                model: ["1", "2"]
            }
        }
    }

    Item { Layout.fillHeight: true }
}
