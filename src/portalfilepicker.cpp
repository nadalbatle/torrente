// Follows Omacut's src/portalfilepicker.cpp (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {
struct PortalFilterRule {
    uint type;
    QString pattern;
};
using PortalFilterRules = QList<PortalFilterRule>;

struct PortalFileFilter {
    QString name;
    PortalFilterRules rules;
};
using PortalFileFilters = QList<PortalFileFilter>;

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule) {
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule) {
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFileFilter &filter) {
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFileFilter &filter) {
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

void registerPortalTypes() {
    static const bool registered = [] {
        qDBusRegisterMetaType<PortalFilterRule>();
        qDBusRegisterMetaType<PortalFilterRules>();
        qDBusRegisterMetaType<PortalFileFilter>();
        qDBusRegisterMetaType<PortalFileFilters>();
        return true;
    }();
    Q_UNUSED(registered);
}

PortalFileFilter torrentFilter() {
    return {QCoreApplication::translate("PortalFilePicker", "Torrent files"),
            {{1, QStringLiteral("application/x-bittorrent")}, {0, QStringLiteral("*.torrent")}}};
}

QString portalToken() {
    return QStringLiteral("torrente_%1").arg(QRandomGenerator::global()->generate());
}

// The portal takes folders as NUL-terminated byte strings.
QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}
}

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {
    registerPortalTypes();
}

void PortalFilePicker::openTorrents() {
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Add"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), true);
    options.insert(QStringLiteral("current_folder"),
                   portalPathBytes(QDir(downloads).exists() ? downloads : QDir::homePath()));
    options.insert(QStringLiteral("filters"), QVariant::fromValue(PortalFileFilters{torrentFilter()}));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(torrentFilter()));
    requestFile(tr("Add Torrent Files"), options, Action::OpenTorrents);
}

void PortalFilePicker::chooseFolder(const QString &current) {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Choose"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("directory"), true);
    // The portal only opens folders that exist; otherwise start at home.
    const QString start = QDir(current).exists() ? current : QDir::homePath();
    options.insert(QStringLiteral("current_folder"), portalPathBytes(start));
    requestFile(tr("Choose Download Folder"), options, Action::ChooseFolder);
}

bool PortalFilePicker::connectToRequestPath(const QString &path) {
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        this, SLOT(handleResponse(uint,QVariantMap)));
}

void PortalFilePicker::closePendingRequest() {
    if (m_pendingPath.isEmpty())
        return;
    QDBusInterface request(QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                           QStringLiteral("org.freedesktop.portal.Request"), QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}

void PortalFilePicker::requestFile(const QString &title, QVariantMap options, Action action) {
    // A request whose Response never came must not block every later dialog.
    if (m_pendingAction != Action::None) {
        closePendingRequest();
        clearPending();
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"), bus);
    if (!portal.isValid()) {
        emit failed(tr("The XDG desktop portal file chooser is not available."));
        return;
    }

    // Subscribe at the path the portal derives from our token before calling,
    // so the response can't arrive before the match rule is in place.
    const QString token = portalToken();
    options.insert(QStringLiteral("handle_token"), token);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    m_pendingAction = action;
    if (!connectToRequestPath(QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token))) {
        clearPending();
        emit failed(tr("Could not listen for the portal file picker response."));
        return;
    }

    const int generation = ++m_pendingGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(QStringLiteral("OpenFile"), QString(), title, options), this);
    m_pendingWatcher = watcher;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pendingWatcher == watcher)
            m_pendingWatcher = nullptr;
        if (generation != m_pendingGeneration || m_pendingAction == Action::None)
            return;
        if (reply.isError()) {
            clearPending();
            emit failed(tr("The portal file picker failed: %1").arg(reply.error().message()));
            return;
        }
        // Old portals can answer at a different path than the token predicts.
        const QString actualPath = reply.value().path();
        if (actualPath != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint,QVariantMap)));
            if (!connectToRequestPath(actualPath)) {
                clearPending();
                emit failed(tr("Could not listen for the portal file picker response."));
            }
        }
    });
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    const Action action = m_pendingAction;
    clearPending();
    if (response != 0)
        return;

    QList<QUrl> urls;
    for (const QString &uri : results.value(QStringLiteral("uris")).toStringList())
        urls.append(QUrl(uri));
    if (urls.isEmpty())
        return;

    if (action == Action::OpenTorrents)
        emit torrentsSelected(urls);
    else if (action == Action::ChooseFolder)
        emit folderSelected(urls.first());
}

void PortalFilePicker::clearPending() {
    if (m_pendingWatcher) {
        m_pendingWatcher->deleteLater();
        m_pendingWatcher = nullptr;
    }
    if (!m_pendingPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint,QVariantMap)));
    }
    m_pendingPath.clear();
    m_pendingAction = Action::None;
}
