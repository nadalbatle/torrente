// Torrente — downloads torrents, finds them, and files them away, the Omarchy way.

#include <QGuiApplication>
#include <QFileInfo>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "daemon.h"
#include "portalfilepicker.h"
#include "qbtclient.h"
#include "theme.h"
#include "translator.h"

namespace {
// The socket lives in the user's runtime directory, which only they can
// reach, rather than in /tmp where another account could claim the name.
QString serverName() {
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty() && QFileInfo(runtime).isDir())
        return runtime + QStringLiteral("/torrente.sock");
    return QStringLiteral("torrente-") + qEnvironmentVariable("USER");
}

// When Torrente is already open, a magnet link clicked in the browser goes to
// that window instead of opening a second one.
bool handOver(const QStringList &sources) {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(300))
        return false;
    socket.write((sources.isEmpty() ? QStringLiteral("\n") : sources.join('\n')).toUtf8());
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}
}

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    if (arguments.value(1) == QLatin1String("daemon")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QGuiApplication app(argc, argv);
        app.setApplicationName("torrente");
        app.setDesktopFileName("torrente");
        Translator translator;
        app.installTranslator(&translator);
        QSettings settings(QStringLiteral("torrente"), QStringLiteral("torrente"));
        QbtClient client(QbtClient::defaultBase());
        PortalFilePicker picker;
        Backend backend(&client, &picker, &settings);
        Daemon daemon(&backend, &settings);
        return app.exec();
    }

    if (Cli::isCommand(arguments)) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QGuiApplication app(argc, argv);
        Translator translator;
        app.installTranslator(&translator);
        return Cli::run(arguments);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("torrente");
    app.setApplicationVersion("0.3.0");
    app.setDesktopFileName("torrente");
    app.setWindowIcon(QIcon::fromTheme("torrente"));

    const QStringList sources = arguments.mid(1);
    if (handOver(sources))
        return 0;

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    QSettings settings(QStringLiteral("torrente"), QStringLiteral("torrente"));
    QbtClient client(QbtClient::defaultBase());
    PortalFilePicker picker;
    Backend backend(&client, &picker, &settings);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(serverName());
    server.listen(serverName());
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
        QLocalSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QLocalSocket::disconnected, socket, [&backend, window, socket] {
            // A handful of links or paths; anything bigger isn't from Torrente.
            const QStringList received =
                QString::fromUtf8(socket->readAll().left(1024 * 1024)).split('\n', Qt::SkipEmptyParts);
            socket->deleteLater();
            if (!received.isEmpty())
                backend.addSources(received);
            if (window) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        });
    });

    if (!sources.isEmpty())
        backend.addSources(sources);

    return app.exec();
}
