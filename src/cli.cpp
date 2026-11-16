#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTextStream>

#include "receiver.h"
#include "rsync.h"

namespace Cli {

static QTextStream &out() { static QTextStream stream(stdout); return stream; }
static QTextStream &err() { static QTextStream stream(stderr); return stream; }

static const char *usage = R"(Handoff sends folders to another laptop with rsync over SSH.

  handoff [open] [PATH...]        Open the window, with PATHs ready to send
  handoff send --to USER@ADDRESS [--into FOLDER] [--overwrite-newer]
               [--dry-run] [--json] PATH...
                                  Send PATHs. Without --into each lands at the
                                  same place under the other home folder.
  handoff status [--json]         Whether this laptop can receive, and its address
  handoff skill install           Install the agent skill for Handoff
  handoff --version

send logs in with your SSH key, or with the password in HANDOFF_PASSWORD.
)";

bool handles(const QString &command) {
    return QStringList{"send", "status", "skill", "help", "--help", "-h", "--version"}.contains(command);
}

// ssh runs this to ask for the password and reads the answer from stdout.
// Any other question (a key's passphrase, say) is left unanswered.
int askpass(const QStringList &arguments) {
    const QString prompt = arguments.value(0);
    const QByteArray secret = qgetenv("HANDOFF_ASKPASS_SECRET");
    if (secret.isEmpty() || !prompt.contains("assword"))
        return 1;
    fputs(secret.constData(), stdout);
    fputc('\n', stdout);
    return 0;
}

static int fail(bool json, const QString &message) {
    if (json)
        out() << QJsonDocument(QJsonObject{{"ok", false}, {"error", message}}).toJson(QJsonDocument::Compact) << "\n";
    err() << "handoff: " << message << "\n";
    return 1;
}

static int send(QStringList arguments) {
    Rsync::Plan plan;
    plan.home = QDir::homePath();
    bool json = false;
    while (!arguments.isEmpty()) {
        const QString argument = arguments.takeFirst();
        if (argument == "--json") json = true;
        else if (argument == "--dry-run") plan.dryRun = true;
        else if (argument == "--overwrite-newer") plan.keepNewer = false;
        else if (argument == "--to" && !arguments.isEmpty()) plan.target = arguments.takeFirst();
        else if (argument == "--into" && !arguments.isEmpty()) { plan.into = arguments.takeFirst(); plan.mirrorHome = false; }
        else if (argument.startsWith("--")) return fail(json, "unknown option " + argument);
        else {
            const QFileInfo info(argument);
            if (!info.exists())
                return fail(json, argument + " does not exist");
            plan.sources << info.absoluteFilePath();
        }
    }

    const bool password = !qEnvironmentVariableIsEmpty("HANDOFF_PASSWORD");
    plan.batch = !password;
    const auto command = Rsync::build(plan);
    if (!command.error.isEmpty())
        return fail(json, command.error);

    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");
    if (password) {
        environment.insert("SSH_ASKPASS", QCoreApplication::applicationFilePath());
        environment.insert("SSH_ASKPASS_REQUIRE", "force");
        environment.insert("HANDOFF_ASKPASS", "1");
        environment.insert("HANDOFF_ASKPASS_SECRET", qEnvironmentVariable("HANDOFF_PASSWORD"));
    }

    QProcess rsync;
    rsync.setProcessEnvironment(environment);
    rsync.start("rsync", command.arguments);
    if (!rsync.waitForStarted())
        return fail(json, "rsync is not installed on this laptop");
    rsync.waitForFinished(-1);

    const QString errors = QString::fromUtf8(rsync.readAllStandardError()).trimmed();
    if (rsync.exitStatus() != QProcess::NormalExit || rsync.exitCode() != 0) {
        if (!errors.isEmpty())
            err() << errors << "\n";
        return fail(json, Rsync::explain(rsync.exitCode(), errors));
    }

    const auto stats = Rsync::parseStats(QString::fromUtf8(rsync.readAllStandardOutput()));
    if (json)
        out() << QJsonDocument(QJsonObject{{"ok", true}, {"dryRun", plan.dryRun}, {"files", stats.files},
                                           {"bytes", stats.bytes}}).toJson(QJsonDocument::Compact) << "\n";
    else
        out() << stats.files << (plan.dryRun ? " files would be sent (" : " files sent (") << stats.bytes << " bytes)\n";
    return 0;
}

static int status(const QStringList &arguments) {
    Receiver receiver;
    if (arguments.contains("--json")) {
        out() << QJsonDocument(QJsonObject{
            {"ready", receiver.ready()}, {"target", receiver.target()}, {"user", receiver.user()},
            {"address", receiver.address()}, {"subnet", receiver.subnet()}, {"sshRunning", receiver.sshRunning()},
            {"firewallActive", receiver.firewallActive()}, {"firewallOpen", receiver.firewallOpen()},
        }).toJson(QJsonDocument::Compact) << "\n";
    } else {
        out() << (receiver.ready() ? "Ready to receive at " + receiver.target()
                                   : QString("Not receiving. Open Handoff and start receiving.")) << "\n";
    }
    return receiver.ready() ? 0 : 1;
}

static int installSkill() {
    QFile bundled(":/skill/SKILL.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return fail(false, "the skill is missing from this build");
    const QString directory = QDir::homePath() + "/.claude/skills/handoff";
    if (!QDir().mkpath(directory))
        return fail(false, "could not create " + directory);
    QSaveFile file(directory + "/SKILL.md");
    if (!file.open(QIODevice::WriteOnly) || file.write(bundled.readAll()) < 0 || !file.commit())
        return fail(false, "could not write " + file.fileName());
    out() << "Installed " << file.fileName() << "\n";
    return 0;
}

int run(const QStringList &arguments) {
    const QString command = arguments.value(0);
    if (command == "--version") {
        out() << "handoff " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    if (command == "send")
        return send(arguments.mid(1));
    if (command == "status")
        return status(arguments.mid(1));
    if (command == "skill" && arguments.value(1) == "install")
        return installSkill();
    out() << usage;
    return command == "skill" ? 1 : 0;
}

}
