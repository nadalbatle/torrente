#include "classifier.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

namespace Classifier {

namespace {

enum class Kind { Video, Audio, Book, Software, Ignored, Unknown };

Kind kindOf(const QString &fileName) {
    static const QHash<QString, Kind> kinds = [] {
        QHash<QString, Kind> map;
        for (const char *ext : {"mkv", "mp4", "avi", "m4v", "mov", "wmv", "ts", "m2ts", "webm", "mpg", "mpeg", "vob"})
            map.insert(ext, Kind::Video);
        for (const char *ext : {"mp3", "flac", "m4a", "aac", "ogg", "opus", "wav", "alac", "ape", "wv", "aiff", "dsf"})
            map.insert(ext, Kind::Audio);
        for (const char *ext : {"epub", "pdf", "mobi", "azw", "azw3", "cbz", "cbr", "djvu", "fb2"})
            map.insert(ext, Kind::Book);
        for (const char *ext : {"iso", "img", "dmg", "exe", "msi", "deb", "rpm", "appimage", "zst", "apk", "bin", "pkg"})
            map.insert(ext, Kind::Software);
        // Sidecar files say nothing about what a torrent is.
        for (const char *ext : {"nfo", "txt", "jpg", "jpeg", "png", "srt", "ass", "sub", "idx", "sfv", "md5", "url",
                                "cue", "log", "m3u", "nzb", "torrent"})
            map.insert(ext, Kind::Ignored);
        return map;
    }();
    return kinds.value(QFileInfo(fileName).suffix().toLower(), Kind::Unknown);
}

// Whole-word matching over a name whose dots, dashes and underscores read as
// spaces, the way release names are written.
QString normalized(const QString &name) {
    QString text = name.toLower();
    static const QRegularExpression separators(QStringLiteral("[._\\-\\[\\]()]+"));
    text.replace(separators, QStringLiteral(" "));
    return QStringLiteral(" ") + text + QStringLiteral(" ");
}

bool has(const QString &text, const char *pattern) {
    const QRegularExpression expression(QStringLiteral("(?<![a-z0-9])(?:%1)(?![a-z0-9])").arg(QLatin1String(pattern)));
    return expression.match(text).hasMatch();
}

bool looksLikeSeries(const QString &text) {
    return has(text, "s\\d{1,2}e\\d{1,3}|s\\d{1,2}|\\d{1,2}x\\d{2}|season|seasons|temporada|temporadas|"
                     "complete series|serie completa|cap\\s?\\d{1,4}|episodio|episode|miniserie|miniseries");
}

bool looksLikeGame(const QString &text) {
    return has(text, "repack|fitgirl|dodi|gog|codex|plaza|elamigos|skidrow|rune|tenoke|flt|razor1911|"
                     "game|games|juego|ps4|ps5|switch|nsp|xci");
}

} // namespace

QStringList categoryIds() {
    return {QStringLiteral("Movies"), QStringLiteral("Series"), QStringLiteral("Music"), QStringLiteral("Books"),
            QStringLiteral("Software"), QStringLiteral("Games"), QStringLiteral("Other")};
}

Result classify(const QString &name, const QList<File> &files) {
    Result result;
    QString text = normalized(name);
    for (const File &file : files)
        text += normalized(file.name);

    // The kind that holds the most bytes wins, so a film with a cover and
    // subtitles is still a film.
    QHash<Kind, qint64> bytes;
    for (const File &file : files)
        bytes[kindOf(file.name)] += qMax<qint64>(file.size, 1);
    bytes.remove(Kind::Ignored);

    Kind kind = Kind::Unknown;
    qint64 most = 0;
    for (auto it = bytes.cbegin(); it != bytes.cend(); ++it) {
        if (it.key() != Kind::Unknown && it.value() > most) {
            most = it.value();
            kind = it.key();
        }
    }

    if (files.isEmpty()) {
        result.category.clear();
    } else if (kind == Kind::Video) {
        result.category = looksLikeSeries(text) ? QStringLiteral("Series") : QStringLiteral("Movies");
    } else if (kind == Kind::Audio) {
        result.category = QStringLiteral("Music");
    } else if (kind == Kind::Book) {
        result.category = QStringLiteral("Books");
    } else if (looksLikeGame(text)) {
        result.category = QStringLiteral("Games");
    } else if (kind == Kind::Software) {
        result.category = QStringLiteral("Software");
    } else {
        result.category = QStringLiteral("Other");
    }

    // Tags describe the release, so they are read from the name even before
    // the files are known. They stay as release names write them (4K, HEVC),
    // the same in every language.
    auto tag = [&](const char *pattern, const char *label) {
        if (has(text, pattern) && !result.tags.contains(QLatin1String(label)))
            result.tags.append(QLatin1String(label));
    };
    tag("2160p|4k|uhd", "4K");
    tag("1080p|1080i|fullhd", "1080p");
    tag("720p", "720p");
    tag("hdr|hdr10|hdr10plus|dv|dolby vision|dovi", "HDR");
    tag("x265|h265|h 265|hevc", "HEVC");
    tag("av1", "AV1");
    tag("remux", "Remux");
    tag("bluray|blu ray|bdrip|brrip|bdremux", "BluRay");
    tag("web dl|webdl|webrip|web", "WEB");
    tag("flac", "FLAC");
    tag("24bit|24 bit|hi res|hires", "Hi-Res");
    tag("mp3|320kbps|320k", "MP3");
    tag("castellano|spanish|español|espanol|esp|spa", "ES");
    tag("latino|lat", "LAT");
    tag("vose|vos|subs|subtitulado", "VOSE");
    tag("multi|dual|multi audio|dual audio", "MULTI");
    // A 4K release is not also a 1080p one.
    if (result.tags.contains(QStringLiteral("4K")))
        result.tags.removeAll(QStringLiteral("1080p"));
    return result;
}

} // namespace Classifier
