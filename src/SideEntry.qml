import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A filter in the sidebar: a name and how many downloads it holds.
Item {
    id: entry
    property string label
    property int count: 0
    property bool current: false
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: 30

    Rectangle {
        anchors.fill: parent
        radius: Math.min(4, theme.radius)
        color: entry.current ? theme.selected : hover.hovered ? theme.hover : "transparent"
    }
    Rectangle {
        visible: entry.current
        width: 3
        height: 16
        radius: 1.5
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        color: theme.accent
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: entry.clicked() }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: entry.label
            elide: Text.ElideRight
            color: entry.current ? theme.foreground : theme.foreground
            font.pixelSize: 13
            font.weight: entry.current ? Font.DemiBold : Font.Normal
        }
        Label {
            textFormat: Text.PlainText
            text: entry.count
            color: theme.faint
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
    }
}
