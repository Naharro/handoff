#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>

Backend::Backend(FilePicker *picker, const QString &settingsFile, const QString &rsync, const QString &ssh,
                 QObject *parent)
    : QObject(parent), m_picker(picker), m_settingsFile(settingsFile), m_rsync(rsync), m_ssh(ssh),
      m_home(QDir::homePath()) {
    if (m_settingsFile.isEmpty())
        m_settingsFile = QDir::homePath() + "/.config/handoff/handoff.conf";
    // The address and options are remembered; the password never is.
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    m_target = settings.value("send/target").toString();
    m_mirrorHome = settings.value("send/mirrorHome", true).toBool();
    m_into = settings.value("send/into", "Handoff").toString();
    m_keepNewer = settings.value("send/keepNewer", true).toBool();

    if (m_picker) {
        connect(m_picker, &FilePicker::picked, this, &Backend::addPaths);
        connect(m_picker, &FilePicker::failed, this, [this](const QString &error) { setState("failed", error); });
    }

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &Backend::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_errors += QString::fromUtf8(m_process.readAllStandardError());
    });
    connect(&m_process, &QProcess::finished, this, &Backend::finished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setState("failed", tr("rsync is not installed on this laptop."));
    });

    connect(&m_check, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setState("failed", tr("ssh is not installed on this laptop."));
    });
    connect(&m_check, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        const QString output = QString::fromUtf8(m_check.readAllStandardOutput()).trimmed();
        const QString errors = QString::fromUtf8(m_check.readAllStandardError()).trimmed();
        if (exitCode == 0 && output == "ready")
            setState("checked", tr("Connected to %1. Ready to send.").arg(m_target));
        else if (exitCode == 0)
            setState("failed", tr("rsync is missing on the other laptop."));
        else
            setState("failed", Rsync::explain(exitCode, errors), errors);
    });
}

Backend::~Backend() {
    for (QProcess *process : {&m_process, &m_check}) {
        if (process->state() == QProcess::NotRunning)
            continue;
        process->disconnect(this);
        process->terminate();
        if (!process->waitForFinished(2000))
            process->kill();
        process->waitForFinished(1000);
    }
}

void Backend::save(const QString &key, const QVariant &value) {
    QSettings(m_settingsFile, QSettings::IniFormat).setValue(key, value);
}

void Backend::setTarget(const QString &target) {
    if (target.trimmed() == m_target)
        return;
    m_target = target.trimmed();
    save("send/target", m_target);
    emit optionsChanged();
}

void Backend::setPassword(const QString &password) {
    if (password == m_password)
        return;
    m_password = password;
    emit optionsChanged();
}

void Backend::setMirrorHome(bool mirror) {
    if (mirror == m_mirrorHome)
        return;
    m_mirrorHome = mirror;
    save("send/mirrorHome", mirror);
    emit optionsChanged();
}

void Backend::setInto(const QString &into) {
    if (into == m_into)
        return;
    m_into = into;
    save("send/into", into);
    emit optionsChanged();
}

void Backend::setKeepNewer(bool keep) {
    if (keep == m_keepNewer)
        return;
    m_keepNewer = keep;
    save("send/keepNewer", keep);
    emit optionsChanged();
}

void Backend::addFolders() {
    if (m_picker)
        m_picker->pick(true, tr("Folders to send"));
}

void Backend::addFiles() {
    if (m_picker)
        m_picker->pick(false, tr("Files to send"));
}

void Backend::addUrls(const QList<QUrl> &urls) {
    QStringList paths;
    for (const QUrl &url : urls)
        if (url.isLocalFile())
            paths << url.toLocalFile();
    addPaths(paths);
}

void Backend::addPaths(const QStringList &paths) {
    QStringList sources = m_sources;
    QStringList missing;
    for (const QString &given : paths) {
        const QString path = QDir::cleanPath(QFileInfo(given).absoluteFilePath());
        if (!QFileInfo::exists(path))
            missing << given;
        else if (!sources.contains(path))
            sources << path;
    }
    if (!missing.isEmpty())
        setState("failed", tr("%1 does not exist.").arg(missing.join(", ")));
    setSources(sources);
}

void Backend::removeAt(int index) {
    if (index < 0 || index >= m_sources.size())
        return;
    QStringList sources = m_sources;
    sources.removeAt(index);
    setSources(sources);
}

void Backend::setSources(const QStringList &sources) {
    if (sources == m_sources)
        return;
    m_undo << m_sources;
    m_redo.clear();
    m_sources = sources;
    emit sourcesChanged();
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo << m_sources;
    m_sources = m_undo.takeLast();
    emit sourcesChanged();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo << m_sources;
    m_sources = m_redo.takeLast();
    emit sourcesChanged();
}

// ssh has no terminal to ask for a password on, so it is told to ask this
// same binary instead ("handoff askpass"), which answers from the
// environment. Only this user's own processes can read that.
QProcessEnvironment Backend::environment() const {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");
    environment.insert("SSH_ASKPASS", QCoreApplication::applicationFilePath());
    environment.insert("SSH_ASKPASS_REQUIRE", "force");
    environment.insert("HANDOFF_ASKPASS", "1");
    environment.insert("HANDOFF_ASKPASS_SECRET", m_password);
    return environment;
}

void Backend::setState(const QString &state, const QString &message, const QString &detail) {
    m_state = state;
    m_message = message;
    m_detail = detail;
    emit stateChanged();
}

void Backend::start(bool dryRun) {
    if (running() || checking())
        return;

    Rsync::Plan plan;
    plan.target = m_target;
    plan.sources = m_sources;
    plan.mirrorHome = m_mirrorHome;
    plan.into = m_into;
    plan.keepNewer = m_keepNewer;
    plan.dryRun = dryRun;
    plan.home = m_home;
    const auto command = Rsync::build(plan);
    if (!command.error.isEmpty()) {
        setState("failed", command.error);
        return;
    }

    m_dryRun = dryRun;
    m_cancelled = false;
    m_buffer.clear();
    m_output.clear();
    m_errors.clear();
    m_percent = 0;
    m_speed.clear();
    m_remaining.clear();
    m_currentFile.clear();
    emit progressChanged();

    m_process.setProcessEnvironment(environment());
    m_process.start(m_rsync, command.arguments);
    setState("running", dryRun ? tr("Checking what would be sent…") : tr("Sending to %1…").arg(m_target));
}

void Backend::cancel() {
    if (running()) {
        m_cancelled = true;
        m_process.terminate();
    }
    if (checking())
        m_check.kill();
}

void Backend::checkConnection() {
    if (running() || checking())
        return;
    const QString target = m_target.trimmed();
    if (target.isEmpty() || !Rsync::validTarget(target)) {
        Rsync::Plan plan;
        plan.target = target;
        setState("failed", Rsync::build(plan).error);
        return;
    }
    m_check.setProcessEnvironment(environment());
    m_check.start(m_ssh, Rsync::sshOptions(false) << target << "command -v rsync >/dev/null && echo ready");
    setState("checking", tr("Connecting to %1…").arg(target));
}

void Backend::readOutput() {
    m_buffer += m_process.readAllStandardOutput();
    // Progress lines end in a carriage return, file names in a newline.
    qsizetype end;
    bool changed = false;
    while ((end = m_buffer.indexOf('\n')) >= 0 || (end = m_buffer.indexOf('\r')) >= 0) {
        const qsizetype carriage = m_buffer.indexOf('\r');
        if (carriage >= 0 && carriage < end)
            end = carriage;
        const QString line = QString::fromUtf8(m_buffer.left(end));
        m_buffer.remove(0, end + 1);
        if (line.trimmed().isEmpty())
            continue;

        const auto progress = Rsync::parseProgress(line);
        if (progress.valid) {
            m_percent = progress.percent;
            m_speed = QLocale().formattedDataSize(progress.bytesPerSecond, 1, QLocale::DataSizeSIFormat) + "/s";
            m_remaining = progress.remaining;
            changed = true;
        } else if (line.startsWith("Number of ") || !m_output.isEmpty()) {
            // The closing statistics; everything from here on is the summary.
            m_output += line + "\n";
        } else {
            m_currentFile = line;
            changed = true;
        }
    }
    if (changed)
        emit progressChanged();
}

void Backend::finished(int exitCode, QProcess::ExitStatus status) {
    readOutput();
    m_currentFile.clear();
    const QString errors = m_errors.trimmed();

    if (m_cancelled) {
        setState("cancelled", tr("Stopped. Send again to carry on where it left off."));
    } else if (status == QProcess::NormalExit && exitCode == 0) {
        const auto stats = Rsync::parseStats(m_output);
        const int files = int(qMax<qint64>(0, stats.files));
        const QString size = QLocale().formattedDataSize(qMax<qint64>(0, stats.bytes), 1, QLocale::DataSizeSIFormat);
        m_percent = 100;
        if (files == 0)
            setState("done", tr("Nothing to send: the other laptop already has everything."));
        else if (m_dryRun)
            setState("done", tr("%n files would be sent (%1).", "", files).arg(size));
        else
            setState("done", tr("%n files sent (%1).", "", files).arg(size));
    } else {
        setState("failed", Rsync::explain(exitCode, errors), errors);
    }
    emit progressChanged();

    if (m_notifies && !m_dryRun && !m_cancelled)
        QProcess::startDetached("notify-send", {"-a", "Handoff", "Handoff", m_message});
}
