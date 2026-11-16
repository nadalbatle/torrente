import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// qBittorrent's own search plugins: install the ones you choose, and turn
// installed ones on or off. Nothing is installed until the user asks.
Sheet {
    id: panel
    title: qsTr("Search engines")
    panelWidth: 520

    property var selected: ({})

    function installed(name) {
        for (var i = 0; i < backend.plugins.length; i++)
            if (backend.plugins[i].name === name)
                return backend.plugins[i]
        return null
    }

    function open() {
        selected = {}
        visible = true
        list.forceActiveFocus()
    }

    function toggle(name) {
        var plugin = installed(name)
        if (plugin) {
            backend.setPluginEnabled(name, !plugin.enabled)
        } else {
            var copy = Object.assign({}, selected)
            copy[name] = !copy[name]
            selected = copy
        }
    }

    function install() {
        var names = []
        for (var name in selected)
            if (selected[name])
                names.push(name)
        if (names.length)
            backend.installPlugins(names)
        selected = {}
    }

    ColumnLayout {
        width: parent.width
        spacing: 12

        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: qsTr("Searches go through qBittorrent's search plugins. Pick the sites to search; Space marks one, I installs what's marked.")
            color: "#8e8e93"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            interactive: false
            model: backend.officialPlugins
            keyNavigationWraps: true
            Keys.onSpacePressed: panel.toggle(backend.officialPlugins[currentIndex].name)
            Keys.onPressed: (event) => {
                if (event.text === "j") { incrementCurrentIndex(); event.accepted = true }
                else if (event.text === "k") { decrementCurrentIndex(); event.accepted = true }
                else if (event.text === "i" || event.text === "I") { panel.install(); event.accepted = true }
            }

            delegate: Item {
                id: entry
                required property var modelData
                required property int index
                readonly property var plugin: panel.installed(modelData.name)
                width: ListView.view.width
                height: 40

                Rectangle {
                    anchors.fill: parent
                    radius: Math.min(4, theme.radius)
                    color: entry.index === list.currentIndex ? "#2c2c30" : "transparent"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 4
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: entry.plugin ? entry.plugin.fullName : entry.modelData.name
                        color: "white"
                        font.pixelSize: 13
                    }
                    Label {
                        textFormat: Text.PlainText
                        text: entry.plugin ? (entry.plugin.enabled ? qsTr("On") : qsTr("Off"))
                                           : panel.selected[entry.modelData.name] ? qsTr("Will install") : qsTr("Not installed")
                        color: entry.plugin && entry.plugin.enabled ? theme.accent : "#8e8e93"
                        font.pixelSize: 12
                    }
                    Switch {
                        focusPolicy: Qt.NoFocus
                        checked: entry.plugin ? entry.plugin.enabled : panel.selected[entry.modelData.name] === true
                        onToggled: panel.toggle(entry.modelData.name)
                    }
                }
                TapHandler { onTapped: list.currentIndex = entry.index }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            BusyIndicator {
                visible: backend.installingPlugins
                running: visible
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }
            Label {
                textFormat: Text.PlainText
                visible: backend.installingPlugins
                text: qsTr("Installing…")
                color: "#8e8e93"
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            DialogButton {
                text: qsTr("Install marked")
                mnemonic: qsTr("I")
                primary: true
                enabled: {
                    for (var name in panel.selected)
                        if (panel.selected[name])
                            return true
                    return false
                }
                onClicked: panel.install()
            }
        }
    }
}
