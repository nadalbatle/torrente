import QtQuick
import QtQuick.Controls

// A panel over the dimmed window. A click outside dismisses it; Escape is
// handled by the window, which closes whatever sheet is open.
Rectangle {
    id: sheet
    anchors.fill: parent
    color: "#cc000000"
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

        Column {
            id: column
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            Label {
                textFormat: Text.PlainText
                text: sheet.title
                color: "white"
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
