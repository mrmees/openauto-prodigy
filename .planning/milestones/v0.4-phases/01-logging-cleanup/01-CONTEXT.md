# Phase 1: Logging Cleanup - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace debug spam with categorized, quiet-by-default logging. Production output is clean by default; debug detail available on demand via CLI flag, config file, or web panel. Does not add new features — only restructures how existing log output is managed.

</domain>

<decisions>
## Implementation Decisions

### Category design
- Per-subsystem categories (~6 categories): AA, BT, Audio, Plugin, UI, Core
- Consolidate existing manual prefix tags (e.g., [AAOrchestrator], [BluetoothDiscovery], [VideoDecoder] all become `oap.aa`)
- Dotted hierarchy naming convention: `oap.aa`, `oap.bt`, `oap.audio`, `oap.plugin`, `oap.ui`, `oap.core`
- Enables Qt wildcard filtering: `oap.aa.*=true`
- Default (quiet) level shows key lifecycle events (app started, AA connected/disconnected, BT paired) plus warnings — a few lines per session, enough to know what happened

### Verbose activation
- CLI flag (`--verbose` or `--debug`) AND persistent config file setting
- Per-category control in config (e.g., `debug_categories: [aa, bt]`) for targeted debugging
- Runtime toggle via web config panel — changes take effect immediately via IPC, no restart required
- `--verbose` includes open-androidauto library output (one switch for full diagnostic mode)

### Log output format
- Include millisecond timestamps in app output (useful when running from terminal or redirecting to file, not just journalctl)
- Thread names/IDs included in verbose output (relevant for ASIO vs Qt main thread debugging)
- Category tag format and file output option: Claude's discretion

### Submodule boundary
- Qt filter rules to suppress open-androidauto library output by default
- Custom Qt message handler (`qInstallMessageHandler()`) for consistent formatting and filtering of all output including library
- Design naming convention (`oap.lib.*`) that the library could adopt later — filter rules would automatically work
- In quiet mode, allow major library connection events through (session start/stop, channel open/close) but suppress per-message protocol chatter

### Claude's Discretion
- Exact category tag format in output (short bracket vs full dotted name)
- Whether to add optional `--log-file` flag for file output
- Custom message handler formatting details
- How to detect and classify library messages without categories (pattern matching, source file, etc.)
- Mapping of existing ~15 manual prefix tags to the 6 subsystem categories

</decisions>

<specifics>
## Specific Ideas

- Runtime verbose toggle via web panel is important for remote debugging without SSH — a real daily-driver scenario
- Library protocol chatter (per-message debug) should be fully suppressed in quiet mode, but connection lifecycle events are useful for diagnosing "why didn't my phone connect" issues
- Per-category config control means a user can leave `debug_categories: [aa]` in their config to always capture AA debug without flooding other subsystems

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `IpcServer` (Unix socket): Already handles JSON request/response between web panel and app — runtime verbose toggle can use this channel
- `ConfigService` (YamlConfig): Already supports deep merge and config read/write — persistent logging settings fit naturally
- Existing manual prefix tags (~15 unique tags): Provide a mapping guide for consolidation into 6 categories

### Established Patterns
- All logging uses Qt's `qDebug()`/`qInfo()`/`qWarning()`/`qCritical()` — no third-party logging frameworks
- Error handling follows graceful degradation pattern — services fail silently with qWarning, plugins fail with qCritical + signal
- Services injected via `IHostContext*` — new logging configuration can flow through the same mechanism

### Integration Points
- `src/main.cpp`: Entry point where `qInstallMessageHandler()` and CLI arg parsing would go
- `web-config/server.py`: Flask routes for the web panel — needs new logging toggle endpoint
- `src/core/services/IpcServer.cpp`: IPC handler for runtime commands from web panel
- Config file (`~/.openauto/config.yaml`): Persistent logging settings
- Every `.cpp` file with log calls (~43 files): Needs migration from raw qDebug/qInfo to QLoggingCategory macros

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-logging-cleanup*
*Context gathered: 2026-03-01*
