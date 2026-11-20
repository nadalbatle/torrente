import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

// One search result: what it is, how big, and how well seeded.
Item {
    id: row
    required property int index
    required property string name
    required property real size
    required property int seeds
    required property int peers
    required property string engine

    property bool selected: false
    signal clicked()
    signal activated()

    width: ListView.view ? ListView.view.width : 0
    height: 52

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: Math.min(6, theme.radius)
        color: row.selected ? theme.selected : hover.hovered ? theme.hover : "transparent"
        border.width: row.selected ? 1 : 0
        border.color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.55)
    }
    HoverHandler { id: hover }
    TapHandler {
        onTapped: row.clicked()
        onDoubleTapped: row.activated()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: row.name
                elide: Text.ElideRight
                color: theme.foreground
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            Label {
                textFormat: Text.PlainText
                text: row.engine
                color: theme.faint
                font.pixelSize: 11
            }
        }
        Label {
            textFormat: Text.PlainText
            Layout.preferredWidth: 80
            horizontalAlignment: Text.AlignRight
            text: Format.size(row.size, Qt.locale())
            color: theme.foreground
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
        Label {
            textFormat: Text.PlainText
            Layout.preferredWidth: 70
            horizontalAlignment: Text.AlignRight
            text: "▲ " + row.seeds
            color: row.seeds > 10 ? theme.accent : row.seeds > 0 ? theme.foreground : theme.faint
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
        Label {
            textFormat: Text.PlainText
            Layout.preferredWidth: 60
            horizontalAlignment: Text.AlignRight
            text: "▼ " + row.peers
            color: theme.dim
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
    }
}
