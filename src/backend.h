#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

#include "qbtclient.h"
#include "searchmodel.h"
#include "torrentmodel.h"

class FilePicker;

// Torrente's state and work. The engine (qbittorrent-nox) does the
// downloading; the backend keeps the window's view of it current, sorts new
// torrents into folders and tags them, and runs searches through the engine's
// search plugins.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString engineState READ engineState NOTIFY engineChanged)
    Q_PROPERTY(QString engineError READ engineError NOTIFY engineChanged)
    Q_PROPERTY(bool startingEngine READ startingEngine NOTIFY engineChanged)
    Q_PROPERTY(double downloadSpeed READ downloadSpeed NOTIFY statsChanged)
    Q_PROPERTY(double uploadSpeed READ uploadSpeed NOTIFY statsChanged)
    Q_PROPERTY(double freeSpace READ freeSpace NOTIFY statsChanged)
    Q_PROPERTY(TorrentModel *torrents READ torrents CONSTANT)
    Q_PROPERTY(SearchModel *results READ results CONSTANT)
    Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(QString searchPattern READ searchPattern NOTIFY searchChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QVariantList officialPlugins READ officialPlugins CONSTANT)
    Q_PROPERTY(bool installingPlugins READ installingPlugins NOTIFY pluginsChanged)
    Q_PROPERTY(QString downloadFolder READ downloadFolder NOTIFY settingsChanged)
    Q_PROPERTY(bool autoFolders READ autoFolders WRITE setAutoFolders NOTIFY settingsChanged)
    Q_PROPERTY(bool autoTags READ autoTags WRITE setAutoTags NOTIFY settingsChanged)

public:
    explicit Backend(QbtClient *client, FilePicker *picker, QSettings *settings, QObject *parent = nullptr);

    QString engineState() const { return m_engineState; }
    QString engineError() const { return m_engineError; }
    bool startingEngine() const { return m_engineStarter.state() != QProcess::NotRunning; }
    double downloadSpeed() const { return m_serverState.value("dl_info_speed").toDouble(); }
    double uploadSpeed() const { return m_serverState.value("up_info_speed").toDouble(); }
    double freeSpace() const { return m_serverState.value("free_space_on_disk").toDouble(-1); }
    TorrentModel *torrents() { return &m_torrents; }
    SearchModel *results() { return &m_results; }
    QStringList categories() const;
    bool searching() const { return m_searchId >= 0 && m_searchRunning; }
    QString searchPattern() const { return m_searchPattern; }
    QVariantList plugins() const { return m_plugins; }
    QVariantList officialPlugins() const;
    bool installingPlugins() const { return m_installingPlugins; }
    QString downloadFolder() const;
    bool autoFolders() const;
    bool autoTags() const;
    void setAutoFolders(bool enabled);
    void setAutoTags(bool enabled);

    // Adds magnets, URLs and .torrent files, whatever the user handed over.
    Q_INVOKABLE void addSources(const QStringList &sources);
    // Adds the magnet or .torrent link on the clipboard; false when there is none.
    Q_INVOKABLE bool pasteFromClipboard();
    Q_INVOKABLE void openTorrentFiles();
    Q_INVOKABLE void chooseDownloadFolder();

    Q_INVOKABLE void toggle(const QString &hash);
    Q_INVOKABLE void remove(const QString &hash, bool deleteFiles);
    Q_INVOKABLE void openFolder(const QString &hash);
    Q_INVOKABLE void setCategory(const QString &hash, const QString &category);
    Q_INVOKABLE void setTags(const QString &hash, const QString &commaSeparated);
    // Sorts and tags this torrent again, as if it had just been added.
    Q_INVOKABLE void reclassify(const QString &hash);

    Q_INVOKABLE void search(const QString &pattern);
    Q_INVOKABLE void stopSearch();
    Q_INVOKABLE void download(int row);
    Q_INVOKABLE void openResultPage(int row);
    Q_INVOKABLE void installPlugins(const QStringList &names);
    Q_INVOKABLE void setPluginEnabled(const QString &name, bool enabled);

    Q_INVOKABLE void startEngine();

    // How often the engine is asked for changes while it answers. The window
    // wants every second; the background daemon is happy with less.
    void setSyncInterval(int milliseconds) { m_syncInterval = milliseconds; m_syncTimer.setInterval(milliseconds); }

    // The folder name a Torrente category gets, in the current language.
    static QString categoryLabel(const QString &id);

signals:
    void engineChanged();
    void statsChanged();
    void categoriesChanged();
    void searchChanged();
    void pluginsChanged();
    void settingsChanged();
    // A short message for the window to show, and whether it is a problem.
    void notify(const QString &message, bool problem);
    // A torrent finished downloading while Torrente was watching it.
    void finished(const QString &hash);

private:
    void sync();
    void setEngineState(const QString &state, const QString &error = {});
    void onReady();
    void hardenEngine();
    void trackCompletion(const QJsonObject &data);
    void applyAutoSortPreferences();
    void ensureCategories();
    QString categoryName(const QString &id) const;
    void classifyNew();
    void classify(const QString &hash);
    void pollSearch();
    void refreshPlugins();
    void addLocalFiles(const QStringList &paths);
    void addTorrentData(const QList<QPair<QString, QByteArray>> &torrents);
    void fetchTorrentLink(const QString &link);
    void addLinks(const QStringList &links);
    void watchDownloads();
    void checkDownloads();
    void report(const QbtClient::Reply &reply, const QString &what);

    QbtClient *m_client;
    // Fetches .torrent links here rather than in the engine, which drops a
    // refused download silently; this way the window can say why.
    QNetworkAccessManager m_web;
    // A site that wants a login gets the link opened in the browser, which
    // has the session; the .torrent it saves to Downloads is picked up here.
    QFileSystemWatcher m_downloadsWatcher;
    QTimer m_downloadsDeadline;
    QSet<QString> m_downloadsSeen;
    FilePicker *m_picker;
    QSettings *m_settings;

    QString m_engineState = QStringLiteral("connecting");
    QString m_engineError;
    QTimer m_syncTimer;
    bool m_syncInFlight = false;
    qint64 m_rid = 0;
    QJsonObject m_serverState;
    QHash<QString, QJsonObject> m_categories;
    QProcess m_engineStarter;

    TorrentModel m_torrents;
    // Torrents already sorted this session, or being sorted now.
    QSet<QString> m_classified;
    // Last known progress per torrent, to notice the moment one completes.
    QHash<QString, double> m_progress;
    int m_syncInterval = 1000;

    SearchModel m_results;
    QTimer m_searchTimer;
    int m_searchId = -1;
    int m_searchOffset = 0;
    bool m_searchRunning = false;
    QString m_searchPattern;
    QVariantList m_plugins;
    bool m_installingPlugins = false;
};
