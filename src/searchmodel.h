#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

// Search results as the engine finds them, best-seeded first. Results arrive
// in batches while the search runs; each batch is merged into the order.
class SearchModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        SizeRole,
        SeedsRole,
        PeersRole,
        EngineRole,
        LinkRole,
        PageRole,
    };

    explicit SearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const QJsonArray &results);
    void clear();
    QJsonObject result(int row) const { return m_results.value(row); }

signals:
    void countChanged();

private:
    QList<QJsonObject> m_results;
};
