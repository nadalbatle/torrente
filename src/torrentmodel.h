#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

// The torrents the engine reports, kept in the order Torrente sorts them:
// what needs attention first, then what's downloading, waiting, stopped, and
// finally what's done. Rows move rather than reset when the order changes, so
// the list keeps its place and selection while torrents progress.
class TorrentModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY countsChanged)
    Q_PROPERTY(QVariantMap categoryCounts READ categoryCounts NOTIFY countsChanged)
    Q_PROPERTY(QVariantMap tagCounts READ tagCounts NOTIFY countsChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY filterChanged)
    Q_PROPERTY(QString tagFilter READ tagFilter WRITE setTagFilter NOTIFY filterChanged)
    Q_PROPERTY(QString textFilter READ textFilter WRITE setTextFilter NOTIFY filterChanged)

public:
    enum Role {
        HashRole = Qt::UserRole + 1,
        NameRole,
        StateRole,
        GroupRole,
        ProgressRole,
        SizeRole,
        DownloadedRole,
        DownloadSpeedRole,
        UploadSpeedRole,
        EtaRole,
        SeedsRole,
        PeersRole,
        RatioRole,
        CategoryRole,
        TagsRole,
        SavePathRole,
        ContentPathRole,
        AddedOnRole,
    };

    // Groups, in display order. Their names are ids; QML translates them.
    static QString groupOf(const QString &state);
    static int groupRank(const QString &group);

    explicit TorrentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Applies a sync/maindata response: full or partial torrents, and removals.
    void apply(const QJsonObject &maindata);
    void clear();

    int total() const { return m_torrents.size(); }
    QVariantMap categoryCounts() const;
    QVariantMap tagCounts() const;
    QJsonObject torrent(const QString &hash) const { return m_torrents.value(hash); }
    QStringList hashes() const { return m_torrents.keys(); }

    QString categoryFilter() const { return m_categoryFilter; }
    QString tagFilter() const { return m_tagFilter; }
    QString textFilter() const { return m_textFilter; }
    void setCategoryFilter(const QString &value);
    void setTagFilter(const QString &value);
    void setTextFilter(const QString &value);

    Q_INVOKABLE int indexOf(const QString &hash) const { return m_order.indexOf(hash); }
    Q_INVOKABLE QString hashAt(int row) const { return m_order.value(row); }

    // The id the uncategorized filter uses.
    static QString uncategorized() { return QStringLiteral("__none__"); }

signals:
    void countChanged();
    void countsChanged();
    void filterChanged();

private:
    QStringList desiredOrder() const;
    bool accepts(const QJsonObject &torrent) const;
    void reorder();

    QHash<QString, QJsonObject> m_torrents;
    QStringList m_order;
    QString m_categoryFilter;
    QString m_tagFilter;
    QString m_textFilter;
};
