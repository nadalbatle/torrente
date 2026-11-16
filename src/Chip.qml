import QtQuick
import QtQuick.Controls

// A small label for a category or tag. The category is drawn in the accent.
Rectangle {
    id: chip
    property alias text: label.text
    property bool strong: false

    implicitWidth: label.implicitWidth + 14
    implicitHeight: 18
    radius: Math.min(4, theme.radius)
    color: strong ? Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.16)
                  : "#232327"
    border.width: 1
    border.color: strong ? Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.45)
                         : "#333338"

    Label {
        textFormat: Text.PlainText
        id: label
        anchors.centerIn: parent
        color: chip.strong ? theme.accent : "#b8b8be"
        font.pixelSize: 11
        font.weight: Font.Medium
    }
}
