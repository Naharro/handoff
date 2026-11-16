#include "receiver.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

// Runs as root through pkexec, with the action and network as arguments so
// nothing is ever pasted into the script. Opening starts the SSH server for
// this session only (it is not enabled at boot) and lets the local network,
// and nobody else, through the firewall. Closing undoes exactly that.
static const char *helperScript = R"(set -e
case "$1" in
open)
  systemctl start sshd
  if systemctl -q is-active ufw; then
    ufw allow from "$2" to any port 22 proto tcp comment handoff
  fi
  ;;
close)
  if [ -n "$2" ] && systemctl -q is-active ufw; then
    ufw delete allow from "$2" to any port 22 proto tcp || true
  fi
  if [ "$3" = stop ]; then
    systemctl stop sshd
  fi
  ;;
esac
)";

Receiver::Receiver(const QString &settingsFile, const QString &rulesFile, const QString &systemctl,
                   const QString &pkexec, QObject *parent)
    : QObject(parent), m_settingsFile(settingsFile), m_rulesFile(rulesFile), m_systemctl(systemctl),
      m_pkexec(pkexec) {
    if (m_settingsFile.isEmpty())
        m_settingsFile = QDir::homePath() + "/.config/handoff/handoff.conf";
    m_user = qEnvironmentVariable("USER", QDir::home().dirName());

    // What Handoff opened is remembered across restarts, so it can still be
    // closed after the app was quit with the door left open.
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    m_opened = settings.value("receive/opened", false).toBool();
    m_openedSubnet = settings.value("receive/subnet").toString();
    m_startedSsh = settings.value("receive/startedSsh", false).toBool();

    connect(&m_helper, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_error = tr("pkexec is not installed, so Handoff cannot ask for permission.");
            emit changed();
        }
    });
    connect(&m_helper, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        const QString errors = QString::fromUtf8(m_helper.readAllStandardError()).trimmed();
        if (exitCode == 0) {
            m_error.clear();
            m_opened = m_opening;
            QSettings settings(m_settingsFile, QSettings::IniFormat);
            settings.setValue("receive/opened", m_opened);
            settings.setValue("receive/subnet", m_opened ? m_openedSubnet : QString());
            settings.setValue("receive/startedSsh", m_opened && m_startedSsh);
        } else if (exitCode == 126 || exitCode == 127) {
            // pkexec's codes for a dismissed or refused password prompt.
            m_error = tr("Permission was not given, so nothing changed.");
        } else {
            m_error = errors.isEmpty() ? tr("The change failed (code %1).").arg(exitCode) : errors;
        }
        refresh();
        emit changed();
    });

    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, &Receiver::refresh);
    m_poll.start();
    refresh();
}

Receiver::~Receiver() {
    if (m_helper.state() != QProcess::NotRunning) {
        m_helper.disconnect(this);
        m_helper.waitForFinished(3000);
    }
}

// ufw keeps one "tuple" comment per rule; a rule for port 22 coming in means
// SSH gets through.
bool Receiver::rulesAllowSsh(const QString &rules) {
    static const QRegularExpression pattern(R"(^### tuple ### (allow|limit) (tcp|any) 22 \S+ \S+ \S+ in\b)",
                                            QRegularExpression::MultilineOption);
    return pattern.match(rules).hasMatch();
}

QString Receiver::subnetOf(const QString &address, int prefixLength) {
    const QHostAddress host(address);
    if (host.protocol() != QAbstractSocket::IPv4Protocol || prefixLength < 1 || prefixLength > 32)
        return {};
    const quint32 mask = prefixLength == 32 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> prefixLength);
    return QHostAddress(host.toIPv4Address() & mask).toString() + "/" + QString::number(prefixLength);
}

void Receiver::refresh() {
    const bool wasReady = ready();
    const QString before = m_address + m_subnet + m_error
                         + QString::number(m_installed * 8 + m_sshRunning * 4 + m_firewallActive * 2 + m_firewallOpen);

    m_installed = !QStandardPaths::findExecutable("sshd", {"/usr/bin", "/usr/sbin"}).isEmpty()
               && !QStandardPaths::findExecutable("rsync").isEmpty();

    // The Wi-Fi or wired address, skipping the bridges that containers and
    // VPNs add, which the other laptop could never reach.
    QString address, subnet;
    static const QRegularExpression virtualName("^(lo|docker|br-|veth|virbr|tun|tap|wg|tailscale|zt)");
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack) || virtualName.match(interface.name()).hasMatch())
            continue;
        for (const auto &entry : interface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            address = entry.ip().toString();
            subnet = subnetOf(address, entry.prefixLength());
            break;
        }
        if (!address.isEmpty())
            break;
    }
    m_address = address;
    m_subnet = subnet;

    QProcess status;
    status.start(m_systemctl, {"is-active", "sshd", "ufw"});
    status.waitForFinished(1500);
    const QStringList lines = QString::fromUtf8(status.readAllStandardOutput()).split('\n');
    m_sshRunning = lines.value(0).trimmed() == "active";
    m_firewallActive = lines.value(1).trimmed() == "active";

    QFile rules(m_rulesFile);
    m_firewallOpen = rules.open(QIODevice::ReadOnly) && rulesAllowSsh(QString::fromUtf8(rules.readAll()));

    const QString after = m_address + m_subnet + m_error
                        + QString::number(m_installed * 8 + m_sshRunning * 4 + m_firewallActive * 2 + m_firewallOpen);
    if (before != after || wasReady != ready())
        emit changed();
}

void Receiver::run(const QStringList &arguments) {
    m_error.clear();
    m_helper.start(m_pkexec, QStringList{"/bin/sh", "-c", helperScript, "handoff"} + arguments);
    emit changed();
}

void Receiver::open() {
    if (busy())
        return;
    refresh();
    if (!m_installed) {
        m_error = tr("OpenSSH and rsync are needed to receive. Install them with: omarchy-pkg-add openssh rsync");
        emit changed();
        return;
    }
    if (m_subnet.isEmpty()) {
        m_error = tr("This laptop is not on a network. Join the Wi-Fi first.");
        emit changed();
        return;
    }
    m_opening = true;
    // Only stop the SSH server later if it was Handoff that started it.
    m_startedSsh = !m_sshRunning;
    m_openedSubnet = m_subnet;
    run({"open", m_subnet});
}

void Receiver::close() {
    if (busy() || !m_opened)
        return;
    m_opening = false;
    run({"close", m_openedSubnet, m_startedSsh ? "stop" : "keep"});
}

void Receiver::toggle() {
    if (m_opened)
        close();
    else if (!ready())
        open();
}

void Receiver::copyTarget() {
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        app->clipboard()->setText(target());
}
