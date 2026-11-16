#pragma once

#include <QStringList>

// The commands that need no window, so scripts and agents can drive Handoff.
// The shape follows Hype's command line (MIT, David Heinemeier Hansson).
namespace Cli {

bool handles(const QString &command);
int askpass(const QStringList &arguments);
int run(const QStringList &arguments);

}
