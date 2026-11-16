#include "qbtclient.h"

#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

QByteArray Form::encoded() const {
    QByteArray body;
    for (const auto &[key, value] : m_items) {
        if (!body.isEmpty())
            body += '&';
        body += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
    }
    return body;
}

QbtClient::QbtClient(const QUrl &base, QObject *parent) : QObject(parent), m_base(base) {
    // The engine is local, so a call that takes this long means it's stuck.
    m_network.setTransferTimeout(15000);
}

QUrl QbtClient::defaultBase() {
    const QString configured = qEnvironmentVariable("TORRENTE_ENGINE_URL");
    return QUrl(configured.isEmpty() ? QStringLiteral("http://127.0.0.1:8080") : configured);
}

namespace {
QNetworkRequest request(const QUrl &base, const QString &path, const QUrlQuery &query = {}) {
    QUrl url = base;
    url.setPath(QStringLiteral("/api/v2/") + path);
    url.setQuery(query);
    QNetworkRequest request(url);
    // qBittorrent refuses API calls whose Referer names another host.
    request.setRawHeader("Referer", base.toString(QUrl::RemovePath).toUtf8());
    return request;
}
}

void QbtClient::get(const QString &path, const QUrlQuery &query, Callback done) {
    finish(m_network.get(request(m_base, path, query)), std::move(done));
}

void QbtClient::post(const QString &path, const Form &form, Callback done) {
    QNetworkRequest req = request(m_base, path);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    finish(m_network.post(req, form.encoded()), std::move(done));
}

void QbtClient::addTorrentFiles(const QList<QPair<QString, QByteArray>> &files, const Form &fields,
                                Callback done) {
    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    for (const auto &[name, data] : files) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-bittorrent"));
        // A file name is whatever is on disk; quotes or line breaks in it
        // would end the header early, so they don't go into it.
        QString safeName = name;
        safeName.replace(QRegularExpression(QStringLiteral(R"(["\\\r\n])")), QStringLiteral("_"));
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"torrents\"; filename=\"%1\"").arg(safeName));
        part.setBody(data);
        multi->append(part);
    }
    for (const auto &[key, value] : fields.items()) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"%1\"").arg(key));
        part.setBody(value.toUtf8());
        multi->append(part);
    }
    QNetworkReply *reply = m_network.post(request(m_base, QStringLiteral("torrents/add")), multi);
    multi->setParent(reply);
    finish(reply, std::move(done));
}

void QbtClient::finish(QNetworkReply *reply, Callback done) {
    connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done)] {
        reply->deleteLater();
        Reply result;
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll();
        result.ok = reply->error() == QNetworkReply::NoError && result.status >= 200 && result.status < 300;
        if (!result.ok)
            result.error = result.status ? QString::fromUtf8(result.body).trimmed() : reply->errorString();

        if (result.status == 0 && reply->error() != QNetworkReply::NoError)
            emit unreachable(reply->errorString());
        else if (result.status == 403 || result.status == 401)
            emit forbidden();
        else
            emit reachable();

        if (done)
            done(result);
    });
}
