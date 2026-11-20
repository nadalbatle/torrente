import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut, in one place. ? opens and closes it.
Sheet {
    id: help
    title: qsTr("Keyboard shortcuts")
    panelWidth: 620

    readonly property var shortcuts: [
        ["1 / 2", qsTr("Downloads / Search")],
        ["Ctrl+V", qsTr("Add the magnet link on the clipboard")],
        ["A", qsTr("Add .torrent files")],
        ["/", qsTr("Filter downloads, or type a search")],
        ["↑ ↓  j k", qsTr("Move through the list")],
        ["[ ]", qsTr("Previous / next category or tag")],
        ["Space", qsTr("Stop or resume; download a search result")],
        ["Enter", qsTr("Open the folder; download a search result")],
        ["C", qsTr("Change the category")],
        ["T", qsTr("Edit the tags")],
        ["R", qsTr("Sort and tag again")],
        ["Delete", qsTr("Remove the download")],
        ["O", qsTr("Open the result's web page")],
        ["E", qsTr("Search engines")],
        [",", qsTr("Settings")],
        ["Esc", qsTr("Close, clear the filter, or stop the search")],
        ["?", qsTr("This help")],
        ["Q", qsTr("Quit (downloads keep going)")],
    ]

    GridLayout {
        width: parent.width
        columns: 2
        columnSpacing: 20
        rowSpacing: 7
        Repeater {
            model: help.shortcuts.length * 2
            Label {
                textFormat: Text.PlainText
                required property int index
                readonly property var item: help.shortcuts[Math.floor(index / 2)]
                Layout.fillWidth: index % 2 === 1
                text: index % 2 === 0 ? item[0] : item[1]
                color: index % 2 === 0 ? theme.accent : theme.foreground
                font.pixelSize: 13
                font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                wrapMode: Text.WordWrap
            }
        }
    }
}
