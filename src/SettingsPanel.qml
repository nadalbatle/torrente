import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Where downloads go and how they're sorted.
Sheet {
    id: settings
    title: qsTr("Settings")
    panelWidth: 520

    function open() {
        visible = true
        changeButton.forceActiveFocus()
    }

    ColumnLayout {
        width: parent.width
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Label {
                textFormat: Text.PlainText
                text: qsTr("Download folder")
                color: "#8e8e93"
                font.pixelSize: 12
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: backend.downloadFolder
                    elide: Text.ElideMiddle
                    color: "white"
                    font.pixelSize: 13
                }
                DialogButton {
                    id: changeButton
                    text: qsTr("Change…")
                    mnemonic: qsTr("C")
                    onClicked: backend.chooseDownloadFolder()
                    Keys.onDownPressed: foldersSwitch.forceActiveFocus()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#2c2c30" }

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Sort into folders by type")
                    color: "white"
                    font.pixelSize: 13
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Movies, Series, Music, Books, Software and Games each get a folder. Downloads wait in a hidden folder until they finish.")
                    color: "#8e8e93"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
            Switch {
                id: foldersSwitch
                checked: backend.autoFolders
                onToggled: backend.autoFolders = checked
                Keys.onUpPressed: changeButton.forceActiveFocus()
                Keys.onDownPressed: tagsSwitch.forceActiveFocus()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Tag automatically")
                    color: "white"
                    font.pixelSize: 13
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Quality, format and language from the release name: 4K, HEVC, FLAC, ES…")
                    color: "#8e8e93"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
            Switch {
                id: tagsSwitch
                checked: backend.autoTags
                onToggled: backend.autoTags = checked
                Keys.onUpPressed: foldersSwitch.forceActiveFocus()
            }
        }

        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            visible: backend.autoFolders
            text: qsTr("Changing the folder moves the downloads already sorted into it.")
            color: "#8e8e93"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
