.pragma library

// Sizes and speeds in the units torrent clients use, with the language's
// own number separators.
function size(bytes, locale) {
    if (bytes === undefined || bytes < 0)
        return "–"
    var units = ["B", "KB", "MB", "GB", "TB", "PB"]
    var value = bytes
    var unit = 0
    while (value >= 1024 && unit < units.length - 1) {
        value /= 1024
        unit++
    }
    var digits = unit === 0 || value >= 100 ? 0 : 1
    return value.toLocaleString(locale, 'f', digits) + " " + units[unit]
}

function speed(bytesPerSecond, locale) {
    return size(bytesPerSecond, locale) + "/s"
}

function percent(fraction, locale) {
    var value = Math.floor(fraction * 1000) / 10
    return value.toLocaleString(locale, 'f', value >= 100 || value === 0 ? 0 : 1) + " %"
}
