#pragma once

#include <QList>
#include <QObject>
#include <QUrl>

// Open and folder dialogs, behind an interface so tests can inject a fake.
// Follows Omacut's src/filepicker.h (MIT, David Heinemeier Hansson).
class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    virtual void openTorrents() = 0;
    virtual void chooseFolder(const QString &current) = 0;

signals:
    void torrentsSelected(const QList<QUrl> &urls);
    void folderSelected(const QUrl &url);
    void failed(const QString &message);
};
