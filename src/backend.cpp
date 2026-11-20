#include "backend.h"


#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include "classifier.h"
#include "filepicker.h"

namespace {
// qBittorrent's own search plugins (github.com/qbittorrent/search-plugins).
// Jackett is left out: it needs a Jackett server of its own.
const char *kPluginBase = "https://raw.githubusercontent.com/qbittorrent/search-plugins/master/nova3/engines/";
const QStringList kOfficialPlugins = {"eztv", "limetorrents", "piratebay", "solidtorrents",
                                      "torlock", "torrentproject", "torrentscsv"};

// Bencoded torrent metadata: a dictionary holding an info dictionary. A web
// page (a login form, an error) is neither.
bool looksLikeTorrent(const QByteArray &data) {
    return data.startsWith('d') && data.contains("4:info");
}

bool isWebLink(const QString &text) {
    return text.startsWith("http://", Qt::CaseInsensitive) || text.startsWith("https://", Qt::CaseInsensitive);
}

constexpr qint64 kMaxTorrentFile = 64 * 1024 * 1024;

bool isMagnetOrLink(const QString &text) {
    return text.startsWith("magnet:?", Qt::CaseInsensitive) || text.startsWith("http://", Qt::CaseInsensitive)
        || text.startsWith("https://", Qt::CaseInsensitive) || text.startsWith("bc://bt/", Qt::CaseInsensitive);
}
}

Backend::Backend(QbtClient *client, FilePicker *picker, QSettings *settings, QObject *parent)
    : QObject(parent), m_client(client), m_picker(picker), m_settings(settings) {
    m_syncTimer.setInterval(1000);
    connect(&m_syncTimer, &QTimer::timeout, this, &Backend::sync);

    m_downloadsDeadline.setSingleShot(true);
    m_downloadsDeadline.setInterval(5 * 60 * 1000);
    connect(&m_downloadsDeadline, &QTimer::timeout, this, [this] {
        const auto folders = m_downloadsWatcher.directories();
        if (!folders.isEmpty())
            m_downloadsWatcher.removePaths(folders);
    });
    connect(&m_downloadsWatcher, &QFileSystemWatcher::directoryChanged, this, &Backend::checkDownloads);

    m_searchTimer.setInterval(800);
    connect(&m_searchTimer, &QTimer::timeout, this, &Backend::pollSearch);

    connect(m_client, &QbtClient::unreachable, this, [this](const QString &message) {
        setEngineState(QStringLiteral("offline"), message);
    });
    connect(m_client, &QbtClient::forbidden, this, [this] { setEngineState(QStringLiteral("forbidden")); });

    connect(&m_engineStarter, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (code != 0 || status != QProcess::NormalExit) {
            const QString detail = QString::fromUtf8(m_engineStarter.readAllStandardError()).trimmed();
            m_engineError = detail.isEmpty() ? tr("systemctl could not start the engine.") : detail;
        }
        emit engineChanged();
        sync();
    });
    connect(&m_engineStarter, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_engineError = tr("systemctl is not available, so the engine can't be started from here.");
        emit engineChanged();
    });

    connect(m_picker, &FilePicker::torrentsSelected, this, [this](const QList<QUrl> &urls) {
        QStringList paths;
        for (const QUrl &url : urls)
            paths.append(url.toLocalFile());
        addLocalFiles(paths);
    });
    connect(m_picker, &FilePicker::folderSelected, this, [this](const QUrl &url) {
        const QString folder = url.toLocalFile();
        if (folder.isEmpty() || folder == downloadFolder())
            return;
        m_settings->setValue("downloadFolder", folder);
        emit settingsChanged();
        if (autoFolders()) {
            applyAutoSortPreferences();
            ensureCategories();
        }
        emit notify(tr("Downloads now go to %1").arg(folder), false);
    });
    connect(m_picker, &FilePicker::failed, this, [this](const QString &message) { emit notify(message, true); });

    sync();
    m_syncTimer.start();
}

// --- Settings ---------------------------------------------------------------

QString Backend::downloadFolder() const {
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return m_settings->value("downloadFolder", downloads + QStringLiteral("/Torrente")).toString();
}

bool Backend::autoFolders() const { return m_settings->value("autoFolders", true).toBool(); }
bool Backend::autoTags() const { return m_settings->value("autoTags", true).toBool(); }

void Backend::setAutoFolders(bool enabled) {
    if (enabled == autoFolders())
        return;
    m_settings->setValue("autoFolders", enabled);
    emit settingsChanged();
    if (enabled && m_engineState == QLatin1String("ready")) {
        applyAutoSortPreferences();
        ensureCategories();
        m_classified.clear();
    }
}

void Backend::setAutoTags(bool enabled) {
    if (enabled == autoTags())
        return;
    m_settings->setValue("autoTags", enabled);
    emit settingsChanged();
    if (enabled)
        m_classified.clear();
}

// --- Engine -----------------------------------------------------------------

void Backend::setEngineState(const QString &state, const QString &error) {
    if (state == m_engineState && error == m_engineError)
        return;
    const bool becameReady = state == QLatin1String("ready") && m_engineState != QLatin1String("ready");
    m_engineState = state;
    m_engineError = error;
    if (state != QLatin1String("ready")) {
        // Whatever the engine had is unknown until it answers again.
        m_rid = 0;
        m_torrents.clear();
        m_categories.clear();
        m_serverState = {};
        emit categoriesChanged();
        emit statsChanged();
        m_syncTimer.setInterval(2000);
    }
    emit engineChanged();
    if (becameReady)
        onReady();
}

void Backend::startEngine() {
    if (startingEngine())
        return;
    m_engineError.clear();
    // The package ships the unit; enabling it also starts it at every login.
    m_engineStarter.start(QStringLiteral("systemctl"),
                          {QStringLiteral("--user"), QStringLiteral("enable"), QStringLiteral("--now"),
                           QStringLiteral("torrente-engine.service")});
    emit engineChanged();
}

void Backend::sync() {
    if (m_syncInFlight)
        return;
    m_syncInFlight = true;
    QUrlQuery query;
    query.addQueryItem("rid", QString::number(m_rid));
    m_client->get("sync/maindata", query, [this](const QbtClient::Reply &reply) {
        m_syncInFlight = false;
        if (!reply.ok)
            return;
        const QJsonObject data = reply.json().object();
        m_rid = data.value("rid").toInteger();
        m_syncTimer.setInterval(m_syncInterval);
        setEngineState(QStringLiteral("ready"));

        if (data.value("full_update").toBool())
            m_categories.clear();
        const QJsonObject categories = data.value("categories").toObject();
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            QJsonObject merged = m_categories.value(it.key());
            const QJsonObject fields = it.value().toObject();
            for (auto f = fields.begin(); f != fields.end(); ++f)
                merged.insert(f.key(), f.value());
            m_categories.insert(it.key(), merged);
        }
        for (const QJsonValue &removed : data.value("categories_removed").toArray())
            m_categories.remove(removed.toString());
        if (!categories.isEmpty() || data.contains("categories_removed") || data.value("full_update").toBool())
            emit categoriesChanged();

        const QJsonObject server = data.value("server_state").toObject();
        for (auto it = server.begin(); it != server.end(); ++it)
            m_serverState.insert(it.key(), it.value());
        if (!server.isEmpty())
            emit statsChanged();

        m_torrents.apply(data);
        trackCompletion(data);
        classifyNew();
    });
}

void Backend::trackCompletion(const QJsonObject &data) {
    // A full update is a first look (or a reconnection): nothing in it counts
    // as having just finished.
    const bool firstLook = data.value("full_update").toBool();
    if (firstLook)
        m_progress.clear();
    for (const QJsonValue &removed : data.value("torrents_removed").toArray())
        m_progress.remove(removed.toString());
    const QJsonObject torrents = data.value("torrents").toObject();
    for (auto it = torrents.begin(); it != torrents.end(); ++it) {
        const QJsonObject fields = it.value().toObject();
        if (!fields.contains("progress"))
            continue;
        const double progress = fields.value("progress").toDouble();
        const auto previous = m_progress.constFind(it.key());
        // A recheck also takes progress below 1 and back; only a real finish
        // comes with a new completion time in the same update.
        const bool justFinished = !firstLook && previous != m_progress.constEnd() && *previous < 1.0
            && progress >= 1.0 && fields.value("completion_on").toDouble() > 0;
        m_progress.insert(it.key(), progress);
        if (justFinished)
            emit finished(it.key());
    }
}

// Torrente works best with qBittorrent's "bypass authentication on localhost",
// but together with the default server-domain list ("*") that lets any web
// page reach the engine through DNS rebinding — and the engine can install
// and run search plugins. When the engine only listens on this computer,
// accept nothing but local host names.
void Backend::hardenEngine() {
    m_client->get("app/preferences", {}, [this](const QbtClient::Reply &reply) {
        if (!reply.ok)
            return;
        const QJsonObject prefs = reply.json().object();
        const QString address = prefs.value("web_ui_address").toString();
        const bool localOnly = address == QLatin1String("127.0.0.1") || address == QLatin1String("::1")
            || address == QLatin1String("localhost");
        const QString domains = prefs.value("web_ui_domain_list").toString().trimmed();
        if (!localOnly || !prefs.value("bypass_local_auth").toBool() || (domains != QLatin1String("*") && !domains.isEmpty()))
            return;
        Form form;
        form.addQueryItem("json", QStringLiteral(R"({"web_ui_domain_list":"127.0.0.1;localhost;[::1]"})"));
        m_client->post("app/setPreferences", form, [this](const QbtClient::Reply &reply) {
            report(reply, tr("Could not secure the engine"));
        });
    });
}

void Backend::onReady() {
    hardenEngine();
    refreshPlugins();
    if (autoFolders()) {
        applyAutoSortPreferences();
        ensureCategories();
    }
}

// --- Sorting into folders and tagging ----------------------------------------

QString Backend::categoryLabel(const QString &id) {
    if (id == QLatin1String("Movies")) return tr("Movies");
    if (id == QLatin1String("Series")) return tr("Series");
    if (id == QLatin1String("Music")) return tr("Music");
    if (id == QLatin1String("Books")) return tr("Books");
    if (id == QLatin1String("Software")) return tr("Software");
    if (id == QLatin1String("Games")) return tr("Games");
    return tr("Other");
}

// A category's name is fixed the first time Torrente creates it, in the
// language of the day, so renaming the language later doesn't move anything.
QString Backend::categoryName(const QString &id) const {
    return m_settings->value(QStringLiteral("categories/") + id, categoryLabel(id)).toString();
}

QStringList Backend::categories() const {
    QStringList names = m_categories.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

void Backend::applyAutoSortPreferences() {
    const QString base = downloadFolder();
    QDir().mkpath(base);
    // Named once, like the categories: a later language switch must not send
    // half-finished downloads to a different folder.
    const QString incomplete =
        m_settings->value(QStringLiteral("incompleteFolder"), QStringLiteral(".") + tr("Incomplete").toLower()).toString();
    m_settings->setValue(QStringLiteral("incompleteFolder"), incomplete);
    // Automatic management moves a torrent to its category's folder; the
    // temporary folder keeps unfinished downloads out of the sorted ones.
    QJsonObject prefs{
        {"auto_tmm_enabled", true},
        {"torrent_changed_tmm_enabled", true},
        {"category_changed_tmm_enabled", true},
        {"save_path_changed_tmm_enabled", true},
        {"save_path", base},
        {"temp_path_enabled", true},
        {"temp_path", base + '/' + incomplete},
    };
    Form form;
    form.addQueryItem("json", QString::fromUtf8(QJsonDocument(prefs).toJson(QJsonDocument::Compact)));
    m_client->post("app/setPreferences", form, [this](const QbtClient::Reply &reply) {
        report(reply, tr("Could not set up the download folders"));
    });
}

void Backend::ensureCategories() {
    const QString base = downloadFolder();
    for (const QString &id : Classifier::categoryIds()) {
        const QString name = categoryName(id);
        m_settings->setValue(QStringLiteral("categories/") + id, name);
        const QString path = base + '/' + name;
        Form form;
        form.addQueryItem("category", name);
        form.addQueryItem("savePath", path);
        if (!m_categories.contains(name)) {
            m_client->post("torrents/createCategory", form, [this](const QbtClient::Reply &reply) {
                // 409 means it appeared meanwhile, which is fine.
                if (reply.status != 409)
                    report(reply, tr("Could not create a category"));
            });
        } else if (m_categories.value(name).value("savePath").toString() != path) {
            m_client->post("torrents/editCategory", form, [this](const QbtClient::Reply &reply) {
                report(reply, tr("Could not move a category"));
            });
        }
    }
}

void Backend::classifyNew() {
    if (!autoFolders() && !autoTags())
        return;
    for (const QString &hash : m_torrents.hashes()) {
        if (m_classified.contains(hash))
            continue;
        const QJsonObject t = m_torrents.torrent(hash);
        // Torrents the user (or an earlier run) already filed are left alone.
        if (!t.value("category").toString().isEmpty()) {
            m_classified.insert(hash);
            continue;
        }
        // A magnet knows its files only once the metadata arrives.
        const QString state = t.value("state").toString();
        const bool hasMetadata = t.contains("has_metadata") ? t.value("has_metadata").toBool()
                                                            : !state.contains("metaDL", Qt::CaseInsensitive);
        if (!hasMetadata)
            continue;
        classify(hash);
    }
}

void Backend::reclassify(const QString &hash) {
    m_classified.remove(hash);
    classify(hash);
}

void Backend::classify(const QString &hash) {
    m_classified.insert(hash);
    QUrlQuery query;
    query.addQueryItem("hash", hash);
    m_client->get("torrents/files", query, [this, hash](const QbtClient::Reply &reply) {
        if (!reply.ok) {
            // Try again on a later sync.
            m_classified.remove(hash);
            return;
        }
        QList<Classifier::File> files;
        for (const QJsonValue &value : reply.json().array()) {
            const QJsonObject file = value.toObject();
            files.append({file.value("name").toString(), qint64(file.value("size").toDouble())});
        }
        const auto result = Classifier::classify(m_torrents.torrent(hash).value("name").toString(), files);

        if (autoFolders() && !result.category.isEmpty()) {
            Form form;
            form.addQueryItem("hashes", hash);
            form.addQueryItem("category", categoryName(result.category));
            m_client->post("torrents/setCategory", form, [this, hash](const QbtClient::Reply &reply) {
                report(reply, tr("Could not file a download"));
                Form automatic;
                automatic.addQueryItem("hashes", hash);
                automatic.addQueryItem("enable", "true");
                m_client->post("torrents/setAutoManagement", automatic);
            });
        }
        if (autoTags() && !result.tags.isEmpty()) {
            Form form;
            form.addQueryItem("hashes", hash);
            form.addQueryItem("tags", result.tags.join(','));
            m_client->post("torrents/addTags", form);
        }
    });
}

// --- Adding -----------------------------------------------------------------

void Backend::addSources(const QStringList &sources) {
    QStringList links, files;
    for (const QString &raw : sources) {
        const QString source = raw.trimmed();
        if (source.isEmpty())
            continue;
        const QUrl url(source);
        if (url.isLocalFile())
            files.append(url.toLocalFile());
        else if (isMagnetOrLink(source))
            links.append(source);
        else if (QFileInfo::exists(source))
            files.append(QFileInfo(source).absoluteFilePath());
        else
            emit notify(tr("Not a magnet link or torrent file: %1").arg(source.left(80)), true);
    }

    QStringList engineLinks;
    for (const QString &link : links) {
        if (isWebLink(link))
            fetchTorrentLink(link);
        else
            engineLinks.append(link);
    }
    addLinks(engineLinks);
    if (!files.isEmpty())
        addLocalFiles(files);
}

// Magnets, and links Torrente couldn't fetch itself, go to the engine as they are.
void Backend::addLinks(const QStringList &links) {
    if (links.isEmpty())
        return;
    Form form;
    form.addQueryItem("urls", links.join('\n'));
    m_client->post("torrents/add", form, [this, count = links.size()](const QbtClient::Reply &reply) {
        if (reply.ok && reply.body.trimmed() != "Fails.")
            emit notify(tr("Added %n torrent(s)", "", count), false);
        else
            emit notify(tr("The engine refused the torrent: %1").arg(reply.error.isEmpty()
                                                                       ? QString::fromUtf8(reply.body)
                                                                       : reply.error), true);
    });
}

void Backend::fetchTorrentLink(const QString &link) {
    QNetworkRequest request{QUrl(link)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Torrente"));
    QNetworkReply *reply = m_web.get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > kMaxTorrentFile)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, link] {
        reply->deleteLater();
        const QString site = QUrl(link).host();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            if (status == 401 || status == 403) {
                // Like any other torrent app: the browser has the session, so
                // it fetches the .torrent, and Torrente takes it from there.
                QDesktopServices::openUrl(QUrl(link));
                watchDownloads();
                emit notify(tr("%1 wants you signed in, so the link is open in your browser. "
                               "The torrent comes in on its own once it downloads.").arg(site), false);
            }
            else if (status == 404 || status == 410)
                emit notify(tr("That torrent isn't on %1 any more (%2).").arg(site).arg(status), true);
            else if (status >= 400)
                emit notify(tr("%1 refused the torrent (%2).").arg(site).arg(status), true);
            else if (reply->error() == QNetworkReply::ProtocolUnknownError
                     || reply->error() == QNetworkReply::InsecureRedirectError)
                // Redirected somewhere Torrente doesn't follow (some trackers
                // send a magnet): the engine may still manage.
                addLinks({link});
            else if (reply->error() == QNetworkReply::HostNotFoundError)
                emit notify(tr("Can't find %1. Check the link and the connection.").arg(site), true);
            else if (reply->error() == QNetworkReply::TimeoutError
                     || reply->error() == QNetworkReply::OperationCanceledError)
                emit notify(tr("%1 didn't send the torrent.").arg(site), true);
            else
                emit notify(tr("Couldn't download the torrent from %1.").arg(site), true);
            return;
        }
        const QByteArray data = reply->readAll();
        if (!looksLikeTorrent(data)) {
            emit notify(tr("That link on %1 is a web page, not a torrent file.").arg(site), true);
            return;
        }
        QString name = QFileInfo(reply->url().path()).fileName();
        if (name.isEmpty())
            name = QStringLiteral("download.torrent");
        addTorrentData({{name, data}});
    });
}

void Backend::watchDownloads() {
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        return;
    if (!m_downloadsWatcher.directories().contains(folder)) {
        // What's already there was not just downloaded.
        m_downloadsSeen.clear();
        for (const QFileInfo &file : QDir(folder).entryInfoList({"*.torrent"}, QDir::Files))
            m_downloadsSeen.insert(file.absoluteFilePath() + '|' + QString::number(file.lastModified().toMSecsSinceEpoch()));
        m_downloadsWatcher.addPath(folder);
    }
    m_downloadsDeadline.start();
}

void Backend::checkDownloads() {
    for (const QString &folder : m_downloadsWatcher.directories()) {
        for (const QFileInfo &file : QDir(folder).entryInfoList({"*.torrent"}, QDir::Files)) {
            const QString key = file.absoluteFilePath() + '|' + QString::number(file.lastModified().toMSecsSinceEpoch());
            if (m_downloadsSeen.contains(key) || file.size() == 0 || file.size() > kMaxTorrentFile)
                continue;
            QFile in(file.absoluteFilePath());
            if (!in.open(QIODevice::ReadOnly))
                continue;
            const QByteArray data = in.readAll();
            // Still being written: the next change will bring it back.
            if (!looksLikeTorrent(data))
                continue;
            m_downloadsSeen.insert(key);
            addTorrentData({{file.fileName(), data}});
        }
    }
}

void Backend::addLocalFiles(const QStringList &paths) {
    QList<QPair<QString, QByteArray>> torrents;
    for (const QString &path : paths) {
        QFile file(path);
        // Torrent files are metadata, rarely more than a few megabytes; a
        // large file is a mistake and shouldn't be read into memory.
        if (file.size() > 64 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
            emit notify(tr("Can't read %1").arg(QFileInfo(path).fileName()), true);
            continue;
        }
        torrents.append({QFileInfo(path).fileName(), file.readAll()});
    }
    addTorrentData(torrents);
}

void Backend::addTorrentData(const QList<QPair<QString, QByteArray>> &torrents) {
    if (torrents.isEmpty())
        return;
    m_client->addTorrentFiles(torrents, {}, [this, count = torrents.size()](const QbtClient::Reply &reply) {
        if (reply.ok && reply.body.trimmed() != "Fails.")
            emit notify(tr("Added %n torrent(s)", "", count), false);
        else
            emit notify(tr("The engine refused the torrent file."), true);
    });
}

bool Backend::pasteFromClipboard() {
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    static const QRegularExpression magnet(QStringLiteral(R"(magnet:\?[^\s"'<>]+)"),
                                           QRegularExpression::CaseInsensitiveOption);
    QStringList found;
    auto it = magnet.globalMatch(text);
    while (it.hasNext())
        found.append(it.next().captured(0));
    if (found.isEmpty() && (text.startsWith("http", Qt::CaseInsensitive) && !text.contains(QRegularExpression("\\s"))))
        found.append(text);
    if (found.isEmpty())
        return false;
    addSources(found);
    return true;
}

void Backend::openTorrentFiles() { m_picker->openTorrents(); }
void Backend::chooseDownloadFolder() { m_picker->chooseFolder(downloadFolder()); }

// --- Acting on torrents -----------------------------------------------------

void Backend::toggle(const QString &hash) {
    const QString state = m_torrents.torrent(hash).value("state").toString();
    const bool stopped = state.startsWith("stopped") || state.startsWith("paused");
    Form form;
    form.addQueryItem("hashes", hash);
    m_client->post(stopped ? "torrents/start" : "torrents/stop", form, [this](const QbtClient::Reply &reply) {
        report(reply, tr("Could not change the download"));
    });
}

void Backend::remove(const QString &hash, bool deleteFiles) {
    Form form;
    form.addQueryItem("hashes", hash);
    form.addQueryItem("deleteFiles", deleteFiles ? "true" : "false");
    m_client->post("torrents/delete", form, [this](const QbtClient::Reply &reply) {
        report(reply, tr("Could not remove the download"));
    });
}

void Backend::openFolder(const QString &hash) {
    const QJsonObject t = m_torrents.torrent(hash);
    // The content path is the file itself for single-file torrents; open the
    // folder that holds it so the file manager shows it among its neighbours.
    QString path = t.value("content_path").toString();
    if (!QFileInfo(path).isDir())
        path = QFileInfo(path).absolutePath();
    if (!QFileInfo::exists(path))
        path = t.value("save_path").toString();
    if (!QFileInfo::exists(path)) {
        emit notify(tr("The folder doesn't exist yet."), true);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void Backend::setCategory(const QString &hash, const QString &category) {
    Form form;
    form.addQueryItem("hashes", hash);
    form.addQueryItem("category", category);
    m_client->post("torrents/setCategory", form, [this](const QbtClient::Reply &reply) {
        report(reply, tr("Could not change the category"));
    });
}

void Backend::setTags(const QString &hash, const QString &commaSeparated) {
    QStringList wanted;
    for (const QString &tag : commaSeparated.split(',', Qt::SkipEmptyParts))
        if (!tag.trimmed().isEmpty() && !wanted.contains(tag.trimmed()))
            wanted.append(tag.trimmed());

    QStringList current;
    for (const QString &tag : m_torrents.torrent(hash).value("tags").toString().split(',', Qt::SkipEmptyParts))
        current.append(tag.trimmed());

    QStringList removed;
    for (const QString &tag : current)
        if (!wanted.contains(tag))
            removed.append(tag);
    if (!removed.isEmpty()) {
        Form form;
        form.addQueryItem("hashes", hash);
        form.addQueryItem("tags", removed.join(','));
        m_client->post("torrents/removeTags", form);
    }
    if (!wanted.isEmpty()) {
        Form form;
        form.addQueryItem("hashes", hash);
        form.addQueryItem("tags", wanted.join(','));
        m_client->post("torrents/addTags", form, [this](const QbtClient::Reply &reply) {
            report(reply, tr("Could not change the tags"));
        });
    }
}

// --- Search -----------------------------------------------------------------

void Backend::search(const QString &pattern) {
    const QString trimmed = pattern.trimmed();
    if (trimmed.isEmpty())
        return;
    stopSearch();
    m_results.clear();
    m_searchPattern = trimmed;
    m_searchOffset = 0;
    m_searchRunning = true;
    emit searchChanged();

    Form form;
    form.addQueryItem("pattern", trimmed);
    form.addQueryItem("plugins", "enabled");
    form.addQueryItem("category", "all");
    m_client->post("search/start", form, [this](const QbtClient::Reply &reply) {
        if (!reply.ok) {
            m_searchRunning = false;
            emit searchChanged();
            emit notify(reply.status == 409 ? tr("Too many searches are running; wait a moment.")
                                            : tr("The search could not start: %1").arg(reply.error),
                        true);
            return;
        }
        m_searchId = reply.json().object().value("id").toInt(-1);
        emit searchChanged();
        m_searchTimer.start();
    });
}

void Backend::pollSearch() {
    if (m_searchId < 0) {
        m_searchTimer.stop();
        return;
    }
    QUrlQuery query;
    query.addQueryItem("id", QString::number(m_searchId));
    query.addQueryItem("offset", QString::number(m_searchOffset));
    const int id = m_searchId;
    m_client->get("search/results", query, [this, id](const QbtClient::Reply &reply) {
        if (id != m_searchId || !reply.ok)
            return;
        const QJsonObject data = reply.json().object();
        const QJsonArray batch = data.value("results").toArray();
        m_searchOffset += batch.size();
        m_results.append(batch);
        if (data.value("status").toString() == QLatin1String("Stopped") && batch.isEmpty()) {
            m_searchTimer.stop();
            m_searchRunning = false;
            emit searchChanged();
        }
    });
}

void Backend::stopSearch() {
    m_searchTimer.stop();
    if (m_searchId >= 0) {
        Form form;
        form.addQueryItem("id", QString::number(m_searchId));
        m_client->post("search/stop", form, [this, form](const QbtClient::Reply &) {
            m_client->post("search/delete", form);
        });
    }
    m_searchId = -1;
    m_searchRunning = false;
    emit searchChanged();
}

void Backend::download(int row) {
    const QJsonObject result = m_results.result(row);
    const QString link = result.value("fileUrl").toString();
    // Results come from third-party search plugins: a "file:" link or a
    // local path would make Torrente read a file from this computer.
    if (!isMagnetOrLink(link))
        return;
    addSources({link});
}

void Backend::openResultPage(int row) {
    const QUrl page(m_results.result(row).value("descrLink").toString());
    if (page.isValid() && (page.scheme() == QLatin1String("http") || page.scheme() == QLatin1String("https")))
        QDesktopServices::openUrl(page);
}

QVariantList Backend::officialPlugins() const {
    QVariantList list;
    for (const QString &name : kOfficialPlugins)
        list.append(QVariantMap{{"name", name}});
    return list;
}

void Backend::refreshPlugins() {
    m_client->get("search/plugins", {}, [this](const QbtClient::Reply &reply) {
        if (!reply.ok)
            return;
        m_plugins.clear();
        for (const QJsonValue &value : reply.json().array()) {
            const QJsonObject plugin = value.toObject();
            m_plugins.append(QVariantMap{{"name", plugin.value("name").toString()},
                                         {"fullName", plugin.value("fullName").toString()},
                                         {"enabled", plugin.value("enabled").toBool()},
                                         {"url", plugin.value("url").toString()}});
        }
        emit pluginsChanged();
    });
}

void Backend::installPlugins(const QStringList &names) {
    QStringList sources;
    for (const QString &name : names)
        if (kOfficialPlugins.contains(name))
            sources.append(QLatin1String(kPluginBase) + name + QStringLiteral(".py"));
    if (sources.isEmpty())
        return;
    m_installingPlugins = true;
    emit pluginsChanged();
    Form form;
    form.addQueryItem("sources", sources.join('|'));
    m_client->post("search/installPlugin", form, [this](const QbtClient::Reply &reply) {
        report(reply, tr("Could not install the search engines"));
        // The engine downloads and checks plugins in the background, so look
        // again a little later rather than reading the list right away.
        QTimer::singleShot(4000, this, [this] {
            m_installingPlugins = false;
            refreshPlugins();
        });
    });
}

void Backend::setPluginEnabled(const QString &name, bool enabled) {
    Form form;
    form.addQueryItem("names", name);
    form.addQueryItem("enable", enabled ? "true" : "false");
    m_client->post("search/enablePlugin", form, [this](const QbtClient::Reply &reply) {
        report(reply, tr("Could not change the search engine"));
        refreshPlugins();
    });
}

// --- Errors -----------------------------------------------------------------

void Backend::report(const QbtClient::Reply &reply, const QString &what) {
    // Connection-wide problems are already on screen as the engine's state.
    if (reply.ok || reply.status == 0 || reply.status == 401 || reply.status == 403)
        return;
    emit notify(reply.error.isEmpty() ? what : QStringLiteral("%1: %2").arg(what, reply.error), true);
}
