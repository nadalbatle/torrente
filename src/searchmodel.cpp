#include "searchmodel.h"

#include <QUrl>
#include <algorithm>

SearchModel::SearchModel(QObject *parent) : QAbstractListModel(parent) {}

int SearchModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_results.size();
}

QHash<int, QByteArray> SearchModel::roleNames() const {
    return {{NameRole, "name"}, {SizeRole, "size"}, {SeedsRole, "seeds"}, {PeersRole, "peers"},
            {EngineRole, "engine"}, {LinkRole, "link"}, {PageRole, "page"}};
}

QVariant SearchModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_results.size())
        return {};
    const QJsonObject &r = m_results.at(index.row());
    switch (role) {
    case NameRole: return r.value("fileName").toString();
    case SizeRole: return r.value("fileSize").toDouble();
    case SeedsRole: return r.value("nbSeeders").toInt();
    case PeersRole: return r.value("nbLeechers").toInt();
    case EngineRole: {
        // Newer engines name the plugin; older ones only give the site.
        const QString engine = r.value("engineName").toString();
        return engine.isEmpty() ? QUrl(r.value("siteUrl").toString()).host() : engine;
    }
    case LinkRole: return r.value("fileUrl").toString();
    case PageRole: return r.value("descrLink").toString();
    }
    return {};
}

void SearchModel::append(const QJsonArray &results) {
    if (results.isEmpty())
        return;
    // A new batch can belong anywhere in the seeded order, so the list is
    // rebuilt; searches are short-lived and the view starts at the top.
    beginResetModel();
    for (const QJsonValue &value : results)
        m_results.append(value.toObject());
    std::stable_sort(m_results.begin(), m_results.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value("nbSeeders").toInt() > b.value("nbSeeders").toInt();
    });
    endResetModel();
    emit countChanged();
}

void SearchModel::clear() {
    beginResetModel();
    m_results.clear();
    endResetModel();
    emit countChanged();
}
