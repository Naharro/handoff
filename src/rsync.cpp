#include "rsync.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>

namespace Rsync {

static QString tr(const char *text) { return QCoreApplication::translate("Rsync", text); }

// A login and a host name or IPv4 address. Anything else is refused before it
// reaches ssh, so an address can never be read as an option.
bool validTarget(const QString &target) {
    static const QRegularExpression pattern("^([a-z_][a-z0-9_.-]*@)?[A-Za-z0-9][A-Za-z0-9.-]*$");
    return pattern.match(target).hasMatch();
}

QStringList sshOptions(bool batch) {
    // A laptop met for the first time is trusted and remembered; one whose key
    // later changes is refused, which is what protects the password.
    QStringList options{"-o", "StrictHostKeyChecking=accept-new", "-o", "ConnectTimeout=10",
                        "-o", "NumberOfPasswordPrompts=1"};
    if (batch)
        options << "-o" << "BatchMode=yes";
    return options;
}

Command build(const Plan &plan) {
    Command command;
    const QString target = plan.target.trimmed();
    if (target.isEmpty()) {
        command.error = tr("Type the address of the other laptop first.");
        return command;
    }
    if (!validTarget(target)) {
        command.error = tr("That address doesn't look right. It should be like user@192.168.1.20.");
        return command;
    }
    if (plan.sources.isEmpty()) {
        command.error = tr("Add the folders or files to send first.");
        return command;
    }

    QStringList &args = command.arguments;
    // --partial keeps a half-sent file so a dropped Wi-Fi connection resumes,
    // and --no-inc-recursive counts everything up front so the percentage is
    // of the whole transfer rather than of what has been found so far.
    args << "-a" << "-H" << "--partial" << "--mkpath" << "--no-inc-recursive"
         << "--info=progress2,name1" << "--stats";
    if (plan.keepNewer)
        args << "--update";
    if (plan.dryRun)
        args << "--dry-run";
    args << "-e" << ("ssh " + sshOptions(plan.batch).join(' '));

    const QString home = QDir::cleanPath(plan.home);
    QStringList sources;
    for (const QString &source : plan.sources) {
        const QString path = QDir::cleanPath(source);
        if (!path.startsWith('/')) {
            command.error = tr("%1 is not a full path.").arg(source);
            return command;
        }
        if (!plan.mirrorHome) {
            sources << path;
            continue;
        }
        if (path == home) {
            command.error = tr("Pick the folders inside your home folder rather than the whole of it.");
            return command;
        }
        if (home.isEmpty() || !path.startsWith(home + "/")) {
            command.error = tr("%1 is outside your home folder. Choose a folder to send it into.").arg(path);
            return command;
        }
        // With --relative, rsync recreates everything after the "/./" marker,
        // so ~/Documents/tax lands at ~/Documents/tax on the other laptop.
        sources << home + "/./" + path.mid(home.size() + 1);
    }

    QString destination = target + ":";
    if (plan.mirrorHome) {
        args << "--relative";
    } else {
        // Remote paths are relative to the home folder already.
        QString into = plan.into.trimmed();
        if (into == "~")
            into.clear();
        else if (into.startsWith("~/"))
            into.remove(0, 2);
        if (into.isEmpty()) {
            command.error = tr("Type the folder on the other laptop to send into.");
            return command;
        }
        destination += into + (into.endsWith('/') ? "" : "/");
    }

    args << "--" << sources << destination;
    return command;
}

Progress parseProgress(const QString &line) {
    // rsync writes "1,234,567  45%  10.50MB/s  0:01:23", and newer versions
    // put a unit on every number: "1,234,567B  45%  10,500,000B/s  0:01:23".
    static const QRegularExpression pattern(R"(^\s*([\d,]+)B?\s+(\d+)%\s+([\d,.]+)([kMG]?)B/s\s+(\d+:\d\d:\d\d))");
    Progress progress;
    const auto match = pattern.match(line);
    if (!match.hasMatch())
        return progress;
    progress.valid = true;
    progress.bytes = match.captured(1).remove(',').toLongLong();
    progress.percent = qBound(0, match.captured(2).toInt(), 100);
    const QString unit = match.captured(4);
    const double scale = unit == "k" ? 1e3 : unit == "M" ? 1e6 : unit == "G" ? 1e9 : 1;
    progress.bytesPerSecond = qint64(match.captured(3).remove(',').toDouble() * scale);
    progress.remaining = match.captured(5);
    return progress;
}

Stats parseStats(const QString &output) {
    static const QRegularExpression files(R"(Number of regular files transferred: ([\d,.]+))");
    static const QRegularExpression bytes(R"(Total transferred file size: ([\d,.]+))");
    Stats stats;
    auto number = [&](const QRegularExpression &pattern) -> qint64 {
        const auto match = pattern.match(output);
        return match.hasMatch() ? match.captured(1).remove(',').remove('.').toLongLong() : -1;
    };
    stats.files = number(files);
    stats.bytes = number(bytes);
    return stats;
}

// What went wrong, in words, from rsync's exit code and what ssh said.
QString explain(int exitCode, const QString &errors) {
    if (errors.contains("Permission denied"))
        return tr("The other laptop refused the login. Check the user name and password.");
    if (errors.contains("REMOTE HOST IDENTIFICATION HAS CHANGED"))
        return tr("The other laptop's identity has changed since last time, so nothing was sent.");
    if (errors.contains("rsync: command not found") || exitCode == 127)
        return tr("rsync is missing on the other laptop.");
    if (errors.contains("Connection refused"))
        return tr("The other laptop is not receiving. Open Handoff there and start receiving.");
    if (errors.contains("timed out") || errors.contains("No route to host")
        || errors.contains("Could not resolve hostname"))
        return tr("Could not reach the other laptop. Check the address and that both are on the same network.");
    if (exitCode == 23 || exitCode == 24)
        return tr("Some files could not be sent.");
    if (exitCode == 255)
        return tr("Could not connect to the other laptop.");
    return tr("rsync stopped with an error (code %1).").arg(exitCode);
}

}
