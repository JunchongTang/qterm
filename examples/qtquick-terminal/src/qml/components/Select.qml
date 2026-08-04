import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Effects
import QtQuickTerminal

// shadcn dropdown select: trigger with value/placeholder plus chevron, popover
// list with the current row marked by a trailing check.
C.ComboBox {
    id: control

    property string placeholder: ""
    property bool invalid: false

    readonly property int _itemHeight: 28

    implicitHeight: 28
    leftPadding: Theme.space2
    rightPadding: Theme.space2 + 14 + Theme.space1_5
    font.pixelSize: Theme.textXs
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1.0 : 0.5

    contentItem: Text {
        readonly property bool _empty: control.currentIndex < 0 || control.displayText === ""
        text: _empty && control.placeholder !== "" ? control.placeholder : control.displayText
        font: control.font
        color: _empty ? Theme.mutedForeground : Theme.foreground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: control.width - width - Theme.space2
        y: (control.height - height) / 2
        name: "chevron-down"
        size: 14
        color: Theme.mutedForeground
    }

    background: Rectangle {
        id: bg
        radius: Theme.radiusMd
        color: Theme.alpha(Theme.input, Theme.input.a * (Theme.dark ? (control.hovered ? 0.5 : 0.3) : 0.2))
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
        border.width: 1
        border.color: control.invalid
                      ? (Theme.dark ? Theme.alpha(Theme.destructive, 0.5) : Theme.destructive)
                      : control.visualFocus ? Theme.ring : Theme.input
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.ringWidth
            radius: bg.radius + Theme.ringWidth
            color: "transparent"
            border.width: Theme.ringWidth
            border.color: Theme.alpha(Theme.destructive, Theme.dark ? 0.4 : 0.2)
            visible: control.invalid
            z: -1
        }

        FocusRing {
            active: control.visualFocus && !control.invalid
            targetRadius: bg.radius
        }
    }

    delegate: C.ItemDelegate {
        id: item
        required property int index
        required property var model

        readonly property bool _selected: control.currentIndex === index
        readonly property bool _active: hovered || highlighted

        width: ListView.view ? ListView.view.width : control.width
        height: control._itemHeight
        padding: 0
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: Item {
            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.space2
                anchors.rightMargin: Theme.space2 + 14
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    const v = item.model[control.textRole]
                    if (v !== undefined)
                        return v
                    const md = item.model.modelData
                    return typeof md === "string" ? md : ""
                }
                font.pixelSize: Theme.textXs
                color: item._active ? Theme.accentForeground : Theme.foreground
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            Icon {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                name: "check"
                size: 14
                color: item._active ? Theme.accentForeground : Theme.foreground
                visible: item._selected
            }
        }

        background: Rectangle {
            visible: item._active
            radius: Theme.radiusMd
            color: Theme.accent
        }
    }

    popup: C.Popup {
        y: control.height + Theme.space1
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2 * padding, 300)
        padding: Theme.space1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            C.ScrollIndicator.vertical: C.ScrollIndicator {}
        }

        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.popover
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
    }
}
