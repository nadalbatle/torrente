#pragma once

#include <QHash>
#include <QObject>
#include <QSettings>
#include <QTimer>

class Backend;

// `torrente daemon`: Torrente without a window. It runs beside the engine as a
// user service, so whatever gets added (from the browser, the bar plugin, or
// the command line) is filed and tagged even when the window is closed, and a
// finished download is announced with a desktop notification.
class Daemon : public QObject {
    Q_OBJECT

public:
    Daemon(Backend *backend, QSettings *settings, QObject *parent = nullptr);

private slots:
    void onActionInvoked(uint id, const QString &action);
    void onNotificationClosed(uint id, uint reason);

private:
    void announce(const QString &hash);

    Backend *m_backend;
    QSettings *m_settings;
    QTimer m_settingsReload;
    // Folder to open for each notification still on screen.
    QHash<uint, QString> m_folders;
};
