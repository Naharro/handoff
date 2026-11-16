// The portal request dance follows Omacut (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>
#include <QUrl>

static const char *service = "org.freedesktop.portal.Desktop";
static const char *requestInterface = "org.freedesktop.portal.Request";

void PortalFilePicker::pick(bool folders, const QString &title) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(tr("The desktop's file chooser is not available."));
        return;
    }
    if (!m_requestPath.isEmpty())
        return;

    // The answer arrives as a signal on a request object whose path is known
    // in advance, so listen before asking or a fast answer is missed.
    const QString token = "handoff" + QString::number(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    m_requestPath = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
    bus.connect(service, m_requestPath, requestInterface, "Response", this, SLOT(onResponse(uint, QVariantMap)));

    QVariantMap options{{"handle_token", token}, {"multiple", true}, {"directory", folders}, {"modal", true}};
    auto message = QDBusMessage::createMethodCall(service, "/org/freedesktop/portal/desktop",
                                                  "org.freedesktop.portal.FileChooser", "OpenFile");
    message << QString() << title << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (!reply.isError())
            return;
        QDBusConnection::sessionBus().disconnect(service, m_requestPath, requestInterface, "Response", this,
                                                 SLOT(onResponse(uint, QVariantMap)));
        m_requestPath.clear();
        emit failed(tr("The desktop's file chooser is not available."));
    });
}

void PortalFilePicker::onResponse(uint response, const QVariantMap &results) {
    QDBusConnection::sessionBus().disconnect(service, m_requestPath, requestInterface, "Response", this,
                                             SLOT(onResponse(uint, QVariantMap)));
    m_requestPath.clear();
    // Anything but 0 means the user closed the chooser, which is not an error.
    if (response != 0)
        return;
    QStringList paths;
    for (const QString &uri : results.value("uris").toStringList()) {
        const QString path = QUrl(uri).toLocalFile();
        if (!path.isEmpty())
            paths << path;
    }
    if (!paths.isEmpty())
        emit picked(paths);
}
