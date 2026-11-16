// Run with: node plugin/tests/Model.test.js
const assert = require("assert")
const Model = require("../Model.js")

const A = "a".repeat(40)
const B = "b".repeat(40)
const C = "c".repeat(64)

let s = Model.merge(null, { rid: 3, full_update: true, torrents: {
  [A]: { name: "Big <b>Buck</b>\nBunny", state: "downloading", progress: 0.5, dlspeed: 2048, eta: 90, added_on: 100, save_path: "/home/u/Torrente/Movies" },
  [B]: { name: "Old", state: "stalledUP", progress: 1, completion_on: 1000, added_on: 50 },
  "not-a-hash": { name: "ignored" }
}, server_state: { dl_info_speed: 2048, up_info_speed: 10 } })

assert.strictEqual(s.rid, 3)
assert.strictEqual(Object.keys(s.torrents).length, 2, "malformed hashes are dropped")
assert.strictEqual(s.torrents[A].name, "Big <b>Buck</b> Bunny", "names stay one line; markup is left for PlainText")

s = Model.merge(s, { rid: 4, torrents: { [A]: { progress: 0.75 }, [C]: { name: "New", state: "metaDL", progress: 0, added_on: 200 } } })
assert.strictEqual(s.torrents[A].progress, 0.75, "partial updates merge")
assert.strictEqual(s.torrents[A].name, "Big <b>Buck</b> Bunny", "fields not sent are kept")

const sum = Model.summarize(s, 1000 + 60)
assert.deepStrictEqual(sum.active.map(t => t.hash), [C, A], "newest download first")
assert.strictEqual(sum.downloading, 2)
assert.deepStrictEqual(sum.finished.map(t => t.hash), [B], "finished in the last day")
assert.strictEqual(Model.summarize(s, 1000 + 2 * 86400).finished.length, 0, "older ones drop off")
assert.strictEqual(Model.status(s.torrents[C]), "metadata")

s = Model.merge(s, { rid: 5, torrents: { [A]: { state: "stoppedDL" } }, torrents_removed: [C] })
assert.strictEqual(s.torrents[C], undefined, "removed torrents go")
assert.strictEqual(Model.status(s.torrents[A]), "stopped")
assert.strictEqual(Model.summarize(s, 0).downloading, 0, "a stopped download isn't downloading")

assert.strictEqual(Model.merge(s, { full_update: true, rid: 9 }).torrents[A], undefined, "a full update starts over")

assert.strictEqual(Model.cleanName("x".repeat(500)).length, 120)
assert.strictEqual(Model.cleanName("a‮b"), "ab", "bidi overrides are stripped")
assert.strictEqual(Model.cleanPath("relative/path"), "")
assert.strictEqual(Model.cleanPath("/ok/path"), "/ok/path")
assert.strictEqual(Model.cleanPath("/bad\npath"), "")

assert.strictEqual(Model.parseReply("x".repeat(5 * 1024 * 1024)), null, "oversized replies are refused")
assert.strictEqual(Model.parseReply("[1,2]"), null, "only objects")
assert.strictEqual(Model.parseReply("{oops"), null)

assert.deepStrictEqual(Model.size(512), [512, "B", 0])
assert.deepStrictEqual(Model.size(1536), [1.5, "KB", 1])
assert.deepStrictEqual(Model.eta(90), ["minutes", 0, 2])
assert.deepStrictEqual(Model.eta(3 * 3600 + 120), ["hours", 3, 2])
assert.strictEqual(Model.eta(8640000), null, "the engine's 'unknown'")

const magnet = "magnet:?xt=urn:btih:" + "f".repeat(40) + "&dn=Test"
assert.strictEqual(Model.magnetFrom("  " + magnet + "\n"), magnet)
assert.strictEqual(Model.magnetFrom("https://example.com/x.torrent"), "")
assert.strictEqual(Model.magnetFrom(magnet + " " + magnet), "", "one link only")
assert.strictEqual(Model.magnetFrom("magnet:?dn=no-hash"), "")
assert.strictEqual(Model.magnetFrom("magnet:?xt=urn:btih:" + "f".repeat(9000)), "", "bounded")

console.log("Model.test.js: all passed")
