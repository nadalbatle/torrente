#pragma once

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QUrlQuery>
#include <functional>

// A form for the engine's POST calls. Unlike QUrlQuery, which reads its values
// as already percent-encoded (so "C++" reaches the engine as "C  " and a
// magnet's own "%26" turns into a stray "&"), values are kept exactly as given
// and encoded once, when the form is sent.
class Form {
public:
    void addQueryItem(const QString &key, const QString &value) { m_items.append({key, value}); }
    const QList<QPair<QString, QString>> &items() const { return m_items; }
    QByteArray encoded() const;

private:
    QList<QPair<QString, QString>> m_items;
};

// A thin client for qBittorrent's Web API (v2), which the engine serves on
// localhost. Every call answers through a callback; failures that concern the
// whole connection (the engine is down, or it wants a login) are also
// reported as signals, so the window can say what's wrong once.
class QbtClient : public QObject {
    Q_OBJECT

public:
    struct Reply {
        bool ok = false;
        int status = 0;
        QByteArray body;
        QString error;
        QJsonDocument json() const { return QJsonDocument::fromJson(body); }
    };
    using Callback = std::function<void(const Reply &)>;

    explicit QbtClient(const QUrl &base, QObject *parent = nullptr);

    QUrl base() const { return m_base; }

    void get(const QString &path, const QUrlQuery &query, Callback done = {});
    void post(const QString &path, const Form &form, Callback done = {});
    // Uploads .torrent files to torrents/add, with the other fields beside them.
    void addTorrentFiles(const QList<QPair<QString, QByteArray>> &files, const Form &fields,
                         Callback done = {});

    // The engine's address: TORRENTE_ENGINE_URL, or qBittorrent's default.
    static QUrl defaultBase();

signals:
    void unreachable(const QString &message);
    void forbidden();
    void reachable();

private:
    void finish(class QNetworkReply *reply, Callback done);

    QUrl m_base;
    QNetworkAccessManager m_network;
};
