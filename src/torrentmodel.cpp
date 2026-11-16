#include "torrentmodel.h"

#include <QJsonArray>
#include <algorithm>

TorrentModel::TorrentModel(QObject *parent) : QAbstractListModel(parent) {}

QString TorrentModel::groupOf(const QString &state) {
    static const QHash<QString, QString> groups = {
        {"error", "attention"},          {"missingFiles", "attention"},
        {"downloading", "downloading"},  {"forcedDL", "downloading"},   {"metaDL", "downloading"},
        {"forcedMetaDL", "downloading"}, {"checkingDL", "downloading"}, {"allocating", "downloading"},
        {"moving", "downloading"},       {"checkingResumeData", "downloading"},
        {"stalledDL", "waiting"},        {"queuedDL", "waiting"},
        {"stoppedDL", "stopped"},        {"pausedDL", "stopped"},
        {"uploading", "done"},           {"forcedUP", "done"},          {"stalledUP", "done"},
        {"queuedUP", "done"},            {"checkingUP", "done"},        {"stoppedUP", "done"},
        {"pausedUP", "done"},
    };
    return groups.value(state, QStringLiteral("waiting"));
}

int TorrentModel::groupRank(const QString &group) {
    static const QStringList order = {"attention", "downloading", "waiting", "stopped", "done"};
    const int rank = order.indexOf(group);
    return rank < 0 ? order.size() : rank;
}

int TorrentModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_order.size();
}

QHash<int, QByteArray> TorrentModel::roleNames() const {
    return {
        {HashRole, "hash"},           {NameRole, "name"},
        {StateRole, "state"},         {GroupRole, "group"},
        {ProgressRole, "progress"},   {SizeRole, "size"},
        {DownloadedRole, "downloaded"}, {DownloadSpeedRole, "downloadSpeed"},
        {UploadSpeedRole, "uploadSpeed"}, {EtaRole, "eta"},
        {SeedsRole, "seeds"},         {PeersRole, "peers"},
        {RatioRole, "ratio"},         {CategoryRole, "category"},
        {TagsRole, "tags"},           {SavePathRole, "savePath"},
        {ContentPathRole, "contentPath"}, {AddedOnRole, "addedOn"},
    };
}

namespace {
QStringList tagsOf(const QJsonObject &torrent) {
    QStringList tags;
    for (const QString &tag : torrent.value("tags").toString().split(',', Qt::SkipEmptyParts))
        tags.append(tag.trimmed());
    return tags;
}
}

QVariant TorrentModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_order.size())
        return {};
    const QString hash = m_order.at(index.row());
    const QJsonObject t = m_torrents.value(hash);
    switch (role) {
    case HashRole: return hash;
    case NameRole: return t.value("name").toString();
    case StateRole: return t.value("state").toString();
    case GroupRole: return groupOf(t.value("state").toString());
    case ProgressRole: return t.value("progress").toDouble();
    // Size counts only the files chosen for download; total_size is all of them.
    case SizeRole: return t.value("size").toDouble(t.value("total_size").toDouble());
    case DownloadedRole: return t.value("completed").toDouble();
    case DownloadSpeedRole: return t.value("dlspeed").toDouble();
    case UploadSpeedRole: return t.value("upspeed").toDouble();
    case EtaRole: return t.value("eta").toDouble();
    case SeedsRole: return t.value("num_seeds").toInt();
    case PeersRole: return t.value("num_leechs").toInt();
    case RatioRole: return t.value("ratio").toDouble();
    case CategoryRole: return t.value("category").toString();
    case TagsRole: return tagsOf(t);
    case SavePathRole: return t.value("save_path").toString();
    case ContentPathRole: return t.value("content_path").toString();
    case AddedOnRole: return t.value("added_on").toDouble();
    }
    return {};
}

void TorrentModel::apply(const QJsonObject &maindata) {
    if (maindata.value("full_update").toBool())
        m_torrents.clear();

    const QJsonObject changed = maindata.value("torrents").toObject();
    for (auto it = changed.begin(); it != changed.end(); ++it) {
        // Partial updates carry only the fields that changed.
        QJsonObject merged = m_torrents.value(it.key());
        const QJsonObject fields = it.value().toObject();
        for (auto field = fields.begin(); field != fields.end(); ++field)
            merged.insert(field.key(), field.value());
        m_torrents.insert(it.key(), merged);
    }
    for (const QJsonValue &removed : maindata.value("torrents_removed").toArray())
        m_torrents.remove(removed.toString());

    reorder();
    emit countsChanged();
}

void TorrentModel::clear() {
    m_torrents.clear();
    reorder();
    emit countsChanged();
}

bool TorrentModel::accepts(const QJsonObject &torrent) const {
    const QString category = torrent.value("category").toString();
    if (m_categoryFilter == uncategorized() ? !category.isEmpty()
                                            : !m_categoryFilter.isEmpty() && category != m_categoryFilter)
        return false;
    if (!m_tagFilter.isEmpty() && !tagsOf(torrent).contains(m_tagFilter))
        return false;
    if (!m_textFilter.isEmpty() && !torrent.value("name").toString().contains(m_textFilter, Qt::CaseInsensitive))
        return false;
    return true;
}

QStringList TorrentModel::desiredOrder() const {
    QStringList order;
    for (auto it = m_torrents.cbegin(); it != m_torrents.cend(); ++it)
        if (accepts(it.value()))
            order.append(it.key());

    std::stable_sort(order.begin(), order.end(), [this](const QString &a, const QString &b) {
        const QJsonObject &x = m_torrents[a], &y = m_torrents[b];
        const QString gx = groupOf(x.value("state").toString()), gy = groupOf(y.value("state").toString());
        if (gx != gy)
            return groupRank(gx) < groupRank(gy);
        // Finished torrents by when they finished, everything else by when it
        // was added: newest first either way.
        const char *key = gx == QLatin1String("done") ? "completion_on" : "added_on";
        const double tx = x.value(key).toDouble(), ty = y.value(key).toDouble();
        if (tx != ty)
            return tx > ty;
        return a < b;
    });
    return order;
}

void TorrentModel::reorder() {
    const QStringList desired = desiredOrder();
    const int before = m_order.size();

    // Rows that left the list.
    for (int row = m_order.size() - 1; row >= 0; --row) {
        if (!desired.contains(m_order.at(row))) {
            beginRemoveRows({}, row, row);
            m_order.removeAt(row);
            endRemoveRows();
        }
    }
    // Rows that joined it, appended and then moved into place below.
    for (const QString &hash : desired) {
        if (!m_order.contains(hash)) {
            beginInsertRows({}, m_order.size(), m_order.size());
            m_order.append(hash);
            endInsertRows();
        }
    }
    // Moves, one row at a time, so views animate and keep their selection.
    for (int target = 0; target < desired.size(); ++target) {
        if (m_order.at(target) == desired.at(target))
            continue;
        const int source = m_order.indexOf(desired.at(target), target + 1);
        beginMoveRows({}, source, source, {}, target);
        m_order.move(source, target);
        endMoveRows();
    }

    if (!m_order.isEmpty())
        emit dataChanged(index(0), index(m_order.size() - 1));
    if (before != m_order.size())
        emit countChanged();
}

QVariantMap TorrentModel::categoryCounts() const {
    QVariantMap counts;
    for (const QJsonObject &torrent : m_torrents) {
        const QString category = torrent.value("category").toString();
        const QString key = category.isEmpty() ? uncategorized() : category;
        counts[key] = counts.value(key).toInt() + 1;
    }
    return counts;
}

QVariantMap TorrentModel::tagCounts() const {
    QVariantMap counts;
    for (const QJsonObject &torrent : m_torrents)
        for (const QString &tag : tagsOf(torrent))
            counts[tag] = counts.value(tag).toInt() + 1;
    return counts;
}

void TorrentModel::setCategoryFilter(const QString &value) {
    if (value == m_categoryFilter)
        return;
    m_categoryFilter = value;
    reorder();
    emit filterChanged();
}

void TorrentModel::setTagFilter(const QString &value) {
    if (value == m_tagFilter)
        return;
    m_tagFilter = value;
    reorder();
    emit filterChanged();
}

void TorrentModel::setTextFilter(const QString &value) {
    if (value == m_textFilter)
        return;
    m_textFilter = value;
    reorder();
    emit filterChanged();
}
