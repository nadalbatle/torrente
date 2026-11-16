import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// One download: its name, a thin progress line in the accent, and what it's
// doing now. Category and tags sit under the name as quiet chips.
Item {
    id: row
    required property int index
    required property string hash
    required property string name
    required property string state
    required property string group
    required property real progress
    required property real size
    required property real downloadSpeed
    required property real uploadSpeed
    required property real eta
    required property int seeds
    required property int peers
    required property string category
    required property var tags

    property bool selected: false
    signal clicked()
    signal activated()

    width: ListView.view ? ListView.view.width : 0
    height: 66

    readonly property bool finished: group === "done"
    readonly property bool problem: group === "attention"

    function stateLabel(state) {
        switch (state) {
        case "downloading": return qsTr("Downloading")
        case "forcedDL": return qsTr("Downloading (forced)")
        case "metaDL":
        case "forcedMetaDL": return qsTr("Fetching metadata")
        case "stalledDL": return qsTr("Waiting for peers")
        case "queuedDL":
        case "queuedUP": return qsTr("Queued")
        case "stoppedDL":
        case "pausedDL": return qsTr("Stopped")
        case "uploading":
        case "forcedUP":
        case "stalledUP": return qsTr("Seeding")
        case "stoppedUP":
        case "pausedUP": return qsTr("Done")
        case "checkingDL":
        case "checkingUP":
        case "checkingResumeData": return qsTr("Checking")
        case "moving": return qsTr("Moving")
        case "allocating": return qsTr("Allocating space")
        case "missingFiles": return qsTr("Files missing")
        case "error": return qsTr("Error")
        }
        return state
    }

    function etaLabel(seconds) {
        if (seconds <= 0 || seconds >= 8640000)
            return ""
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        if (h > 0)
            return qsTr("%1 h %2 min left").arg(h.toLocaleString(Qt.locale(), 'f', 0)).arg(m)
        if (m > 0)
            return qsTr("%1 min left").arg(m)
        return qsTr("less than a minute left")
    }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: Math.min(6, theme.radius)
        color: row.selected ? "#26262a" : hover.hovered ? "#1a1a1d" : "transparent"
        border.width: row.selected ? 1 : 0
        border.color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.55)
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: row.clicked()
        onDoubleTapped: row.activated()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 9
        anchors.bottomMargin: 9
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: row.name
                elide: Text.ElideMiddle
                color: row.group === "stopped" ? "#9a9aa0" : "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Label {
                textFormat: Text.PlainText
                text: row.finished ? Format.size(row.size, Qt.locale())
                                   : Format.percent(row.progress, Qt.locale()) + "  ·  " + Format.size(row.size, Qt.locale())
                color: "#8e8e93"
                font.pixelSize: 12
                font.features: { "tnum": 1 }
            }
        }

        // Progress, drawn only while there's progress to show.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 3
            radius: 1.5
            color: "#2a2a2e"
            visible: !row.finished
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, row.progress))
                height: parent.height
                radius: parent.radius
                color: row.problem ? "#e5534b" : row.group === "stopped" ? "#6e6e73" : theme.accent
                Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                textFormat: Text.PlainText
                text: row.stateLabel(row.state)
                color: row.problem ? "#e5534b" : row.group === "downloading" ? theme.accent : "#8e8e93"
                font.pixelSize: 12
            }
            Label {
                textFormat: Text.PlainText
                visible: row.downloadSpeed > 0 || row.uploadSpeed > 0
                text: (row.downloadSpeed > 0 ? "↓ " + Format.speed(row.downloadSpeed, Qt.locale()) : "")
                      + (row.downloadSpeed > 0 && row.uploadSpeed > 0 ? "   " : "")
                      + (row.uploadSpeed > 0 ? "↑ " + Format.speed(row.uploadSpeed, Qt.locale()) : "")
                color: "#c7c7cc"
                font.pixelSize: 12
                font.features: { "tnum": 1 }
            }
            Label {
                textFormat: Text.PlainText
                visible: text !== "" && !row.finished
                text: row.etaLabel(row.eta)
                color: "#8e8e93"
                font.pixelSize: 12
            }
            Label {
                textFormat: Text.PlainText
                visible: row.group === "downloading" || row.group === "waiting"
                text: qsTr("%1 seeds · %2 peers").arg(row.seeds).arg(row.peers)
                color: "#6e6e73"
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }

            Repeater {
                model: row.category !== "" ? [row.category] : []
                Chip { required property var modelData; text: modelData; strong: true }
            }
            Repeater {
                model: row.tags
                Chip { required property var modelData; text: modelData }
            }
        }
    }
}
