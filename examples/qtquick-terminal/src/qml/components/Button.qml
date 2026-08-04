import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as C
import QtQuickTerminal

// shadcn push button. The base type is imported aliased so the file's own type
// name `Button` stays available for enum access (Button.Default etc.).
C.Button {
    id: control

    enum Variant { Default, Secondary, Outline, Ghost, Destructive, Link }
    enum Size { Default, Sm, Lg }

    property int variant: Button.Default
    property int size: Button.Default
    property string iconName: ""
    property string trailingIconName: ""

    readonly property real _dim: size === Button.Sm ? 24 : size === Button.Lg ? 32 : 28
    readonly property int _iconSize: size === Button.Sm ? 12 : size === Button.Lg ? 16 : 14
    readonly property real _hpad: size === Button.Lg ? Theme.space2_5 : Theme.space2

    readonly property color _fg: {
        switch (variant) {
        case Button.Default: return Theme.primaryForeground
        case Button.Secondary: return Theme.secondaryForeground
        case Button.Destructive: return Theme.destructive
        case Button.Link: return Theme.primary
        default: return Theme.foreground // Outline / Ghost
        }
    }

    implicitHeight: _dim
    implicitWidth: Math.max(contentItem.implicitWidth + leftPadding + rightPadding, _dim)
    padding: 0
    leftPadding: _hpad - (iconName !== "" ? 2 : 0)
    rightPadding: _hpad - (trailingIconName !== "" ? 2 : 0)
    font.pixelSize: Theme.textXs
    font.weight: Font.Medium
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1.0 : 0.5

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        // active:translate-y-px -- content sinks 1px while pressed.
        transform: Translate { y: control.down ? 1 : 0 }

        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: Theme.space1

            Icon {
                visible: control.iconName !== ""
                name: control.iconName
                size: control._iconSize
                color: control._fg
            }
            Text {
                visible: control.text !== ""
                text: control.text
                font.pixelSize: control.font.pixelSize
                font.weight: control.font.weight
                font.underline: control.variant === Button.Link && control.hovered
                color: control._fg
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            Icon {
                visible: control.trailingIconName !== ""
                name: control.trailingIconName
                size: control._iconSize
                color: control._fg
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        border.width: control.variant === Button.Outline ? 1 : 0
        border.color: Theme.border
        transform: Translate { y: control.down ? 1 : 0 }
        color: {
            switch (control.variant) {
            case Button.Default:
                return control.hovered ? Theme.alpha(Theme.primary, 0.8) : Theme.primary
            case Button.Secondary:
                return control.hovered ? Qt.darker(Theme.secondary, 1.05) : Theme.secondary
            case Button.Destructive:
                return Theme.alpha(Theme.destructive, control.hovered ? 0.2 : 0.1)
            case Button.Outline:
                return Theme.alpha(Theme.input, Theme.input.a * (control.hovered ? 0.5 : 0))
            case Button.Ghost:
                return control.hovered ? Theme.muted : Theme.alpha(Theme.muted, 0)
            default:
                return "transparent" // Link
            }
        }
        Behavior on color { ColorAnimation { duration: Theme.durBase } }

        FocusRing { active: control.visualFocus; targetRadius: Theme.radiusMd }
    }
}
