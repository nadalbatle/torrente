// What the Torrente bar plugin knows about the engine, built from
// qBittorrent's sync/maindata replies. Everything from the engine is
// untrusted: sizes are bounded here, names are cleaned to one plain line, and
// callers show them with Text.PlainText and never hand them to a shell.

var MAX_RESPONSE = 4 * 1024 * 1024
var MAX_TORRENTS = 5000
var MAX_NAME = 120
var MAX_PATH = 4096
var MAX_MAGNET = 8192
var SHOWN_ACTIVE = 6
var SHOWN_FINISHED = 3
var FINISHED_WINDOW = 24 * 60 * 60

var DOWNLOADING = ["downloading", "forcedDL", "metaDL", "forcedMetaDL", "stalledDL", "queuedDL", "checkingDL",
  "allocating", "moving", "checkingResumeData"]
var STOPPED = ["stoppedDL", "pausedDL"]
var PROBLEM = ["error", "missingFiles"]

function cleanName(raw) {
  var text = String(raw == null ? "" : raw)
  if (text.length > MAX_NAME * 4) text = text.slice(0, MAX_NAME * 4)
  text = text.replace(/[\u0000-\u001f\u007f]/g, " ")
  text = text.replace(/[‪-‮⁦-⁩‎‏﻿]/g, "")
  text = text.replace(/\s+/g, " ").trim()
  if (text.length > MAX_NAME) text = text.slice(0, MAX_NAME - 1).trim() + "…"
  return text
}

function cleanPath(raw) {
  var text = String(raw == null ? "" : raw)
  if (text.length === 0 || text.length > MAX_PATH || text.charAt(0) !== "/") return ""
  if (/[\u0000-\u001f\u007f]/.test(text)) return ""
  return text
}

function number(raw, fallback) {
  var n = Number(raw)
  return isFinite(n) ? n : fallback
}

function emptyState() {
  return { rid: 0, torrents: {}, server: {} }
}

// Parses a reply body; null when it is too big or not the object we expect.
function parseReply(body) {
  var text = String(body == null ? "" : body)
  if (text.length === 0 || text.length > MAX_RESPONSE) return null
  try {
    var data = JSON.parse(text)
    return data && typeof data === "object" && !Array.isArray(data) ? data : null
  } catch (e) {
    return null
  }
}

// Folds one sync/maindata reply into the state and returns the new state.
function merge(state, data) {
  var next = data.full_update || !state ? emptyState() : {
    rid: state.rid,
    torrents: Object.assign({}, state.torrents),
    server: Object.assign({}, state.server)
  }
  next.rid = Math.max(0, Math.floor(number(data.rid, next.rid)))

  var changed = data.torrents && typeof data.torrents === "object" ? data.torrents : {}
  var count = Object.keys(next.torrents).length
  for (var hash in changed) {
    if (!/^[0-9a-f]{40}$|^[0-9a-f]{64}$/.test(hash)) continue
    var fields = changed[hash]
    if (!fields || typeof fields !== "object") continue
    var known = next.torrents[hash]
    if (!known) {
      if (count >= MAX_TORRENTS) continue
      count++
      known = { hash: hash }
    }
    var merged = Object.assign({}, known)
    if ("name" in fields) merged.name = cleanName(fields.name)
    if ("state" in fields) merged.state = String(fields.state).slice(0, 32)
    if ("progress" in fields) merged.progress = Math.max(0, Math.min(1, number(fields.progress, 0)))
    if ("dlspeed" in fields) merged.dlspeed = Math.max(0, number(fields.dlspeed, 0))
    if ("eta" in fields) merged.eta = number(fields.eta, 8640000)
    if ("size" in fields) merged.size = Math.max(0, number(fields.size, 0))
    if ("added_on" in fields) merged.addedOn = number(fields.added_on, 0)
    if ("completion_on" in fields) merged.completionOn = number(fields.completion_on, 0)
    if ("save_path" in fields) merged.savePath = cleanPath(fields.save_path)
    next.torrents[hash] = merged
  }
  var removed = Array.isArray(data.torrents_removed) ? data.torrents_removed : []
  for (var i = 0; i < removed.length; i++) delete next.torrents[String(removed[i])]

  var server = data.server_state && typeof data.server_state === "object" ? data.server_state : {}
  if ("dl_info_speed" in server) next.server.download = Math.max(0, number(server.dl_info_speed, 0))
  if ("up_info_speed" in server) next.server.upload = Math.max(0, number(server.up_info_speed, 0))
  return next
}

function isUnfinished(t) {
  return (t.progress || 0) < 1 && DOWNLOADING.concat(STOPPED, PROBLEM).indexOf(t.state) >= 0
}

function isStopped(t) {
  return STOPPED.indexOf(t.state) >= 0
}

// What a row says about where a download is: one of the keys the panel
// translates.
function status(t) {
  if (PROBLEM.indexOf(t.state) >= 0) return "problem"
  if (isStopped(t)) return "stopped"
  if (t.state === "metaDL" || t.state === "forcedMetaDL") return "metadata"
  if (t.state === "queuedDL") return "queued"
  if (t.state === "stalledDL") return "stalled"
  if (t.state === "checkingDL" || t.state === "checkingResumeData" || t.state === "allocating") return "checking"
  if (t.state === "moving") return "moving"
  return "downloading"
}

// Downloads still under way, newest first, and the ones that finished in the
// last day, most recent first. `now` is in seconds.
function summarize(state, now) {
  var active = []
  var finished = []
  var downloading = 0
  for (var hash in state.torrents) {
    var t = state.torrents[hash]
    if (isUnfinished(t)) {
      active.push(t)
      if (!isStopped(t) && PROBLEM.indexOf(t.state) < 0) downloading++
    } else if ((t.progress || 0) >= 1 && t.completionOn > 0 && now - t.completionOn < FINISHED_WINDOW) {
      finished.push(t)
    }
  }
  active.sort(function(a, b) { return (b.addedOn || 0) - (a.addedOn || 0) || (a.hash < b.hash ? -1 : 1) })
  finished.sort(function(a, b) { return (b.completionOn || 0) - (a.completionOn || 0) || (a.hash < b.hash ? -1 : 1) })
  return {
    active: active.slice(0, SHOWN_ACTIVE),
    moreActive: Math.max(0, active.length - SHOWN_ACTIVE),
    activeCount: active.length,
    downloading: downloading,
    finished: finished.slice(0, SHOWN_FINISHED),
    download: state.server.download || 0,
    upload: state.server.upload || 0
  }
}

// A size as [value, unit, decimals], in the binary units Torrente's window uses.
function size(bytes) {
  var units = ["B", "KB", "MB", "GB", "TB", "PB"]
  var value = Math.max(0, number(bytes, 0))
  var unit = 0
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024
    unit++
  }
  return [value, units[unit], unit === 0 || value >= 100 ? 0 : 1]
}

// Time left as [key, hours, minutes] for the panel to translate, or null when
// the engine doesn't know.
function eta(seconds) {
  var s = number(seconds, -1)
  if (s < 0 || s >= 8640000) return null
  var minutes = Math.max(1, Math.ceil(s / 60))
  if (minutes < 60) return ["minutes", 0, minutes]
  return ["hours", Math.floor(minutes / 60), minutes % 60]
}

// The magnet link on the clipboard, or "" when there isn't exactly one.
function magnetFrom(text) {
  var value = String(text == null ? "" : text).trim()
  if (value.length === 0 || value.length > MAX_MAGNET) return ""
  if (/\s/.test(value)) return ""
  if (!/^magnet:\?([^#]*&)?xt=urn:bt(ih|mh):[0-9a-zA-Z]+/i.test(value)) return ""
  return value
}

function clampIndex(index, length) {
  if (length <= 0) return 0
  return Math.max(0, Math.min(length - 1, index))
}

if (typeof module !== "undefined") {
  module.exports = {
    cleanName: cleanName,
    cleanPath: cleanPath,
    emptyState: emptyState,
    parseReply: parseReply,
    merge: merge,
    status: status,
    isStopped: isStopped,
    summarize: summarize,
    size: size,
    eta: eta,
    magnetFrom: magnetFrom,
    clampIndex: clampIndex
  }
}
