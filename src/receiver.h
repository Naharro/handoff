#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

// The receiving side: whether this laptop can be reached over SSH, the
// address the sender should type, and opening and closing the door.
class Receiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString user READ user CONSTANT)
    Q_PROPERTY(QString address READ address NOTIFY changed)
    Q_PROPERTY(QString target READ target NOTIFY changed)
    Q_PROPERTY(QString subnet READ subnet NOTIFY changed)
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool sshRunning READ sshRunning NOTIFY changed)
    Q_PROPERTY(bool firewallActive READ firewallActive NOTIFY changed)
    Q_PROPERTY(bool firewallOpen READ firewallOpen NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool opened READ opened NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    explicit Receiver(const QString &settingsFile = {}, const QString &rulesFile = "/etc/ufw/user.rules",
                      const QString &systemctl = "systemctl", const QString &pkexec = "pkexec",
                      QObject *parent = nullptr);
    ~Receiver() override;

    QString user() const { return m_user; }
    QString address() const { return m_address; }
    QString target() const { return m_address.isEmpty() ? QString() : m_user + "@" + m_address; }
    QString subnet() const { return m_subnet; }
    bool installed() const { return m_installed; }
    bool sshRunning() const { return m_sshRunning; }
    bool firewallActive() const { return m_firewallActive; }
    bool firewallOpen() const { return m_firewallOpen; }
    bool ready() const { return m_sshRunning && (!m_firewallActive || m_firewallOpen) && !m_address.isEmpty(); }
    // True while Handoff itself has opened this laptop, and so should close it.
    bool opened() const { return m_opened; }
    bool busy() const { return m_helper.state() != QProcess::NotRunning; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void copyTarget();

    static bool rulesAllowSsh(const QString &rules);
    static QString subnetOf(const QString &address, int prefixLength);

signals:
    void changed();

private:
    void run(const QStringList &arguments);

    QString m_settingsFile, m_rulesFile, m_systemctl, m_pkexec;
    QString m_user, m_address, m_subnet, m_error;
    bool m_installed = true, m_sshRunning = false, m_firewallActive = false, m_firewallOpen = false;
    bool m_opened = false, m_opening = false, m_startedSsh = false;
    QString m_openedSubnet;
    QProcess m_helper;
    QTimer m_poll;
};
