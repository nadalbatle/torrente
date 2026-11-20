import QtQuick
import QtQuick.Controls

// A panel over the dimmed window. A click outside dismisses it; Escape is
// handled by the window, which closes whatever sheet is open.
Rectangle {
    id: sheet
    anchors.fill: parent
    color: theme.overlay
    visible: false
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property string title: ""
    property int panelWidth: 460
    default property alias content: body.data
    signal dismissed()

    MouseArea {
        anchors.fill: parent
        onClicked: sheet.dismissed()
    }

    DialogPanel {
        anchors.centerIn: parent
        width: Math.min(sheet.panelWidth, sheet.width - 48)
        height: Math.min(column.implicitHeight + 48, sheet.height - 48)
        MouseArea { anchors.fill: parent }

        // The mouse's way out, besides a click outside or Escape.
        AbstractButton {
            id: closeButton
            objectName: "sheetClose"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 12
            width: 28
            height: 28
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            onClicked: sheet.dismissed()
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Close")
            contentItem: Label {
                text: "\u00d7"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 20
                color: closeButton.hovered ? theme.foreground : theme.dim
            }
            background: Rectangle {
                radius: Math.min(4, theme.radius)
                color: closeButton.down ? theme.pressed : closeButton.hovered ? theme.pressed : "transparent"
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        Column {
            id: column
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            Label {
                textFormat: Text.PlainText
                text: sheet.title
                color: theme.foreground
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Item {
                id: body
                width: parent.width
                implicitHeight: childrenRect.height
                height: implicitHeight
            }
        }
    }
}
