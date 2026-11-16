#pragma once

#include <QStringList>

// Commands that need no window, for scripts and agents:
//   torrente add <magnet|url|file>...   add torrents to the engine
//   torrente list [--json]              list torrents
// Each exits 0 on success and 1 on failure, with errors on stderr.
namespace Cli {

// True when the arguments name a command rather than things to open.
bool isCommand(const QStringList &arguments);
int run(const QStringList &arguments);

} // namespace Cli
