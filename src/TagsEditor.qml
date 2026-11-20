import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Edits a download's tags as one comma-separated line.
Sheet {
    id: editor
    signal accepted(string tags)

    function open(tags) {
        field.text = tags.join(", ")
        visible = true
        field.forceActiveFocus()
        field.selectAll()
    }

    ColumnLayout {
        width: parent.width
        spacing: 12
        TextField {
            id: field
            Layout.fillWidth: true
            Material.roundedScale: theme.radius
            placeholderText: qsTr("Tags, separated by commas")
            onAccepted: editor.accepted(text)
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: qsTr("Enter saves. Leave it empty to remove every tag.")
            color: theme.dim
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
