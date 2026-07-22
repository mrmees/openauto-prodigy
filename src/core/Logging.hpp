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
Q_DECLARE_LOGGING_CATEGORY(lcEq)

namespace oap {

/// Install the custom message handler. Call early in main(), before any logging.
void installLogHandler();

/// Enable or disable verbose (debug-level) output for all categories.
void setVerbose(bool verbose);

/// Query current verbose state.
bool isVerbose();

/// Return whether category is one of the documented short category names.
bool isValidDebugCategory(const QString& category);

/// Validate a selective category list. If supplied, error receives a useful
/// explanation suitable for an IPC response.
bool validateDebugCategories(const QStringList& categories, QString* error = nullptr);

/// Enable debug output only for the named categories (e.g. {"aa", "bt"}).
/// Pass "aa" to also enable "oaa.*" (prodigy-oaa-protocol library).
/// Invalid entries are ignored defensively and never become filter-rule text.
void setDebugCategories(const QStringList& categories);

/// Apply the shared verbose-versus-selective logging policy. Verbose mode
/// takes precedence; otherwise the supplied selective categories are applied.
void applyLoggingPolicy(bool verbose, const QStringList& categories);

/// Enable file logging in addition to stderr.
void setLogFile(const QString& path);

/// Test helper: returns true if a message context looks like it came from
/// the prodigy-oaa-protocol library (based on file path, category, or bracket tag).
bool isLibraryMessage(const char* category, const char* file, const QString& message);

} // namespace oap
