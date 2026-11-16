#include "daemon.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QFileInfo>
#include <QJsonObject>
#include <QTextStream>
#include <QUrl>

#include "backend.h"

namespace {
const char *kService = "org.freedesktop.Notifications";
const char *kPath = "/org/freedesktop/Notifications";
const char *kInterface = "org.freedesktop.Notifications";
}

Daemon::Daemon(Backend *backend, QSettings *settings, QObject *parent)
    : QObject(parent), m_backend(backend), m_settings(settings) {
    m_backend->setSyncInterval(2000);

    // The window changes settings (download folder, sorting on or off) in the
    // same file; read it again now and then so the daemon follows along.
    m_settingsReload.setInterval(5000);
    connect(&m_settingsReload, &QTimer::timeout, this, [this] { m_settings->sync(); });
    m_settingsReload.start();

    connect(m_backend, &Backend::finished, this, &Daemon::announce);
    connect(m_backend, &Backend::notify, this, [](const QString &message, bool problem) {
        if (problem)
            QTextStream(stderr) << "torrente: " << message << '\n';
    });
    connect(m_backend, &Backend::engineChanged, this, [this] {
        QTextStream(stderr) << "torrente: engine " << m_backend->engineState() << '\n';
    });

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kService, kPath, kInterface, "ActionInvoked", this, SLOT(onActionInvoked(uint, QString)));
    bus.connect(kService, kPath, kInterface, "NotificationClosed", this, SLOT(onNotificationClosed(uint, uint)));
}

void Daemon::announce(const QString &hash) {
    const QJsonObject torrent = m_backend->torrents()->torrent(hash);
    const QString name = torrent.value("name").toString();
    const QString content = torrent.value("content_path").toString();
    const QString folder = QFileInfo(content).isDir() ? content : torrent.value("save_path").toString();

    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, "Notify");
    // Notification servers may read the body as markup; a torrent's name is
    // whatever its uploader chose, so it goes in escaped.
    message << QStringLiteral("Torrente") << uint(0) << QStringLiteral("torrente")
            << tr("Download finished") << name.toHtmlEscaped()
            << QStringList{QStringLiteral("default"), tr("Open folder")}
            << QVariantMap{{QStringLiteral("desktop-entry"), QStringLiteral("torrente")},
                           {QStringLiteral("category"), QStringLiteral("transfer.complete")}}
            // Omarchy shows a normal notification for 8 seconds unless asked
            // for longer (30 at most); a finished download is often noticed
            // late, so ask for the most. It stays in the history after.
            << int(30000);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, folder](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<uint> reply = *call;
        call->deleteLater();
        if (reply.isError()) {
            QTextStream(stderr) << "torrente: can't show a notification: " << reply.error().message() << '\n';
            return;
        }
        QTextStream(stderr) << "torrente: announced a finished download (notification " << reply.value() << ")\n";
        if (!folder.isEmpty())
            m_folders.insert(reply.value(), folder);
    });
}

void Daemon::onActionInvoked(uint id, const QString &action) {
    Q_UNUSED(action);
    const QString folder = m_folders.take(id);
    if (!folder.isEmpty())
        QTextStream(stderr) << "torrente: opening the folder of notification " << id << '\n';
    if (!folder.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void Daemon::onNotificationClosed(uint id, uint reason) {
    Q_UNUSED(reason);
    m_folders.remove(id);
}
