#pragma once

#include <QVariantMap>

#include "filepicker.h"

// The desktop's own file chooser, through xdg-desktop-portal.
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    using FilePicker::FilePicker;
    void pick(bool folders, const QString &title) override;

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    QString m_requestPath;
};
