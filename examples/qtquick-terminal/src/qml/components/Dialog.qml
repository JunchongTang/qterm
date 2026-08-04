import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import QtQuick.Controls.Basic as C
import QtQuick.Effects
import QtQuickTerminal

// shadcn modal dialog: a rounded popover surface over a dimmed backdrop, with
// a title/description header, a body filled by the default content, and an
// optional footer bar separated by a divider.
C.Dialog {
    id: control

    property string description: ""
    property bool showCloseButton: true
    property alias footerContent: footerHost.data

    modal: true
    anchors.centerIn: parent
    implicitWidth: 360
    padding: Theme.space4

    // Clip the body so overflowing content stays inside the rounded surface.
    Component.onCompleted: contentItem.clip = true

    QQC.Overlay.modal: Rectangle {
        color: Theme.alpha("#000000", 0.6)
    }

    background: Rectangle {
        color: Theme.popover
        radius: Theme.radiusXl
        border.width: Theme.overlayRingWidth
        border.color: Theme.overlayRing
        layer.enabled: true
        layer.effect: MultiEffect {
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: Theme.shadowColor
            shadowBlur: Theme.shadowBlur
            shadowVerticalOffset: Theme.shadowOffset
        }
    }

    header: Item {
        visible: control.title !== "" || control.description !== "" || control.showCloseButton
        implicitHeight: visible
            ? Math.max(headerCol.implicitHeight, control.showCloseButton ? 24 : 0) + Theme.space4
            : 0

        ColumnLayout {
            id: headerCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.space4
            anchors.rightMargin: control.showCloseButton ? Theme.space4 + 24 : Theme.space4
            anchors.topMargin: Theme.space4
            spacing: Theme.space1

            Text {
                visible: control.title !== ""
                text: control.title
                color: Theme.foreground
                font.pixelSize: Theme.textSm
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Text {
                visible: control.description !== ""
                text: control.description
                color: Theme.mutedForeground
                font.pixelSize: Theme.textXs
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        IconButton {
            visible: control.showCloseButton
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            iconName: "x"
            size: IconButton.Small
            onClicked: control.close()
        }
    }

    // Footer bar: muted fill flush inside the dialog border, divided from the
    // body by a 1px line; content is stretched to full width so a trailing
    // RowLayout right-aligns its buttons.
    footer: Item {
        visible: footerHost.children.length > 0
        implicitHeight: footerHost.children.length > 0
                        ? footerHost.childrenRect.height + 2 * Theme.space4 : 0

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: Theme.overlayRingWidth
            anchors.rightMargin: Theme.overlayRingWidth
            anchors.bottomMargin: Theme.overlayRingWidth
            color: Theme.alpha(Theme.muted, 0.5)
            bottomLeftRadius: Theme.radiusXl - Theme.overlayRingWidth
            bottomRightRadius: Theme.radiusXl - Theme.overlayRingWidth
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.overlayRingWidth
            anchors.rightMargin: Theme.overlayRingWidth
            height: 1
            color: Theme.border
        }

        Item {
            id: footerHost
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.space4
            anchors.rightMargin: Theme.space4
            implicitHeight: childrenRect.height
        }
        Binding {
            target: footerHost.children.length > 0 ? footerHost.children[0] : null
            property: "width"
            value: footerHost.width
        }
    }

    enter: Transition {
        NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: Theme.durBase; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: Theme.durFast; easing.type: Easing.InCubic }
    }
}
