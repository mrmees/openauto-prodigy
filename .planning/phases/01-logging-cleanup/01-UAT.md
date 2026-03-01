---
status: diagnosed
phase: 01-logging-cleanup
source: [01-01-SUMMARY.md, 01-02-SUMMARY.md]
started: 2026-03-01T23:00:00Z
updated: 2026-03-01T23:30:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Default Quiet Output
expected: Launch app normally. Output should be minimal — no debug spam, only info/warning level messages with lifecycle events. No raw untagged qDebug lines.
result: pass

### 2. Verbose CLI Flag
expected: Launch with `--verbose` flag. Output should show detailed debug lines with millisecond timestamps, category bracket tags like [AA], [BT], [Audio], [Plugin], [UI], [Core], and thread names.
result: issue
reported: "verbose flag is parsed and confirmed (shows 'Verbose logging enabled (CLI)' with thread name) but no [D] debug lines appear — only [I], [W], [C] same as quiet mode. Migration converted nearly all qDebug calls to qCInfo instead of qCDebug, so verbose has nothing extra to show."
severity: major

### 3. Log File Output
expected: Launch with `--log-file /tmp/prodigy-test.log`. Log output should be written to that file. Check with `cat /tmp/prodigy-test.log` after running for a few seconds.
result: pass

### 4. Web Panel Logging Controls
expected: Open the web config panel in a browser. The settings page should have a Logging card with a verbose toggle switch and 6 category checkboxes (AA, BT, Audio, Plugin, UI, Core).
result: skipped
reason: Web panel is broken (pre-existing issue, not phase 1 regression)

### 5. Runtime Verbose Toggle via Web
expected: With the app running, toggle verbose ON from the web panel. Log output should immediately become detailed without restarting. Toggle OFF — output should go quiet again.
result: skipped
reason: Web panel is broken (pre-existing issue, not phase 1 regression)

### 6. Per-Category Debug via Web
expected: In the web panel, leave verbose OFF but check just one or two category boxes (e.g., AA, BT). Only those categories should show debug output. Others stay quiet.
result: skipped
reason: Web panel is broken (pre-existing issue, not phase 1 regression)

### 7. Settings Persistence
expected: Change logging settings (e.g., enable verbose or select categories) via the web panel. Restart the app. The logging settings should survive the restart — same verbosity level on relaunch.
result: skipped
reason: Web panel is broken — no UI to change settings at runtime

### 8. Library Output Filtering
expected: In default (quiet) mode, open-androidauto library debug output should be suppressed. In verbose mode, library messages should appear. Major library lifecycle events (connections, sessions) should show even in quiet mode.
result: issue
reported: "Library output filtering not working during active AA session. [AASession] RX and [TCPTransport] spam floods quiet-mode output. Messages tagged [D] [App] — hitting Qt default category instead of being recognized as library messages. isLibraryMessage() heuristic not catching them."
severity: major

## Summary

total: 8
passed: 2
issues: 2
pending: 0
skipped: 4

## Gaps

- truth: "Verbose mode shows detailed debug lines beyond what quiet mode shows"
  status: failed
  reason: "User reported: verbose flag is parsed and confirmed but no [D] debug lines appear — migration converted nearly all qDebug to qCInfo instead of preserving severity levels"
  severity: major
  test: 2
  root_cause: "Migration blanket-converted ~250 qDebug() to qCInfo() instead of triaging. 158 qCInfo vs 16 qCDebug. Verbose plumbing works but has nothing to unmask."
  artifacts:
    - path: "src/core/services/BluetoothManager.cpp"
      issue: "~38 qCInfo calls, most should be qCDebug (per-attempt, per-property-change)"
    - path: "src/core/aa/AndroidAutoOrchestrator.cpp"
      issue: "~22 qCInfo calls, per-watchdog-poll and per-step lifecycle should be qCDebug"
    - path: "src/core/aa/EvdevTouchReader.cpp"
      issue: "~19 qCInfo calls, per-slot touch events should be qCDebug"
    - path: "src/core/aa/BluetoothDiscoveryService.cpp"
      issue: "~16 qCInfo calls, hex dumps and per-message parsing should be qCDebug"
    - path: "src/core/aa/VideoDecoder.cpp"
      issue: "~15 qCInfo calls, per-packet codec sniffing and per-frame stats should be qCDebug"
    - path: "src/core/services/AudioService.cpp"
      issue: "~10 qCInfo calls, per-stream and buffer growth should be qCDebug"
  missing:
    - "Audit each qCInfo — keep one-time events as Info, downgrade per-packet/per-frame/per-retry to Debug"
  debug_session: ".planning/debug/verbose-no-debug-output.md"

- truth: "Library debug output is suppressed in quiet mode during active AA session"
  status: failed
  reason: "User reported: [AASession] RX spam floods quiet-mode output, tagged [D] [App] — library messages not recognized by isLibraryMessage() heuristic"
  severity: major
  test: 8
  root_cause: "g_libraryTags[] built from guesses, not library audit. Missing [AASession], [PhoneStatusChannel], [NavChannel], [MediaStatusChannel]. Has 4 phantom entries matching nothing real. Also missing colon-prefixed patterns (Messenger:, FrameAssembler:)."
  artifacts:
    - path: "src/core/Logging.cpp"
      issue: "g_libraryTags[] at line 23-30 incomplete — missing 4 real tags, has 4 phantom tags"
  missing:
    - "Replace tag list with verified set from library source audit"
    - "Add colon-prefixed patterns (Messenger:, FrameAssembler:)"
  debug_session: ".planning/debug/oaa-log-quiet-mode.md"
