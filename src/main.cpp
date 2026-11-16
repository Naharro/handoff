// handoff — send folders to another laptop over the local network, with
// rsync over SSH, and get this laptop ready to receive them.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "portalfilepicker.h"
#include "receiver.h"
#include "theme.h"
#include "translator.h"

static const char *version = "0.1.0";

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // ssh calls the binary back to ask for the password. The marker keeps a
    // file that happens to be called "askpass" from being read as that.
    if (qEnvironmentVariableIsSet("HANDOFF_ASKPASS") && qEnvironmentVariableIsSet("SSH_ASKPASS"))
        return Cli::askpass(arguments);

    if (Cli::handles(arguments.value(0))) {
        // No window is shown, so no display is needed either.
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QGuiApplication app(argc, argv);
        app.setApplicationName("handoff");
        app.setApplicationVersion(version);
        return Cli::run(arguments);
    }
    if (arguments.value(0) == "open")
        arguments.removeFirst();

    QGuiApplication app(argc, argv);
    app.setApplicationName("handoff");
    app.setApplicationVersion(version);

    // Matches the window to handoff.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("handoff");
    app.setWindowIcon(QIcon::fromTheme("handoff"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);
    backend.setNotifies(true);
    backend.addPaths(arguments);
    Receiver receiver;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("receiver", &receiver);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
