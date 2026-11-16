#pragma once

#include <QString>
#include <QStringList>

// The rsync command line and the reading of its output, kept free of
// processes so every rule about what gets sent where is covered by tests.
namespace Rsync {

struct Plan {
    QString target;          // user@address of the laptop that receives
    QStringList sources;     // absolute local paths
    bool mirrorHome = true;  // land each source at the same place under the remote home
    QString into;            // remote folder, when not mirroring
    bool keepNewer = true;   // leave files that are newer on the receiver alone
    bool dryRun = false;
    bool batch = false;      // never ask for a password (the command line)
    QString home;            // the local home folder
};

struct Command {
    QStringList arguments;
    QString error;
};

struct Progress {
    bool valid = false;
    qint64 bytes = 0;
    int percent = 0;
    qint64 bytesPerSecond = 0;
    QString remaining;
};

struct Stats {
    qint64 files = -1;
    qint64 bytes = -1;
};

bool validTarget(const QString &target);
QStringList sshOptions(bool batch);
Command build(const Plan &plan);
Progress parseProgress(const QString &line);
Stats parseStats(const QString &output);
QString explain(int exitCode, const QString &errors);

}
