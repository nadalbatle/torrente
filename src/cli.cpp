#include "cli.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

#include "qbtclient.h"

namespace Cli {

namespace {
QbtClient::Reply wait(QbtClient &, const std::function<void(QbtClient::Callback)> &call) {
    QEventLoop loop;
    QbtClient::Reply result;
    call([&](const QbtClient::Reply &reply) {
        result = reply;
        loop.quit();
    });
    loop.exec();
    return result;
}

int fail(const QString &message) {
    QTextStream(stderr) << "torrente: " << message << '\n';
    return 1;
}

QString engineProblem(const QbtClient::Reply &reply) {
    if (reply.status == 0)
        return QCoreApplication::translate("Cli", "the engine is not running (systemctl --user start torrente-engine)");
    if (reply.status == 401 || reply.status == 403)
        return QCoreApplication::translate("Cli", "the engine asks for a login; allow localhost without authentication");
    return reply.error;
}

int add(QbtClient &client, const QStringList &sources) {
    if (sources.isEmpty())
        return fail(QCoreApplication::translate("Cli", "nothing to add"));
    QStringList links;
    QList<QPair<QString, QByteArray>> files;
    for (const QString &source : sources) {
        if (QFileInfo::exists(source)) {
            QFile file(source);
            if (!file.open(QIODevice::ReadOnly))
                return fail(QCoreApplication::translate("Cli", "can't read %1").arg(source));
            files.append({QFileInfo(source).fileName(), file.readAll()});
        } else {
            links.append(source);
        }
    }
    if (!links.isEmpty()) {
        Form form;
        form.addQueryItem("urls", links.join('\n'));
        const auto reply = wait(client, [&](QbtClient::Callback done) { client.post("torrents/add", form, done); });
        if (!reply.ok || reply.body.trimmed() == "Fails.")
            return fail(reply.ok ? QCoreApplication::translate("Cli", "the engine refused the link") : engineProblem(reply));
    }
    if (!files.isEmpty()) {
        const auto reply = wait(client, [&](QbtClient::Callback done) { client.addTorrentFiles(files, {}, done); });
        if (!reply.ok || reply.body.trimmed() == "Fails.")
            return fail(reply.ok ? QCoreApplication::translate("Cli", "the engine refused the file") : engineProblem(reply));
    }
    return 0;
}

// The bar plugin ships in the package; the Omarchy shell only loads plugins
// from the user's config, so it is copied there (and copied again to update).
const char *kPluginId = "torrente.panel";

QString pluginSource() {
    const QString overridden = qEnvironmentVariable("TORRENTE_PLUGIN_DIR");
    return overridden.isEmpty() ? QStringLiteral("/usr/share/torrente/omarchy-plugin") : overridden;
}

QString pluginTarget() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/omarchy/plugins/" + kPluginId;
}

bool copyTree(const QString &from, const QString &to) {
    QDir source(from);
    if (!QDir().mkpath(to))
        return false;
    for (const QFileInfo &entry : source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString destination = to + '/' + entry.fileName();
        if (entry.isDir() ? !copyTree(entry.filePath(), destination) : !QFile::copy(entry.filePath(), destination))
            return false;
    }
    return true;
}

int runQuietly(const QString &program, const QStringList &arguments) {
    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!process.waitForFinished(30000))
        return -1;
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

int pluginInstall() {
    const QString source = pluginSource();
    if (!QFileInfo::exists(source + "/manifest.json"))
        return fail(QCoreApplication::translate("Cli", "the bar plugin isn't installed at %1").arg(source));
    const QString target = pluginTarget();
    const QString staging = target + ".new";
    QDir(staging).removeRecursively();
    if (!copyTree(source, staging)) {
        QDir(staging).removeRecursively();
        return fail(QCoreApplication::translate("Cli", "can't copy the plugin to %1").arg(staging));
    }
    // Replace the old copy only once the new one is complete.
    const QFileInfo existing(target);
    if (existing.isSymLink())
        QFile::remove(target);
    else if (existing.exists())
        QDir(target).removeRecursively();
    if (!QDir().rename(staging, target))
        return fail(QCoreApplication::translate("Cli", "can't move the plugin into %1").arg(target));

    runQuietly("omarchy-shell", {"shell", "rescanPlugins"});
    // The shell rescans in the background, so the plugin can take a moment to
    // become known; ask for it a few times before giving up.
    int enabled = -1;
    for (int attempt = 0; attempt < 10 && enabled != 0; ++attempt) {
        if (attempt > 0)
            QThread::msleep(500);
        enabled = runQuietly("omarchy", {"plugin", "enable", kPluginId, "--section", "right"});
    }
    if (enabled != 0)
        return fail(QCoreApplication::translate("Cli", "the plugin is in place, but the bar didn't take it; try: omarchy plugin enable %1").arg(kPluginId));
    QTextStream(stdout) << QCoreApplication::translate("Cli", "Torrente is on the bar.") << '\n';
    return 0;
}

int pluginRemove() {
    runQuietly("omarchy", {"plugin", "disable", kPluginId});
    const QString target = pluginTarget();
    const QFileInfo existing(target);
    if (existing.isSymLink())
        QFile::remove(target);
    else if (existing.exists())
        QDir(target).removeRecursively();
    runQuietly("omarchy-shell", {"shell", "rescanPlugins"});
    QTextStream(stdout) << QCoreApplication::translate("Cli", "Torrente is off the bar.") << '\n';
    return 0;
}

int list(QbtClient &client, bool json) {
    const auto reply = wait(client, [&](QbtClient::Callback done) { client.get("torrents/info", {}, done); });
    if (!reply.ok)
        return fail(engineProblem(reply));
    QTextStream out(stdout);
    const QJsonArray torrents = reply.json().array();
    if (json) {
        QJsonArray rows;
        for (const QJsonValue &value : torrents) {
            const QJsonObject t = value.toObject();
            rows.append(QJsonObject{{"hash", t.value("hash")},         {"name", t.value("name")},
                                    {"state", t.value("state")},       {"progress", t.value("progress")},
                                    {"size", t.value("size")},         {"category", t.value("category")},
                                    {"tags", t.value("tags")},         {"content_path", t.value("content_path")}});
        }
        out << QJsonDocument(rows).toJson(QJsonDocument::Indented);
        return 0;
    }
    for (const QJsonValue &value : torrents) {
        const QJsonObject t = value.toObject();
        out << QString::number(int(t.value("progress").toDouble() * 100)).rightJustified(3) << "%  "
            << t.value("state").toString().leftJustified(12) << ' ' << t.value("name").toString() << '\n';
    }
    return 0;
}
}

bool isCommand(const QStringList &arguments) {
    const QString first = arguments.value(1);
    return first == QLatin1String("add") || first == QLatin1String("list") || first == QLatin1String("plugin") || first == QLatin1String("--help")
        || first == QLatin1String("-h") || first == QLatin1String("help");
}

int run(const QStringList &arguments) {
    const QString command = arguments.value(1);
    QbtClient client(QbtClient::defaultBase());
    if (command == QLatin1String("add"))
        return add(client, arguments.mid(2));
    if (command == QLatin1String("plugin")) {
        const QString action = arguments.value(2);
        if (action == QLatin1String("install"))
            return pluginInstall();
        if (action == QLatin1String("remove"))
            return pluginRemove();
        return fail(QStringLiteral("usage: torrente plugin install|remove"));
    }
    if (command == QLatin1String("list"))
        return list(client, arguments.contains(QStringLiteral("--json")));
    QTextStream(stdout) << "torrente                          open the window\n"
                           "torrente <magnet|url|file>...    open the window and add them\n"
                           "torrente add <magnet|url|file>... add without a window\n"
                           "torrente list [--json]            list torrents\n"
                           "torrente plugin install|remove    put Torrente on the Omarchy bar, or take it off\n"
                           "torrente daemon                   file, tag and announce downloads without a window\n"
                           "                                  (run by torrente-daemon.service)\n";
    return 0;
}

} // namespace Cli
