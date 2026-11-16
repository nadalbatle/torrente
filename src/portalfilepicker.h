#pragma once

#include <QVariantMap>

#include "filepicker.h"

class QDBusPendingCallWatcher;

// xdg-desktop-portal FileChooser over D-Bus, so the dialog is the desktop's
// own. Follows Omacut's src/portalfilepicker.h (MIT, David Heinemeier Hansson).
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void openTorrents() override;
    void chooseFolder(const QString &current) override;

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    enum class Action { None, OpenTorrents, ChooseFolder };

    void requestFile(const QString &title, QVariantMap options, Action action);
    bool connectToRequestPath(const QString &path);
    void clearPending();
    void closePendingRequest();

    QString m_pendingPath;
    Action m_pendingAction = Action::None;
    int m_pendingGeneration = 0;
    QDBusPendingCallWatcher *m_pendingWatcher = nullptr;
};
