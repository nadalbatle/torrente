import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Says why there are no downloads to show when the engine can't be reached,
// and offers the one fix that's in reach.
Rectangle {
    id: notice
    color: "#0e0e10"

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(460, notice.width - 64)
        spacing: 12

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: backend.engineState === "connecting" || backend.startingEngine
            running: visible
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "white"
            font.pixelSize: 18
            font.weight: Font.DemiBold
            text: backend.engineState === "forbidden" ? qsTr("The engine asks for a password")
                : backend.engineState === "offline" ? qsTr("The engine isn't running")
                : qsTr("Connecting to the engine…")
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#a1a1a6"
            font.pixelSize: 13
            visible: text !== ""
            text: backend.engineState === "forbidden"
                ? qsTr("Torrente talks to qBittorrent's engine on this computer. In qBittorrent's Web UI options, turn on \"Bypass authentication for clients on localhost\".")
                : backend.engineState === "offline"
                  ? qsTr("Torrente downloads through qbittorrent-nox, which runs in the background so downloads continue when the window is closed.")
                  : ""
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#e5534b"
            font.pixelSize: 12
            visible: backend.engineState === "offline" && backend.engineError !== ""
            text: backend.engineError
        }
        DialogButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            visible: backend.engineState === "offline"
            enabled: !backend.startingEngine
            primary: true
            text: qsTr("Start the engine")
            mnemonic: qsTr("S")
            onClicked: backend.startEngine()
        }
    }
}
