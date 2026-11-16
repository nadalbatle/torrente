#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "classifier.h"
#include "theme.h"
#include "torrentmodel.h"
#include "translator.h"

using Classifier::classify;

class TorrenteTests : public QObject {
    Q_OBJECT

private slots:
    // --- Sorting into folders ---

    void filmGoesToMovies()
    {
        const auto r = classify("Some.Film.2024.1080p.BluRay.x265",
                                {{"Some.Film.2024.1080p.BluRay.x265.mkv", 4000000000}, {"cover.jpg", 200000}});
        QCOMPARE(r.category, QString("Movies"));
    }

    void episodesGoToSeries()
    {
        QCOMPARE(classify("Show.S02E05.720p.WEB", {{"Show.S02E05.720p.WEB.mkv", 900000000}}).category, QString("Series"));
        QCOMPARE(classify("Serie Temporada 1 Completa", {{"cap 01.mp4", 500}, {"cap 02.mp4", 500}}).category,
                 QString("Series"));
    }

    void musicBooksAndSoftware()
    {
        QCOMPARE(classify("Album (2020) [FLAC]", {{"01 - Song.flac", 30000000}, {"cover.jpg", 900000}}).category,
                 QString("Music"));
        QCOMPARE(classify("Some Book", {{"book.epub", 2000000}}).category, QString("Books"));
        QCOMPARE(classify("debian-13.7.0-arm64-netinst.iso", {{"debian-13.7.0-arm64-netinst.iso", 700000000}}).category,
                 QString("Software"));
    }

    void gamesBeatSoftware()
    {
        QCOMPARE(classify("Some Game [FitGirl Repack]", {{"setup.exe", 5000000}, {"data.bin", 900000000}}).category,
                 QString("Games"));
    }

    void unknownIsOtherAndNoFilesIsNothing()
    {
        QCOMPARE(classify("Mystery", {{"thing.xyz", 10}}).category, QString("Other"));
        QVERIFY(classify("magnet only", {}).category.isEmpty());
    }

    // --- Tags ---

    void tagsFromTheReleaseName()
    {
        const auto r = classify("Film.2160p.UHD.HDR.x265.SPANISH.WEB-DL", {});
        QVERIFY(r.tags.contains("4K"));
        QVERIFY(r.tags.contains("HDR"));
        QVERIFY(r.tags.contains("HEVC"));
        QVERIFY(r.tags.contains("ES"));
        QVERIFY(r.tags.contains("WEB"));
        QVERIFY(!r.tags.contains("1080p"));
    }

    void tagsNeedWholeWords()
    {
        // "lat" inside "chocolate" and "web" inside "cobweb" are not tags.
        const auto r = classify("chocolate cobweb", {});
        QVERIFY(r.tags.isEmpty());
    }

    // --- Ordering the list ---

    void groupsSortInOrder()
    {
        TorrentModel model;
        model.apply(maindata({
            {"a", torrent("Done one", "uploading", 10, 50)},
            {"b", torrent("Downloading one", "downloading", 20)},
            {"c", torrent("Broken", "error", 5)},
            {"d", torrent("Stopped one", "stoppedDL", 30)},
            {"e", torrent("Waiting one", "stalledDL", 40)},
        }, true));
        QCOMPARE(names(model), QStringList({"Broken", "Downloading one", "Waiting one", "Stopped one", "Done one"}));
    }

    void newestFirstWithinAGroup()
    {
        TorrentModel model;
        model.apply(maindata({{"a", torrent("Older", "downloading", 10)}, {"b", torrent("Newer", "downloading", 20)}}, true));
        QCOMPARE(names(model), QStringList({"Newer", "Older"}));
    }

    void rowsMoveInsteadOfResetting()
    {
        TorrentModel model;
        model.apply(maindata({{"a", torrent("A", "downloading", 20)}, {"b", torrent("B", "downloading", 10)}}, true));
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy moves(&model, &QAbstractItemModel::rowsMoved);
        // A finishes: it moves below B, as a move.
        model.apply(maindata({{"a", QJsonObject{{"state", "uploading"}, {"completion_on", 99}}}}, false));
        QCOMPARE(names(model), QStringList({"B", "A"}));
        QCOMPARE(resets.count(), 0);
        QCOMPARE(moves.count(), 1);
    }

    void filtersAndCounts()
    {
        TorrentModel model;
        QJsonObject film = torrent("Film", "downloading", 1);
        film["category"] = "Movies";
        film["tags"] = "4K, ES";
        model.apply(maindata({{"a", film}, {"b", torrent("Loose", "downloading", 2)}}, true));
        QCOMPARE(model.categoryCounts().value("Movies").toInt(), 1);
        QCOMPARE(model.categoryCounts().value(TorrentModel::uncategorized()).toInt(), 1);
        QCOMPARE(model.tagCounts().value("ES").toInt(), 1);

        model.setCategoryFilter("Movies");
        QCOMPARE(names(model), QStringList({"Film"}));
        model.setCategoryFilter(TorrentModel::uncategorized());
        QCOMPARE(names(model), QStringList({"Loose"}));
        model.setCategoryFilter("");
        model.setTagFilter("4K");
        QCOMPARE(names(model), QStringList({"Film"}));
        model.setTagFilter("");
        model.setTextFilter("loo");
        QCOMPARE(names(model), QStringList({"Loose"}));
    }

    void removalsLeaveTheList()
    {
        TorrentModel model;
        model.apply(maindata({{"a", torrent("A", "downloading", 1)}}, true));
        QJsonObject gone{{"torrents_removed", QJsonArray{"a"}}};
        model.apply(gone);
        QCOMPARE(model.rowCount(), 0);
    }

    // --- Theme and language (from the Omarchy app template) ---

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#86b85c\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#86b85c"));
    }

    void spanishCatalogLoads()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "es-MX\n");
        Translator translator(dir.path() + "/language", QFINDTESTDATA("../src/i18n"));
        QCOMPARE(translator.translate("Main", "Movies"), QString("Películas"));
        QCOMPARE(translator.translate("Main", "%n result(s)", nullptr, 1), QString("%n resultado"));
        QCOMPARE(translator.translate("Main", "%n result(s)", nullptr, 3), QString("%n resultados"));
    }

private:
    static QJsonObject torrent(const QString &name, const QString &state, double addedOn, double completionOn = 0)
    {
        return {{"name", name}, {"state", state}, {"added_on", addedOn}, {"completion_on", completionOn},
                {"category", ""}, {"tags", ""}};
    }

    static QJsonObject maindata(std::initializer_list<std::pair<const char *, QJsonObject>> torrents, bool full)
    {
        QJsonObject map;
        for (const auto &[hash, t] : torrents)
            map.insert(hash, t);
        return {{"full_update", full}, {"torrents", map}};
    }

    static QStringList names(const TorrentModel &model)
    {
        QStringList list;
        for (int row = 0; row < model.rowCount(); ++row)
            list.append(model.data(model.index(row), TorrentModel::NameRole).toString());
        return list;
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(TorrenteTests)
#include "torrente_tests.moc"
