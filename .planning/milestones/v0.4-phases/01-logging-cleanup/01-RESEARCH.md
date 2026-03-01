# Phase 1: Logging Cleanup - Research

**Researched:** 2026-03-01
**Domain:** Qt QLoggingCategory, custom message handlers, runtime log filtering
**Confidence:** HIGH

## Summary

This phase replaces 256 raw `qDebug()`/`qInfo()`/`qWarning()` calls across 26 source files (plus 113 in the open-androidauto library submodule) with Qt's built-in `QLoggingCategory` system. The project already uses Qt's logging macros exclusively (no third-party logging), so the migration is straightforward: declare categories, replace calls with category-aware macros, install a custom message handler for formatting and filtering, and wire up runtime controls.

The open-androidauto library has one existing `Q_LOGGING_CATEGORY` instance (`oaa.messenger.frameparser`) already using the `oaa.*` prefix convention. The main app has zero QLoggingCategory usage today and no CLI argument parsing (`QCommandLineParser` is not used). All log messages use manual bracket prefixes like `[AAOrchestrator]`, `[BtManager]`, `[VideoDecoder]` etc.

**Primary recommendation:** Use Qt's `QLoggingCategory` with `qCDebug`/`qCInfo`/`qCWarning` macros, a custom `qInstallMessageHandler`, and `QLoggingCategory::setFilterRules()` for runtime toggling. No external logging libraries needed.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Per-subsystem categories (~6): `oap.aa`, `oap.bt`, `oap.audio`, `oap.plugin`, `oap.ui`, `oap.core`
- Consolidate existing manual prefix tags into these 6 categories
- Dotted hierarchy naming convention enabling Qt wildcard filtering
- Default (quiet) shows key lifecycle events + warnings only
- CLI flag (`--verbose` or `--debug`) AND persistent config file setting
- Per-category control in config (`debug_categories: [aa, bt]`)
- Runtime toggle via web config panel (IPC, no restart)
- `--verbose` includes open-androidauto library output
- Custom `qInstallMessageHandler()` for formatting/filtering
- Qt filter rules to suppress library output by default
- Design `oap.lib.*` naming convention for library adoption later
- In quiet mode, allow major library connection events through but suppress protocol chatter
- Millisecond timestamps in app output
- Thread names/IDs in verbose output

### Claude's Discretion
- Exact category tag format in output (short bracket vs full dotted name)
- Whether to add optional `--log-file` flag for file output
- Custom message handler formatting details
- How to detect and classify library messages without categories (pattern matching, source file, etc.)
- Mapping of existing ~15 manual prefix tags to the 6 subsystem categories

### Deferred Ideas (OUT OF SCOPE)
None
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LOG-01 | Default log output is clean -- no debug spam in journalctl during normal operation | QLoggingCategory default rules suppress `qCDebug` by default; custom message handler filters library output; category-based filtering via `setFilterRules()` |
| LOG-02 | Debug-level logging is available via --verbose flag or config option | `QCommandLineParser` for CLI, `YamlConfig` for persistent setting, `QLoggingCategory::setFilterRules()` to enable all categories at runtime |
| LOG-03 | Log categories are defined per component (AA, BT, Audio, Plugin, etc.) | 6 subsystem categories via `Q_LOGGING_CATEGORY` / `Q_DECLARE_LOGGING_CATEGORY` macros, dotted hierarchy `oap.*` |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| QLoggingCategory | Qt 6.4+ (built-in) | Category-based log filtering | Qt's native categorized logging; zero dependencies, compile-time disable support |
| QCommandLineParser | Qt 6.4+ (built-in) | CLI `--verbose` / `--debug` flag parsing | Qt's standard CLI parser; type-safe, auto-generates `--help` |
| qInstallMessageHandler | Qt 6.4+ (built-in) | Custom message formatting and output routing | Only way to control Qt's log output format and destination globally |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| YamlConfig (existing) | In-tree | Persistent `debug_categories` setting | Already handles config read/write and IPC updates |
| IpcServer (existing) | In-tree | Runtime verbose toggle from web panel | Already has JSON command dispatch; add `set_logging` / `get_logging` commands |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| QLoggingCategory | spdlog / Boost.Log | Overkill -- project already uses Qt logging exclusively; adding external deps for no benefit |
| Custom message handler | QT_MESSAGE_PATTERN env var | Env var can set format but cannot do runtime toggling or library message filtering |
| Per-file categories | Per-class categories | Per-class (~26 categories) is too granular; 6 subsystem categories match the architecture |

## Architecture Patterns

### Recommended Project Structure
```
src/
  core/
    Logging.hpp          # Q_DECLARE_LOGGING_CATEGORY for all 6 categories + message handler declaration
    Logging.cpp          # Q_LOGGING_CATEGORY definitions + custom message handler implementation
  main.cpp              # QCommandLineParser, qInstallMessageHandler, initial filter rules
```

### Pattern 1: Centralized Category Declaration
**What:** One header declares all logging categories; one .cpp defines them. Every source file includes the header.
**When to use:** Small-to-medium projects with a fixed set of subsystem categories.
**Example:**
```cpp
// src/core/Logging.hpp
#pragma once
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcAA)
Q_DECLARE_LOGGING_CATEGORY(lcBT)
Q_DECLARE_LOGGING_CATEGORY(lcAudio)
Q_DECLARE_LOGGING_CATEGORY(lcPlugin)
Q_DECLARE_LOGGING_CATEGORY(lcUI)
Q_DECLARE_LOGGING_CATEGORY(lcCore)

namespace oap {
void installLogHandler();
void setVerbose(bool enabled);
void setDebugCategories(const QStringList& categories);
}
```

```cpp
// src/core/Logging.cpp
#include "Logging.hpp"
#include <QDateTime>
#include <QThread>
#include <cstdio>

Q_LOGGING_CATEGORY(lcAA,     "oap.aa",     QtInfoMsg)
Q_LOGGING_CATEGORY(lcBT,     "oap.bt",     QtInfoMsg)
Q_LOGGING_CATEGORY(lcAudio,  "oap.audio",  QtInfoMsg)
Q_LOGGING_CATEGORY(lcPlugin, "oap.plugin", QtInfoMsg)
Q_LOGGING_CATEGORY(lcUI,     "oap.ui",     QtInfoMsg)
Q_LOGGING_CATEGORY(lcCore,   "oap.core",   QtInfoMsg)

// The third argument (QtInfoMsg) sets the default threshold:
// QtDebugMsg = show everything (verbose)
// QtInfoMsg  = suppress debug (quiet default)
// QtWarningMsg = suppress debug+info
```

### Pattern 2: Custom Message Handler with Library Detection
**What:** A `qInstallMessageHandler` callback that formats output, detects library messages (by category prefix or bracket tag pattern), and applies filtering rules.
**When to use:** When you need to control output from both your code and third-party library code that uses Qt logging.
**Example:**
```cpp
static bool g_verbose = false;
static QStringList g_activeCategories;

// Library messages without QLoggingCategory still have category == "default"
// They can be identified by bracket prefix pattern: "[ControlChannel]", "[TCPTransport]"
static bool isLibraryMessage(const QString& category, const QString& msg)
{
    if (category.startsWith("oaa."))
        return true;
    // Heuristic: bracket tags from open-androidauto source files
    static const QStringList libTags = {
        "[ControlChannel]", "[TCPTransport]", "[BluetoothChannel]",
        "[VideoChannel]", "[AudioChannel]", "[AASession]",
        "[Messenger]", "[FrameParser]", "[FrameAssembler]"
        // ... etc
    };
    for (const auto& tag : libTags) {
        if (msg.startsWith(tag))
            return true;
    }
    return false;
}

static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    // In quiet mode, suppress library debug/info (except lifecycle)
    // ...format with timestamp, optional thread ID, category tag...
}
```

### Pattern 3: Runtime Filter Rule Toggle
**What:** Use `QLoggingCategory::setFilterRules()` to change log verbosity at runtime without restart.
**When to use:** Web panel toggle, IPC command.
**Example:**
```cpp
void oap::setVerbose(bool enabled)
{
    g_verbose = enabled;
    if (enabled) {
        QLoggingCategory::setFilterRules("oap.*=true\noaa.*=true");
    } else {
        QLoggingCategory::setFilterRules("oap.*.debug=false\noaa.*=false");
    }
}

void oap::setDebugCategories(const QStringList& categories)
{
    QStringList rules;
    rules << "oap.*.debug=false" << "oaa.*=false";  // default: quiet
    for (const auto& cat : categories) {
        rules << QString("oap.%1.debug=true").arg(cat);
        if (cat == "aa")
            rules << "oaa.*=true";  // AA verbose includes library
    }
    QLoggingCategory::setFilterRules(rules.join('\n'));
}
```

### Pattern 4: IPC Command for Logging
**What:** Add `set_logging` / `get_logging` commands to the existing IPC dispatch.
**When to use:** Web panel runtime toggle.
**Example:**
```cpp
// In IpcServer::handleRequest():
if (command == QLatin1String("set_logging"))
    return handleSetLogging(data);
if (command == QLatin1String("get_logging"))
    return handleGetLogging();

// handleSetLogging reads:
// {"command":"set_logging","data":{"verbose":true}}
// or {"command":"set_logging","data":{"categories":["aa","bt"]}}
```

### Anti-Patterns to Avoid
- **Per-file or per-class categories:** 26+ categories is unmanageable for users. Stick to 6 subsystem categories.
- **Defining categories in header files:** `Q_LOGGING_CATEGORY` (the definition macro) in a header causes duplicate symbol errors when multiple .cpp files include it. Only `Q_DECLARE_LOGGING_CATEGORY` goes in headers.
- **Using `QT_MESSAGE_PATTERN` for filtering:** Env var only controls formatting, not filtering. Cannot be changed at runtime.
- **Calling `setFilterRules()` multiple times with partial rules:** Each call REPLACES all previous rules. Always emit the complete ruleset.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Log category filtering | Custom enable/disable flags per subsystem | `QLoggingCategory` + `setFilterRules()` | Qt already handles category hierarchy, wildcard matching, and compile-time elision |
| CLI argument parsing | Manual `argv` scanning | `QCommandLineParser` | Handles `--help`, `--version`, value options, error reporting |
| Log formatting | Manual `QString::asprintf` everywhere | `qInstallMessageHandler` | Single handler, consistent format, respects Qt's built-in context (file, line, function) |
| Log file output | Manual `QFile` write in every log call | `qInstallMessageHandler` with `FILE*` | One place to control file rotation, flushing |

## Common Pitfalls

### Pitfall 1: Q_LOGGING_CATEGORY Default Level Argument
**What goes wrong:** Forgetting the third argument to `Q_LOGGING_CATEGORY` means debug messages are enabled by default, defeating the "quiet by default" goal.
**Why it happens:** The macro defaults to `QtDebugMsg` (show everything) if no threshold is specified.
**How to avoid:** Always pass `QtInfoMsg` as the third argument: `Q_LOGGING_CATEGORY(lcAA, "oap.aa", QtInfoMsg)`.
**Warning signs:** Debug output appearing when `--verbose` is NOT set.

### Pitfall 2: setFilterRules Replaces All Rules
**What goes wrong:** Calling `setFilterRules("oap.aa.debug=true")` disables all other previously-set rules.
**Why it happens:** `setFilterRules()` is not additive -- it replaces the entire ruleset.
**How to avoid:** Always build the complete set of rules and apply them in one call.
**Warning signs:** Enabling one category silences another.

### Pitfall 3: Library Messages Without Categories
**What goes wrong:** The 113 log calls in open-androidauto mostly use raw `qDebug()`/`qInfo()` (only 1 file uses QLoggingCategory). These go to the "default" category and can't be filtered by Qt's category rules.
**Why it happens:** The library hasn't adopted QLoggingCategory yet, and we can't modify the submodule.
**How to avoid:** The custom message handler must detect library messages heuristically (bracket prefix pattern matching or `QMessageLogContext::file` path containing "open-androidauto") and apply filtering logic in the handler itself.
**Warning signs:** Library debug spam leaking through in quiet mode.

### Pitfall 4: Thread Safety of Custom Message Handler
**What goes wrong:** The custom message handler is called from multiple threads (ASIO worker threads, Qt main thread). Writing to shared state (verbose flag, category list) without synchronization causes races.
**Why it happens:** AA protocol runs on Boost.ASIO threads; UI on Qt main thread; PipeWire on its own thread.
**How to avoid:** Use `std::atomic<bool>` for the verbose flag. For category list changes, use `QLoggingCategory::setFilterRules()` which is thread-safe. The message handler's output (fprintf/write) is inherently serialized by the OS for stderr.
**Warning signs:** Garbled log lines, missing output under load.

### Pitfall 5: QMessageLogContext::category is Null for Uncategorized Messages
**What goes wrong:** Accessing `ctx.category` when it's null causes a crash.
**Why it happens:** Messages from `qDebug() << "..."` (no category) may have `ctx.category == nullptr` depending on Qt build configuration.
**How to avoid:** Always null-check: `QString cat = ctx.category ? ctx.category : "default"`.
**Warning signs:** Segfault on first log message.

## Code Examples

### Migration Pattern: Before and After

**Before (current):**
```cpp
qInfo() << "[BtManager] Pairable:" << enabled;
qDebug() << "[AAPlugin] Sidebar vol:" << level;
qWarning() << "[BtManager] RemoveDevice failed:" << reply.error().message();
```

**After:**
```cpp
#include "core/Logging.hpp"

qCInfo(lcBT) << "Pairable:" << enabled;
qCDebug(lcAA) << "Sidebar vol:" << level;
qCWarning(lcBT) << "RemoveDevice failed:" << reply.error().message();
```

The category tag replaces the manual bracket prefix. The custom message handler adds `[AA]` or `[BT]` formatting automatically.

### Tag-to-Category Mapping

| Existing Bracket Tag | New Category | Rationale |
|---------------------|-------------|-----------|
| `[AAOrchestrator]` | `oap.aa` (lcAA) | Android Auto subsystem |
| `[AAPlugin]` | `oap.aa` (lcAA) | Android Auto subsystem |
| `[VideoDecoder]` | `oap.aa` (lcAA) | AA video pipeline |
| `[EvdevTouch]` | `oap.aa` (lcAA) | AA touch input |
| `[ServiceDiscoveryBuilder]` | `oap.aa` (lcAA) | AA protocol |
| `[ThemeNightMode]` | `oap.aa` (lcAA) | AA-triggered night mode |
| `[BTDiscovery]` | `oap.bt` (lcBT) | Bluetooth discovery |
| `[BtManager]` | `oap.bt` (lcBT) | Bluetooth management |
| `[BtAgent]` | `oap.bt` (lcBT) | BlueZ agent |
| `[Audio]` | `oap.audio` (lcAudio) | Audio/PipeWire |
| `[GpioNightMode]` | `oap.core` (lcCore) | System GPIO |
| `[TimedNightMode]` | `oap.core` (lcCore) | System timer |
| `[PluginViewHost]` | `oap.ui` (lcUI) | QML view loading |
| `[system-service]` | `oap.core` (lcCore) | System daemon IPC |
| (no tag - main.cpp) | `oap.core` (lcCore) | Application lifecycle |
| (no tag - PluginManager) | `oap.plugin` (lcPlugin) | Plugin lifecycle |
| (no tag - CompanionListener) | `oap.core` (lcCore) | Companion service |

### Library Message Detection via File Path
```cpp
static bool isLibraryMessage(const QMessageLogContext& ctx)
{
    // QMessageLogContext::file contains source path at compile time
    if (ctx.file && strstr(ctx.file, "open-androidauto"))
        return true;
    // Also check category prefix for any that adopted QLoggingCategory
    if (ctx.category && strncmp(ctx.category, "oaa.", 4) == 0)
        return true;
    return false;
}
```

### Config YAML Schema for Logging
```yaml
logging:
  verbose: false               # Global verbose (same as --verbose)
  debug_categories: []         # Per-category: ["aa", "bt", "audio"]
  # Optional (Claude's discretion):
  log_file: ""                 # Empty = stderr only
```

### QCommandLineParser Setup
```cpp
QCommandLineParser parser;
parser.setApplicationDescription("OpenAuto Prodigy");
parser.addHelpOption();
parser.addVersionOption();

QCommandLineOption verboseOption(
    QStringList() << "v" << "verbose",
    "Enable verbose debug logging for all components");
parser.addOption(verboseOption);

parser.process(app);

bool verbose = parser.isSet(verboseOption);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `qDebug() << "[Tag] ..."` | `qCDebug(category) << "..."` | Qt 5.2+ (mature since Qt 5.5) | Compile-time category filtering, runtime rule control |
| `QT_MESSAGE_PATTERN` env | `qInstallMessageHandler` + rules | Always available | Handler gives full control; env var still works as fallback |
| `QDebug::setFilterRules(QString)` | Same API, stable since Qt 5.3 | Unchanged | No API migration needed between Qt 6.4 and 6.8 |

**Compatibility notes:**
- `Q_LOGGING_CATEGORY` third argument (default severity) available since Qt 5.4 -- safe for both 6.4 and 6.8.
- `QLoggingCategory::setFilterRules()` is static, thread-safe, available since Qt 5.3.
- `QMessageLogContext` fields (`file`, `line`, `function`, `category`) all stable across Qt 6.x.
- No API differences between Qt 6.4 (dev VM) and Qt 6.8 (Pi target) for any logging features.

## Discretion Recommendations

### Tag Format: Short Brackets
**Recommendation:** Use short bracket tags like `[AA]`, `[BT]`, `[Audio]` in output, not the full dotted name `oap.aa`.
**Rationale:** Bracket tags are shorter, easier to scan in a terminal, match the existing convention users are familiar with. The dotted names are internal to Qt's filtering system.

### Log File Flag: Yes, Add It
**Recommendation:** Add `--log-file <path>` CLI option.
**Rationale:** Minimal implementation cost (redirect in message handler), high value for users who want to capture logs for bug reports without understanding journalctl.

### Library Detection: File Path + Pattern Matching
**Recommendation:** Use `QMessageLogContext::file` path check as primary (contains "open-androidauto"), bracket tag pattern as fallback.
**Rationale:** File path is reliable and doesn't break when library adds new tags. Bracket pattern catches messages where file info is stripped (release builds without `QT_MESSAGELOGCONTEXT`).

### Lifecycle Events Pass-Through
**Recommendation:** In quiet mode, let library messages through if they match lifecycle patterns: "opened", "closed", "connected", "disconnected", "session". Suppress anything containing per-message patterns: "readyRead", "bytes", "parse", hex dumps.
**Rationale:** Users troubleshooting connection issues need to see channel open/close and session state, but not per-frame protocol noise.

## Open Questions

1. **`QT_MESSAGELOGCONTEXT` in release builds**
   - What we know: In release builds, `QMessageLogContext::file`, `line`, and `function` may be empty/null unless `QT_MESSAGELOGCONTEXT` is defined. The `category` field is always populated for categorized messages.
   - What's unclear: Whether the current CMakeLists.txt defines `QT_MESSAGELOGCONTEXT` for release builds.
   - Recommendation: Check CMakeLists.txt. If not defined, add it (or accept that library detection via file path won't work in release builds and rely on bracket pattern matching instead).

2. **Library submodule category adoption**
   - What we know: Only 1 of 16 library source files uses `Q_LOGGING_CATEGORY` (`oaa.messenger.frameparser`). The rest use raw `qDebug()`/`qInfo()`.
   - What's unclear: Whether the library will adopt categories more broadly.
   - Recommendation: Design the handler to work without library categories (heuristic detection). If/when the library adopts `oaa.*` categories, the filter rules will automatically apply -- no app changes needed.

## Sources

### Primary (HIGH confidence)
- Qt 6.4 installed on dev machine (`dpkg -l qt6-base-dev` = 6.4.2) -- API availability verified
- Project source code (256 log calls across 26 files, 113 in library, 0 existing QLoggingCategory in app)
- Qt documentation: `QLoggingCategory`, `qInstallMessageHandler`, `QCommandLineParser` -- stable APIs since Qt 5.x, unchanged in Qt 6

### Secondary (MEDIUM confidence)
- `QMessageLogContext::file` behavior in release builds -- depends on build flags (verified concept, need to check project's CMake config)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Qt's built-in logging is the only sensible choice for a Qt-only project
- Architecture: HIGH - centralized categories + custom handler is the standard Qt pattern; 6 categories align with project architecture
- Pitfalls: HIGH - pitfalls are well-documented in Qt docs and verified against project code (thread model, library submodule boundary, category defaults)

**Research date:** 2026-03-01
**Valid until:** 2026-06-01 (stable Qt APIs, no churn expected)
