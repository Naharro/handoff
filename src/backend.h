#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QUrl>

#include "filepicker.h"
#include "rsync.h"

// The sending side: the list of things to send, where they go, and the rsync
// process that carries them.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QString home READ home CONSTANT)
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY optionsChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY optionsChanged)
    Q_PROPERTY(bool mirrorHome READ mirrorHome WRITE setMirrorHome NOTIFY optionsChanged)
    Q_PROPERTY(QString into READ into WRITE setInto NOTIFY optionsChanged)
    Q_PROPERTY(bool keepNewer READ keepNewer WRITE setKeepNewer NOTIFY optionsChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY stateChanged)
    Q_PROPERTY(int percent READ percent NOTIFY progressChanged)
    Q_PROPERTY(QString speed READ speed NOTIFY progressChanged)
    Q_PROPERTY(QString remaining READ remaining NOTIFY progressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY progressChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY sourcesChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY sourcesChanged)

public:
    // Tests pass their own picker, settings file, and stand-ins for the tools.
    explicit Backend(FilePicker *picker = nullptr, const QString &settingsFile = {}, const QString &rsync = "rsync",
                     const QString &ssh = "ssh", QObject *parent = nullptr);
    ~Backend() override;

    QStringList sources() const { return m_sources; }
    QString home() const { return m_home; }
    QString target() const { return m_target; }
    QString password() const { return m_password; }
    bool mirrorHome() const { return m_mirrorHome; }
    QString into() const { return m_into; }
    bool keepNewer() const { return m_keepNewer; }
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    bool checking() const { return m_check.state() != QProcess::NotRunning; }
    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QString detail() const { return m_detail; }
    int percent() const { return m_percent; }
    QString speed() const { return m_speed; }
    QString remaining() const { return m_remaining; }
    QString currentFile() const { return m_currentFile; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    void setTarget(const QString &target);
    void setPassword(const QString &password);
    void setMirrorHome(bool mirror);
    void setInto(const QString &into);
    void setKeepNewer(bool keep);
    void setNotifies(bool notifies) { m_notifies = notifies; }

    Q_INVOKABLE void addFolders();
    Q_INVOKABLE void addFiles();
    Q_INVOKABLE void addPaths(const QStringList &paths);
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void send() { start(false); }
    Q_INVOKABLE void preview() { start(true); }
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void checkConnection();

signals:
    void sourcesChanged();
    void optionsChanged();
    void stateChanged();
    void progressChanged();

private:
    void start(bool dryRun);
    void setSources(const QStringList &sources);
    void setState(const QString &state, const QString &message, const QString &detail = {});
    void readOutput();
    void finished(int exitCode, QProcess::ExitStatus status);
    void save(const QString &key, const QVariant &value);
    QProcessEnvironment environment() const;

    FilePicker *m_picker;
    QString m_settingsFile, m_rsync, m_ssh, m_home;
    QStringList m_sources;
    QList<QStringList> m_undo, m_redo;
    QString m_target, m_password, m_into;
    bool m_mirrorHome = true, m_keepNewer = true;
    bool m_notifies = false, m_dryRun = false, m_cancelled = false;

    QProcess m_process, m_check;
    QString m_state = "idle", m_message, m_detail;
    QByteArray m_buffer;
    QString m_output, m_errors;
    int m_percent = 0;
    QString m_speed, m_remaining, m_currentFile;
};
