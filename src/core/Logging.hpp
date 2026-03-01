#pragma once

#include <QLoggingCategory>
#include <QStringList>

// --- Subsystem logging categories ---
Q_DECLARE_LOGGING_CATEGORY(lcAA)
Q_DECLARE_LOGGING_CATEGORY(lcBT)
Q_DECLARE_LOGGING_CATEGORY(lcAudio)
Q_DECLARE_LOGGING_CATEGORY(lcPlugin)
Q_DECLARE_LOGGING_CATEGORY(lcUI)
Q_DECLARE_LOGGING_CATEGORY(lcCore)

namespace oap {

/// Install the custom message handler. Call early in main(), before any logging.
void installLogHandler();

/// Enable or disable verbose (debug-level) output for all categories.
void setVerbose(bool verbose);

/// Query current verbose state.
bool isVerbose();

/// Enable debug output only for the named categories (e.g. {"aa", "bt"}).
/// Pass "aa" to also enable "oaa.*" (open-androidauto library).
void setDebugCategories(const QStringList& categories);

/// Enable file logging in addition to stderr.
void setLogFile(const QString& path);

/// Test helper: returns true if a message context looks like it came from
/// the open-androidauto library (based on file path, category, or bracket tag).
bool isLibraryMessage(const char* category, const char* file, const QString& message);

} // namespace oap
