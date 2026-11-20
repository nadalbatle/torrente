import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "Model.js" as Model

// Torrente on the bar: download speed while something downloads, and a panel
// with the downloads under way, the ones that just finished, and quick ways to
// add a magnet or open Torrente. It talks to Torrente's engine
// (qbittorrent-nox) over its Web API on localhost; sorting into folders and
// tagging is done by `torrente daemon`, so the plugin only adds and controls.
//
// Everything the engine sends is untrusted: Model.js bounds and cleans it,
// text is shown as Text.PlainText, and commands get argv lists, never a shell
// line built from it.
Panel {
  id: root
  moduleName: "torrente.panel"
  ipcTarget: "torrente.panel"
  manageIpc: false

  // md-download, one supplementary-plane code point, so the bar treats it as
  // an icon.
  readonly property string downloadIcon: "󰇚"
  readonly property string pauseIcon: "󰏤"
  readonly property string playIcon: "󰐊"
  readonly property string folderIcon: "󰉋"
  readonly property string pasteIcon: "󰆒"
  readonly property string openIcon: "󰏌"
  readonly property string engineIcon: "󰐥"
  readonly property string removeIcon: "󰆴"

  // The row whose remove button was clicked once and waits for a second click.
  property string confirmingHash: ""

  readonly property color foreground: bar ? bar.foreground : Commons.Color.foreground
  readonly property color urgent: bar ? bar.urgent : Commons.Color.urgent
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family
  readonly property color dim: Qt.darker(foreground, 1.45)

  readonly property int port: {
    var n = parseInt(setting("port", 8080), 10)
    return isFinite(n) && n > 0 && n < 65536 ? n : 8080
  }
  readonly property bool hideWhenIdle: setting("hideWhenIdle", false) === true
  readonly property string base: "http://127.0.0.1:" + port + "/api/v2/"

  // "connecting", "ready", "offline" or "forbidden".
  property string engine: "connecting"
  property var state: Model.emptyState()
  property var summary: Model.summarize(Model.emptyState(), 0)
  property bool syncing: false
  property string notice: ""
  property bool noticeProblem: false

  property int cursorIndex: 0
  property bool cursorActive: false

  readonly property bool busy: engine === "ready" && summary.downloading > 0

  // Rows the keyboard cursor walks, top to bottom.
  readonly property var rows: {
    var list = []
    if (engine === "offline") list.push({ kind: "start" })
    for (var i = 0; i < summary.active.length; i++) list.push({ kind: "active", torrent: summary.active[i] })
    for (var j = 0; j < summary.finished.length; j++) list.push({ kind: "finished", torrent: summary.finished[j] })
    if (engine === "ready") list.push({ kind: "paste" })
    list.push({ kind: "open" })
    return list
  }

  function rowIndex(kind, hash) {
    for (var i = 0; i < rows.length; i++) {
      if (rows[i].kind === kind && (!hash || (rows[i].torrent && rows[i].torrent.hash === hash))) return i
    }
    return -1
  }

  function hasCursor(kind, hash) {
    return cursorActive && rowIndex(kind, hash) === cursorIndex
  }

  function setCursor(kind, hash) {
    var i = rowIndex(kind, hash)
    if (i < 0) return
    cursorActive = true
    cursorIndex = i
  }

  function moveCursor(dy) {
    if (!cursorActive) {
      cursorActive = true
      cursorIndex = 0
      return
    }
    cursorIndex = Model.clampIndex(cursorIndex + dy, rows.length)
  }

  // --- Talking to the engine -------------------------------------------------

  function request(method, path, form, done) {
    var xhr = new XMLHttpRequest()
    var finished = false
    var timer = Qt.createQmlObject('import QtQuick; Timer { interval: 4000 }', root)
    timer.triggered.connect(function() {
      if (finished) return
      finished = true
      xhr.abort()
      timer.destroy()
      done(0, "")
    })
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== XMLHttpRequest.DONE || finished) return
      finished = true
      timer.stop()
      timer.destroy()
      done(xhr.status, xhr.status === 200 ? xhr.responseText : "")
    }
    xhr.open(method, root.base + path)
    if (method === "POST") xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded")
    timer.start()
    xhr.send(form || null)
  }

  function sync() {
    if (syncing) return
    syncing = true
    request("GET", "sync/maindata?rid=" + state.rid, "", function(status, body) {
      syncing = false
      if (status === 0) return setEngine("offline")
      if (status === 401 || status === 403) return setEngine("forbidden")
      var data = Model.parseReply(body)
      if (!data) return
      engine = "ready"
      state = Model.merge(state, data)
      summary = Model.summarize(state, Date.now() / 1000)
    })
  }

  function setEngine(value) {
    if (engine === value) return
    engine = value
    state = Model.emptyState()
    summary = Model.summarize(state, 0)
  }

  function toggleTorrent(torrent) {
    if (!torrent) return
    var action = Model.isStopped(torrent) ? "start" : "stop"
    request("POST", "torrents/" + action, "hashes=" + torrent.hash, function(status) {
      if (status !== 200) say(strings.t("notice.engineRefused"), true)
      sync()
    })
  }

  // Unfinished downloads are cancelled (the partial files go too); finished
  // ones only leave the list, their files stay. Either takes a second click.
  function removeTorrent(torrent, cancel) {
    if (!torrent) return
    if (confirmingHash !== torrent.hash) {
      confirmingHash = torrent.hash
      confirmTimer.restart()
      say(strings.t(cancel ? "notice.confirmCancel" : "notice.confirmRemove"), true)
      return
    }
    confirmingHash = ""
    confirmTimer.stop()
    var form = "hashes=" + torrent.hash + "&deleteFiles=" + (cancel ? "true" : "false")
    request("POST", "torrents/delete", form, function(status) {
      if (status !== 200) say(strings.t("notice.engineRefused"), true)
      else say(strings.t(cancel ? "notice.cancelled" : "notice.removed"), false)
      sync()
    })
  }

  function openFolder(torrent) {
    if (!torrent || !torrent.savePath) return
    Util.execArgv(["xdg-open", torrent.savePath])
    root.close()
  }

  function openTorrente() {
    Util.execArgv(["torrente"])
    root.close()
  }

  function startEngine() {
    say(strings.t("notice.starting"), false)
    Util.execArgv(["systemctl", "--user", "start", "torrente-engine.service"])
    engine = "connecting"
    retryTimer.restart()
  }

  function pasteMagnet() {
    if (!pasteProc.running) pasteProc.running = true
  }

  function addMagnet(text) {
    var magnet = Model.magnetFrom(text)
    if (magnet === "") return say(strings.t("notice.noMagnet"), true)
    request("POST", "torrents/add", "urls=" + encodeURIComponent(magnet), function(status, body) {
      if (status !== 200 || String(body).trim() === "Fails.") return say(strings.t("notice.engineRefused"), true)
      say(strings.t("notice.added"), false)
      sync()
    })
  }

  function say(text, problem) {
    notice = text
    noticeProblem = problem
    noticeTimer.restart()
  }

  function activate(row) {
    if (!row) return
    if (row.kind === "active") toggleTorrent(row.torrent)
    else if (row.kind === "finished") openFolder(row.torrent)
    else if (row.kind === "paste") pasteMagnet()
    else if (row.kind === "open") openTorrente()
    else if (row.kind === "start") startEngine()
  }

  // --- Words -------------------------------------------------------------------

  function sizeText(bytes) {
    var s = Model.size(bytes)
    return strings.t("unit." + s[1]).arg(Commons.I18n.formatNumber(s[0], s[2]))
  }

  function speedText(bytes) {
    return strings.t("speed").arg(sizeText(bytes))
  }

  function percentText(fraction) {
    var value = Math.floor((fraction || 0) * 1000) / 10
    return strings.t("percent").arg(Commons.I18n.formatNumber(value, value >= 100 || value === 0 ? 0 : 1))
  }

  function etaText(seconds) {
    var e = Model.eta(seconds)
    if (!e) return ""
    if (e[0] === "minutes") return strings.t("eta.minutes").arg(Commons.I18n.formatNumber(e[2]))
    return strings.t("eta.hours").arg(Commons.I18n.formatNumber(e[1])).arg(Commons.I18n.formatNumber(e[2]))
  }

  function rowCaption(t) {
    var status = Model.status(t)
    var parts = [percentText(t.progress)]
    if (status === "downloading") {
      parts.push(speedText(t.dlspeed || 0))
      var left = etaText(t.eta)
      if (left !== "") parts.push(left)
    } else {
      parts.push(strings.t("status." + status))
    }
    return parts.join(strings.t("separator"))
  }

  function readout() {
    if (root.bar && root.bar.vertical) return root.downloadIcon
    if (!busy) return root.downloadIcon
    return strings.t("bar.reading").arg(root.downloadIcon).arg(speedText(summary.download))
  }

  readonly property string tooltipText: {
    if (engine === "offline") return strings.t("bar.offline")
    if (engine === "forbidden") return strings.t("bar.forbidden")
    if (engine !== "ready") return strings.t("panel.title")
    if (summary.activeCount === 0) return strings.t("bar.idle")
    return strings.tn("bar.downloading", summary.downloading).arg(Commons.I18n.formatNumber(summary.downloading))
      + strings.t("separator") + speedText(summary.download)
  }

  readonly property string heroMeta: {
    if (engine === "offline") return strings.t("hero.offline")
    if (engine === "forbidden") return strings.t("hero.forbidden")
    if (engine !== "ready") return strings.t("hero.connecting")
    return strings.t("hero.speeds").arg(speedText(summary.download)).arg(speedText(summary.upload))
  }

  readonly property string heroDetail: {
    if (engine !== "ready" || summary.downloading === 0) return ""
    return strings.tn("bar.downloading", summary.downloading).arg(Commons.I18n.formatNumber(summary.downloading))
  }

  function broadcast(method) {
    var items = bar && typeof bar.moduleWidgets === "function" ? bar.moduleWidgets(moduleName) : [root]
    for (var i = 0; i < items.length; i++) {
      if (items[i] && typeof items[i][method] === "function") items[i][method]()
    }
  }

  function refresh() { sync() }

  onOpenedChanged: if (opened) {
    confirmingHash = ""
    cursorActive = false
    cursorIndex = 0
    sync()
    Qt.callLater(function() {
      if (panel.loadedContent) panel.loadedContent.forceActiveFocus()
    })
  }

  onRowsChanged: cursorIndex = Model.clampIndex(cursorIndex, rows.length)

  Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  Process {
    id: pasteProc
    property string collected: ""
    command: ["wl-paste", "--no-newline", "--type", "text/plain"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: pasteProc.collected = String(text || "").slice(0, 16384)
    }
    onExited: function(code) {
      root.addMagnet(code === 0 ? pasteProc.collected : "")
      pasteProc.collected = ""
    }
  }

  // Quick while the panel is open or something downloads, slow otherwise.
  Timer {
    interval: root.opened ? 1000 : (root.summary.activeCount > 0 ? 2000 : 6000)
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: root.sync()
  }

  Timer {
    id: retryTimer
    interval: 1500
    onTriggered: root.sync()
  }

  Timer {
    id: confirmTimer
    interval: 4000
    onTriggered: root.confirmingHash = ""
  }

  Timer {
    id: noticeTimer
    interval: 4000
    onTriggered: root.notice = ""
  }

  ShellIpc {
    target: root.ipcTarget

    function open(): void { root.open() }
    function close(): void { root.close() }
    function show(): void { root.open() }
    function hide(): void { root.close() }
    function toggle(): void { root.toggle() }
    function refresh(): void { root.broadcast("refresh") }
    function paste(): void { root.pasteMagnet() }
  }

  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.readout()
    tooltipText: root.tooltipText
    dimmed: root.engine === "offline" || root.engine === "forbidden"
    concealed: root.hideWhenIdle && !root.busy && !root.opened
    useActiveColor: false
    fontSize: Style.font.body
    horizontalMargin: 8
    onPressed: function(b) {
      if (b === Qt.LeftButton) root.toggle()
      else if (b === Qt.MiddleButton) root.pasteMagnet()
      else if (b === Qt.RightButton) root.openTorrente()
    }
  }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: panel.loadedContent
    contentWidth: panel.fittedContentWidth(Style.space(400))
    contentHeight: panel.fittedContentHeight(panel.loadedContentHeight)

    content: PanelKeyCatcher {
      anchors.fill: parent
      implicitHeight: column.implicitHeight
      onMoveRequested: function(dx, dy) { if (dy !== 0) root.moveCursor(dy) }
      onActivateRequested: if (root.cursorActive) root.activate(root.rows[root.cursorIndex])
      onReturnRequested: {
        if (!root.cursorActive) return
        var row = root.rows[root.cursorIndex]
        if (row && row.kind === "active") root.openFolder(row.torrent)
        else root.activate(row)
      }
      onCloseRequested: root.close()
      onTabRequested: function(direction) { root.switchPanel(direction) }
      onTextKey: function(t) {
        if (t === "p" || t === "P" || t === "v" || t === "V") root.pasteMagnet()
        else if (t === "o" || t === "O") root.openTorrente()
      }

      Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Style.space(12)

        PanelHero {
          title: strings.t("panel.title")
          meta: root.heroMeta
          detail: root.heroDetail
          foreground: root.foreground
          fontFamily: root.fontFamily
          iconComponent: Component {
            Text {
              textFormat: Text.PlainText
              text: root.downloadIcon
              color: root.foreground
              font.family: root.fontFamily
              font.pixelSize: Style.font.display
            }
          }
        }

        Column {
          visible: root.summary.active.length > 0
          width: parent.width
          spacing: Style.space(4)

          PanelSeparator { foreground: root.foreground }

          PanelSectionHeader {
            text: strings.t("section.downloading")
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Repeater {
            model: root.summary.active
            delegate: TorrentRow {
              required property var modelData
              torrent: modelData
              kind: "active"
              caption: root.rowCaption(modelData)
              problem: Model.status(modelData) === "problem"
              actionIcon: Model.isStopped(modelData) ? root.playIcon : root.pauseIcon
              actionTip: Model.isStopped(modelData) ? strings.t("action.resume") : strings.t("action.pause")
              onAction: root.toggleTorrent(modelData)
              removeTip: strings.t("action.cancel")
              onRemove: root.removeTorrent(modelData, true)
              onOpen: root.openFolder(modelData)
            }
          }

          Plain {
            visible: root.summary.moreActive > 0
            text: strings.tn("section.more", root.summary.moreActive).arg(Commons.I18n.formatNumber(root.summary.moreActive))
            color: root.dim
            font.pixelSize: Style.font.caption
            leftPadding: Style.space(8)
          }
        }

        Column {
          visible: root.summary.finished.length > 0
          width: parent.width
          spacing: Style.space(4)

          PanelSeparator { foreground: root.foreground }

          PanelSectionHeader {
            text: strings.t("section.finished")
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Repeater {
            model: root.summary.finished
            delegate: TorrentRow {
              required property var modelData
              torrent: modelData
              kind: "finished"
              caption: root.sizeText(modelData.size || 0)
              actionIcon: root.folderIcon
              actionTip: strings.t("action.openFolder")
              onAction: root.openFolder(modelData)
              removeTip: strings.t("action.remove")
              onRemove: root.removeTorrent(modelData, false)
              onOpen: root.openFolder(modelData)
            }
          }
        }

        Plain {
          visible: root.engine === "ready" && root.summary.activeCount === 0 && root.summary.finished.length === 0
          width: parent.width
          text: strings.t("panel.empty")
          color: root.dim
          wrapMode: Text.WordWrap
          font.pixelSize: Style.font.bodySmall
        }

        Plain {
          visible: root.engine === "forbidden"
          width: parent.width
          text: strings.t("panel.forbidden")
          color: root.dim
          wrapMode: Text.WordWrap
          font.pixelSize: Style.font.bodySmall
        }

        PanelSeparator { foreground: root.foreground }

        Column {
          width: parent.width
          spacing: Style.space(2)

          ActionRow {
            visible: root.engine === "offline"
            kind: "start"
            icon: root.engineIcon
            label: strings.t("action.startEngine")
            shortcut: ""
          }
          ActionRow {
            visible: root.engine === "ready"
            kind: "paste"
            icon: root.pasteIcon
            label: strings.t("action.paste")
            shortcut: "P"
          }
          ActionRow {
            kind: "open"
            icon: root.openIcon
            label: strings.t("action.openTorrente")
            shortcut: "O"
          }
        }

        Plain {
          visible: root.notice !== ""
          width: parent.width
          text: root.notice
          color: root.noticeProblem ? root.urgent : root.dim
          wrapMode: Text.WordWrap
          font.pixelSize: Style.font.caption
          leftPadding: Style.space(8)
        }
      }
    }
  }

  component Plain: Text {
    textFormat: Text.PlainText
    color: root.foreground
    font.family: root.fontFamily
    font.pixelSize: Style.font.body
  }

  component TorrentRow: CursorSurface {
    id: row
    property var torrent: ({})
    property string kind: "active"
    property string caption: ""
    property bool problem: false
    property string actionIcon: ""
    property string actionTip: ""
    property string removeTip: ""
    readonly property bool confirming: root.confirmingHash !== "" && root.confirmingHash === torrent.hash
    signal action()
    signal remove()
    signal open()

    width: parent ? parent.width : 0
    implicitHeight: textColumn.implicitHeight + Style.space(12)
    hasCursor: root.hasCursor(kind, torrent.hash)
    foreground: root.foreground

    MouseArea {
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onEntered: root.setCursor(row.kind, row.torrent.hash)
      onClicked: row.open()
    }

    PanelActionButton {
      id: removeButton
      anchors.right: parent.right
      anchors.rightMargin: Style.space(6)
      anchors.verticalCenter: parent.verticalCenter
      iconText: root.removeIcon
      tooltipText: row.confirming ? strings.t("action.confirm") : row.removeTip
      foreground: row.confirming ? root.urgent : root.foreground
      hoverColor: root.urgent
      fontFamily: root.fontFamily
      onClicked: row.remove()
    }

    PanelActionButton {
      id: actionButton
      anchors.right: removeButton.left
      anchors.rightMargin: Style.space(2)
      anchors.verticalCenter: parent.verticalCenter
      iconText: row.actionIcon
      tooltipText: row.actionTip
      foreground: root.foreground
      fontFamily: root.fontFamily
      onClicked: row.action()
    }

    Column {
      id: textColumn
      anchors.left: parent.left
      anchors.leftMargin: Style.space(8)
      anchors.right: actionButton.left
      anchors.rightMargin: Style.space(8)
      anchors.verticalCenter: parent.verticalCenter
      spacing: Style.space(3)

      Plain {
        width: parent.width
        text: row.torrent.name || row.torrent.hash
        elide: Text.ElideRight
        font.pixelSize: Style.font.bodySmall
      }

      Rectangle {
        id: track
        visible: row.kind === "active"
        width: parent.width
        height: Style.space(4)
        radius: height / 2
        color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.12)

        Rectangle {
          height: parent.height
          radius: parent.radius
          color: row.problem ? root.urgent : (Model.isStopped(row.torrent) ? root.dim : Commons.Color.accent)
          width: Math.max(0, track.width * Math.max(0, Math.min(1, row.torrent.progress || 0)))

          Behavior on width {
            NumberAnimation { duration: Style.duration(180); easing.type: Easing.OutCubic }
          }
        }
      }

      Plain {
        width: parent.width
        text: row.caption
        color: row.problem ? root.urgent : root.dim
        elide: Text.ElideRight
        font.pixelSize: Style.font.caption
      }
    }
  }

  component ActionRow: CursorSurface {
    id: actionRow
    property string kind: ""
    property string icon: ""
    property string label: ""
    property string shortcut: ""

    width: parent ? parent.width : 0
    implicitHeight: visible ? labelText.implicitHeight + Style.space(12) : 0
    hasCursor: root.hasCursor(kind)
    foreground: root.foreground

    MouseArea {
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onEntered: root.setCursor(actionRow.kind)
      onClicked: root.activate({ kind: actionRow.kind })
    }

    Plain {
      id: iconText
      anchors.left: parent.left
      anchors.leftMargin: Style.space(8)
      anchors.verticalCenter: parent.verticalCenter
      text: actionRow.icon
      font.pixelSize: Style.font.icon
    }

    Plain {
      id: labelText
      anchors.left: iconText.right
      anchors.leftMargin: Style.space(10)
      anchors.right: keyText.left
      anchors.rightMargin: Style.space(8)
      anchors.verticalCenter: parent.verticalCenter
      text: actionRow.label
      elide: Text.ElideRight
      font.pixelSize: Style.font.bodySmall
    }

    Plain {
      id: keyText
      anchors.right: parent.right
      anchors.rightMargin: Style.space(10)
      anchors.verticalCenter: parent.verticalCenter
      text: actionRow.shortcut
      color: root.dim
      font.pixelSize: Style.font.caption
    }
  }
}
