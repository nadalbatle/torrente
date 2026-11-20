import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

ApplicationWindow {
    id: win
    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "Torrente"
    color: theme.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.background: theme.background
    Material.roundedScale: theme.radius

    // 0: downloads, 1: search.
    property int page: 0
    property string selectedHash: ""
    property int resultIndex: 0

    readonly property bool engineReady: backend.engineState === "ready"
    readonly property bool overlayOpen: help.visible || settings.visible || engines.visible
        || categoryPicker.visible || tagsEditor.visible || removeDialog.visible
    // Letter shortcuts stand aside while a text field has the keyboard.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition")
    readonly property bool keysFree: !overlayOpen && !typing

    readonly property int selectedRow: backend.torrents.count, backend.torrents.indexOf(selectedHash)

    function selectRow(row) {
        var count = backend.torrents.count
        if (count === 0) {
            selectedHash = ""
            return
        }
        row = Math.max(0, Math.min(count - 1, row))
        selectedHash = backend.torrents.hashAt(row)
        torrentList.positionViewAtIndex(row, ListView.Contain)
    }

    function move(step) {
        if (page === 0) {
            selectRow(selectedRow < 0 ? 0 : selectedRow + step)
        } else {
            resultIndex = Math.max(0, Math.min(backend.results.count - 1, resultIndex + step))
            resultList.positionViewAtIndex(resultIndex, ListView.Contain)
        }
    }

    function selectedTorrent() {
        if (selectedRow < 0)
            return null
        var item = torrentList.itemAtIndex(selectedRow)
        return item
    }

    // The sidebar's filters in order, for [ and ].
    function filters() {
        var list = [{ category: "", tag: "" }]
        for (var i = 0; i < backend.categories.length; i++)
            list.push({ category: backend.categories[i], tag: "" })
        if (backend.torrents.categoryCounts["__none__"])
            list.push({ category: "__none__", tag: "" })
        var tags = Object.keys(backend.torrents.tagCounts).sort()
        for (var j = 0; j < tags.length; j++)
            list.push({ category: "", tag: tags[j] })
        return list
    }

    function cycleFilter(step) {
        var list = filters()
        var at = 0
        for (var i = 0; i < list.length; i++)
            if (list[i].category === backend.torrents.categoryFilter && list[i].tag === backend.torrents.tagFilter)
                at = i
        var next = list[(at + step + list.length) % list.length]
        backend.torrents.categoryFilter = next.category
        backend.torrents.tagFilter = next.tag
    }

    function showPage(index) {
        page = index
        if (index === 1 && searchField.text === "")
            searchField.forceActiveFocus()
        else
            pageStack.forceActiveFocus()
    }

    function closeOverlay() {
        help.visible = false
        settings.visible = false
        engines.visible = false
        categoryPicker.visible = false
        tagsEditor.visible = false
        removeDialog.visible = false
        pageStack.forceActiveFocus()
    }

    function toast(message, problem) {
        toastLabel.text = message
        toastBox.problem = problem
        toastBox.opacity = 1
        toastTimer.restart()
    }

    Connections {
        target: backend
        function onNotify(message, problem) { win.toast(message, problem) }
    }

    // --- Shortcuts ---------------------------------------------------------

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing || help.visible
        onActivated: help.visible ? win.closeOverlay() : (win.closeOverlay(), help.visible = true) }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: win.keysFree; autoRepeat: false
        onActivated: Qt.quit() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut
        onActivated: {
            if (win.overlayOpen) win.closeOverlay()
            else if (win.page === 1 && backend.searching) backend.stopSearch()
            else if (win.typing) pageStack.forceActiveFocus()
            else if (win.page === 0 && (backend.torrents.textFilter !== "" || backend.torrents.categoryFilter !== "" || backend.torrents.tagFilter !== "")) {
                filterField.text = ""
                backend.torrents.categoryFilter = ""
                backend.torrents.tagFilter = ""
            }
        }
    }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.showPage(0) }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.showPage(1) }
    Shortcut { sequences: ["Ctrl+Tab"]; context: Qt.ApplicationShortcut; enabled: !win.overlayOpen
        onActivated: win.showPage(1 - win.page) }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: win.keysFree
        onActivated: win.page === 0 ? filterField.forceActiveFocus() : searchField.forceActiveFocus() }
    Shortcut { sequences: [StandardKey.Paste]; context: Qt.ApplicationShortcut; enabled: !win.overlayOpen && !win.typing
        onActivated: if (!backend.pasteFromClipboard()) win.toast(qsTr("There's no magnet link on the clipboard."), true) }
    Shortcut { sequences: ["A", "Ctrl+O"]; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.openTorrentFiles() }
    Shortcut { sequences: ["Down", "J"]; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.move(1) }
    Shortcut { sequences: ["Up", "K"]; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.move(-1) }
    Shortcut { sequence: "Home"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.move(-100000) }
    Shortcut { sequence: "End"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.move(100000) }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 0; onActivated: win.cycleFilter(1) }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 0; onActivated: win.cycleFilter(-1) }
    Shortcut { sequence: ","; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: settings.open() }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: engines.open() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keysFree
        onActivated: {
            if (win.page === 0 && win.selectedHash !== "") backend.toggle(win.selectedHash)
            else if (win.page === 1 && backend.results.count > 0) win.downloadResult(win.resultIndex)
        }
    }
    Shortcut { sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; enabled: win.keysFree
        onActivated: {
            if (win.page === 0 && win.selectedHash !== "") backend.openFolder(win.selectedHash)
            else if (win.page === 1 && backend.results.count > 0) win.downloadResult(win.resultIndex)
        }
    }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 1 && backend.results.count > 0
        onActivated: backend.openResultPage(win.resultIndex) }
    Shortcut { sequences: ["Delete", "X"]; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 0 && win.selectedHash !== ""
        onActivated: removeDialog.visible = true }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 0 && win.selectedHash !== ""
        onActivated: win.chooseCategory() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 0 && win.selectedHash !== ""
        onActivated: win.editTags() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.keysFree && win.page === 0 && win.selectedHash !== ""
        onActivated: { backend.reclassify(win.selectedHash); win.toast(qsTr("Sorting it again…"), false) } }

    function chooseCategory() {
        var options = [{ label: qsTr("No category"), value: "" }]
        for (var i = 0; i < backend.categories.length; i++)
            options.push({ label: backend.categories[i], value: backend.categories[i] })
        categoryPicker.options = options
        var item = win.selectedTorrent()
        categoryPicker.open(item ? item.category : "")
    }

    function editTags() {
        var item = win.selectedTorrent()
        tagsEditor.open(item ? item.tags : [])
    }

    function downloadResult(index) {
        backend.download(index)
    }

    // --- Layout ------------------------------------------------------------

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top bar: the two pages, and what the engine is doing overall.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: theme.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 4

                    Repeater {
                        model: [qsTr("Downloads"), qsTr("Search")]
                        Item {
                            id: tab
                            required property string modelData
                            required property int index
                            Layout.preferredHeight: 52
                            Layout.preferredWidth: tabLabel.implicitWidth + 28
                            Label {
                                textFormat: Text.PlainText
                                id: tabLabel
                                anchors.centerIn: parent
                                text: tab.modelData
                                color: win.page === tab.index ? theme.foreground : theme.dim
                                font.pixelSize: 14
                                font.weight: win.page === tab.index ? Font.DemiBold : Font.Normal
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: tabLabel.implicitWidth
                                height: 2
                                color: theme.accent
                                visible: win.page === tab.index
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: win.showPage(tab.index) }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        textFormat: Text.PlainText
                        visible: win.engineReady
                        text: "↓ " + Format.speed(backend.downloadSpeed, Qt.locale())
                        color: backend.downloadSpeed > 0 ? theme.accent : theme.dim
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                    }
                    Label {
                        textFormat: Text.PlainText
                        visible: win.engineReady
                        Layout.leftMargin: 14
                        text: "↑ " + Format.speed(backend.uploadSpeed, Qt.locale())
                        color: theme.dim
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                    }
                    Label {
                        textFormat: Text.PlainText
                        visible: win.engineReady && backend.freeSpace >= 0
                        Layout.leftMargin: 14
                        text: qsTr("%1 free").arg(Format.size(backend.freeSpace, Qt.locale()))
                        color: theme.faint
                        font.pixelSize: 12
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.leftMargin: 18
                        text: qsTr("? shortcuts")
                        color: theme.faint
                        font.pixelSize: 12
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: theme.hover
                }
            }

            StackLayout {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.page
                focus: true

                // --- Downloads ---
                RowLayout {
                    spacing: 0

                    // Sidebar: categories and tags, with how many each holds.
                    Rectangle {
                        Layout.preferredWidth: 210
                        Layout.fillHeight: true
                        color: theme.surface

                        Flickable {
                            anchors.fill: parent
                            anchors.margins: 10
                            contentHeight: sidebar.implicitHeight
                            clip: true

                            ColumnLayout {
                                id: sidebar
                                width: parent.width
                                spacing: 2

                                SideEntry {
                                    label: qsTr("All downloads")
                                    count: backend.torrents.total
                                    current: backend.torrents.categoryFilter === "" && backend.torrents.tagFilter === ""
                                    onClicked: { backend.torrents.categoryFilter = ""; backend.torrents.tagFilter = "" }
                                }

                                Label {
                                    textFormat: Text.PlainText
                                    Layout.topMargin: 14
                                    Layout.leftMargin: 12
                                    Layout.bottomMargin: 4
                                    visible: backend.categories.length > 0
                                    text: qsTr("Folders")
                                    color: theme.faint
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                                Repeater {
                                    model: backend.categories
                                    SideEntry {
                                        required property string modelData
                                        label: modelData
                                        count: backend.torrents.categoryCounts[modelData] || 0
                                        current: backend.torrents.categoryFilter === modelData
                                        onClicked: { backend.torrents.tagFilter = ""; backend.torrents.categoryFilter = modelData }
                                    }
                                }
                                SideEntry {
                                    visible: (backend.torrents.categoryCounts["__none__"] || 0) > 0
                                    label: qsTr("Unsorted")
                                    count: backend.torrents.categoryCounts["__none__"] || 0
                                    current: backend.torrents.categoryFilter === "__none__"
                                    onClicked: { backend.torrents.tagFilter = ""; backend.torrents.categoryFilter = "__none__" }
                                }

                                Label {
                                    textFormat: Text.PlainText
                                    Layout.topMargin: 14
                                    Layout.leftMargin: 12
                                    Layout.bottomMargin: 4
                                    visible: Object.keys(backend.torrents.tagCounts).length > 0
                                    text: qsTr("Tags")
                                    color: theme.faint
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                                Repeater {
                                    model: Object.keys(backend.torrents.tagCounts).sort()
                                    SideEntry {
                                        required property string modelData
                                        label: modelData
                                        count: backend.torrents.tagCounts[modelData] || 0
                                        current: backend.torrents.tagFilter === modelData
                                        onClicked: { backend.torrents.categoryFilter = ""; backend.torrents.tagFilter = modelData }
                                    }
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: theme.hover }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 0

                        TextField {
                            id: filterField
                            Layout.fillWidth: true
                            Layout.margins: 12
                            Layout.bottomMargin: 4
                            Material.roundedScale: theme.radius
                            placeholderText: qsTr("Filter downloads")
                            onTextChanged: backend.torrents.textFilter = text
                            Keys.onDownPressed: { pageStack.forceActiveFocus(); win.selectRow(0) }
                            onAccepted: { pageStack.forceActiveFocus(); win.selectRow(0) }
                        }

                        ListView {
                            id: torrentList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: backend.torrents
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            bottomMargin: 12

                            section.property: "group"
                            section.criteria: ViewSection.FullString
                            section.delegate: Item {
                                required property string section
                                width: ListView.view.width
                                height: 34
                                Label {
                                    textFormat: Text.PlainText
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 6
                                    x: 20
                                    text: section === "attention" ? qsTr("Needs attention")
                                        : section === "downloading" ? qsTr("Downloading")
                                        : section === "waiting" ? qsTr("Waiting")
                                        : section === "stopped" ? qsTr("Stopped")
                                        : qsTr("Done")
                                    color: section === "attention" ? theme.urgent : theme.faint
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            delegate: TorrentRow {
                                selected: hash === win.selectedHash
                                onClicked: { win.selectedHash = hash; pageStack.forceActiveFocus() }
                                onActivated: backend.openFolder(hash)
                                onToggleRequested: backend.toggle(hash)
                                onRemoveRequested: removeDialog.visible = true
                                onCategoryRequested: win.chooseCategory()
                                onTagsRequested: win.editTags()
                            }
                        }

                        // Nothing to show: say how to add something.
                        ColumnLayout {
                            visible: win.engineReady && backend.torrents.count === 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.alignment: Qt.AlignCenter
                            spacing: 8
                            Item { Layout.fillHeight: true }
                            Label {
                                textFormat: Text.PlainText
                                Layout.alignment: Qt.AlignHCenter
                                text: backend.torrents.total === 0 ? qsTr("No downloads yet") : qsTr("Nothing matches")
                                color: theme.foreground
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                            Label {
                                textFormat: Text.PlainText
                                Layout.alignment: Qt.AlignHCenter
                                Layout.maximumWidth: 420
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                visible: backend.torrents.total === 0
                                text: qsTr("Paste a magnet link with Ctrl+V, drop a .torrent file here, press A to pick one, or press 2 to search.")
                                color: theme.dim
                                font.pixelSize: 13
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                }

                // --- Search ---
                ColumnLayout {
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        Layout.bottomMargin: 4
                        spacing: 10

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            Material.roundedScale: theme.radius
                            placeholderText: qsTr("Search torrents")
                            onAccepted: {
                                backend.search(text)
                                win.resultIndex = 0
                                pageStack.forceActiveFocus()
                            }
                            Keys.onDownPressed: pageStack.forceActiveFocus()
                        }
                        BusyIndicator {
                            visible: backend.searching
                            running: visible
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                        }
                        Label {
                            textFormat: Text.PlainText
                            visible: backend.results.count > 0 || (backend.searchPattern !== "" && !backend.searching)
                            text: qsTr("%n result(s)", "", backend.results.count)
                            color: theme.dim
                            font.pixelSize: 12
                        }
                        DialogButton {
                            text: qsTr("Engines")
                            onClicked: engines.open()
                        }
                    }

                    ListView {
                        id: resultList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        visible: backend.results.count > 0
                        model: backend.results
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        topMargin: 6
                        bottomMargin: 12
                        delegate: ResultRow {
                            selected: index === win.resultIndex
                            onClicked: { win.resultIndex = index; pageStack.forceActiveFocus() }
                            onActivated: win.downloadResult(index)
                        }
                    }

                    Item {
                        visible: backend.results.count === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(460, parent.width - 48)
                        spacing: 10
                        readonly property bool noEngines: {
                            for (var i = 0; i < backend.plugins.length; i++)
                                if (backend.plugins[i].enabled)
                                    return false
                            return true
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.alignment: Qt.AlignHCenter
                            text: parent.noEngines ? qsTr("No search engines yet")
                                : backend.searching ? qsTr("Searching…")
                                : backend.searchPattern !== "" ? qsTr("Nothing found for “%1”").arg(backend.searchPattern)
                                : qsTr("Type what you're looking for and press Enter")
                            color: theme.foreground
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.alignment: Qt.AlignHCenter
                            Layout.maximumWidth: 440
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            visible: parent.noEngines
                            text: qsTr("Torrente searches through qBittorrent's search plugins. Choose which sites to search first.")
                            color: theme.dim
                            font.pixelSize: 13
                        }
                        DialogButton {
                            Layout.alignment: Qt.AlignHCenter
                            visible: parent.noEngines
                            primary: true
                            text: qsTr("Choose search engines")
                            onClicked: engines.open()
                        }
                    }
                    }
                }
            }
        }

        // Drop .torrent files or magnet text anywhere on the window.
        DropArea {
            anchors.fill: parent
            onDropped: (drop) => {
                var sources = []
                if (drop.hasUrls)
                    for (var i = 0; i < drop.urls.length; i++)
                        sources.push(drop.urls[i].toString())
                else if (drop.hasText)
                    sources = drop.text.split(/\s+/)
                backend.addSources(sources)
                drop.accept()
            }
        }

        EngineNotice {
            anchors.fill: parent
            anchors.topMargin: 52
            visible: !win.engineReady
        }

        // A short message at the bottom, gone after a few seconds.
        Rectangle {
            id: toastBox
            property bool problem: false
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            width: Math.min(toastLabel.implicitWidth + 36, parent.width - 48)
            height: toastLabel.implicitHeight + 20
            radius: Math.min(6, theme.radius)
            color: theme.background
            border.width: 1
            border.color: problem ? theme.urgent : theme.line
            opacity: 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 180 } }
            Label {
                textFormat: Text.PlainText
                id: toastLabel
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.parent.width - 84)
                wrapMode: Text.WordWrap
                color: toastBox.problem ? theme.urgent : theme.foreground
                font.pixelSize: 13
            }
            Timer {
                id: toastTimer
                interval: 3500
                onTriggered: toastBox.opacity = 0
            }
        }

        HelpPanel { id: help; onDismissed: win.closeOverlay() }
        SettingsPanel { id: settings; onDismissed: win.closeOverlay() }
        EnginesPanel { id: engines; onDismissed: win.closeOverlay() }
        ListPicker {
            id: categoryPicker
            title: qsTr("Category")
            onDismissed: win.closeOverlay()
            onChosen: (value) => {
                backend.setCategory(win.selectedHash, value)
                win.closeOverlay()
            }
        }
        TagsEditor {
            id: tagsEditor
            title: qsTr("Tags")
            onDismissed: win.closeOverlay()
            onAccepted: (tags) => {
                backend.setTags(win.selectedHash, tags)
                win.closeOverlay()
            }
        }
        ConfirmDialog {
            id: removeDialog
            visible: false
            title: qsTr("Remove this download?")
            message: {
                var item = win.selectedTorrent()
                return item ? item.name : ""
            }
            initialFocus: 0
            buttons: [
                { text: qsTr("Cancel"), mnemonic: qsTr("C") },
                { text: qsTr("Remove, keep files"), mnemonic: qsTr("R"), primary: true },
                { text: qsTr("Delete files too"), mnemonic: qsTr("D") },
            ]
            onDismissed: win.closeOverlay()
            onChosen: (index) => {
                if (index > 0)
                    backend.remove(win.selectedHash, index === 2)
                win.closeOverlay()
            }
        }
    }
}
