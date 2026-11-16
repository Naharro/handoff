#pragma once

#include <QObject>
#include <QStringList>

// Asks the user for files or folders. The app talks to this rather than to
// the desktop portal directly, so tests can answer for the user. The pattern
// follows Omacut (MIT, David Heinemeier Hansson).
class FilePicker : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    virtual void pick(bool folders, const QString &title) = 0;

signals:
    void picked(const QStringList &paths);
    void failed(const QString &error);
};
