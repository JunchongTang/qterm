import QtQuick
import QtQuick.Controls.Basic as C
import QtQuickTerminal

// shadcn single-line text field: input/20 tinted fill, 1px input border,
// ring border + focus ring on focus, destructive treatment when invalid.
C.TextField {
    id: control

    property bool invalid: false

    implicitHeight: 28
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    topPadding: 0
    bottomPadding: 0
    font.pixelSize: Theme.textXs
    color: Theme.foreground
    placeholderTextColor: Theme.mutedForeground
    selectionColor: Theme.alpha(Theme.primary, 0.35)
    selectedTextColor: Theme.foreground
    verticalAlignment: TextInput.AlignVCenter
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1.0 : 0.5

    background: Rectangle {
        id: bg
        radius: Theme.radiusMd
        color: Theme.alpha(Theme.input, Theme.input.a * (Theme.dark ? 0.3 : 0.2))
        border.width: 1
        border.color: control.invalid
                      ? (Theme.dark ? Theme.alpha(Theme.destructive, 0.5) : Theme.destructive)
                      : control.activeFocus ? Theme.ring : Theme.input
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

        // aria-invalid ring, shown whenever invalid regardless of focus.
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
            active: control.activeFocus && !control.invalid
            targetRadius: bg.radius
        }
    }
}
