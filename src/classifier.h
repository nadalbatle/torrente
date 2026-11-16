#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Decides where a torrent belongs and what to tag it with, from its name and
// its files alone. Pure functions, so the rules are easy to test and to read.
namespace Classifier {

struct File {
    QString name;
    qint64 size = 0;
};

struct Result {
    // One of categoryIds(), or empty when there is nothing to go on yet
    // (a magnet whose file list hasn't arrived).
    QString category;
    QStringList tags;
};

// The categories Torrente creates, by stable id. The folder each one gets is
// named in the user's language when it is created (see Backend).
QStringList categoryIds();

Result classify(const QString &name, const QList<File> &files);

} // namespace Classifier
