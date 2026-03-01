---
status: resolved
trigger: "open-androidauto library output not filtered in quiet mode - [AASession] RX ch X msgId Y len Z floods log as [D] [App]"
created: 2026-03-01T00:00:00Z
updated: 2026-03-01T00:30:00Z
---

## Current Focus

hypothesis: The bracket tag check in isLibraryMessage() doesn't match [AASession] because it's not in the g_libraryTags list
test: Compare g_libraryTags entries against actual tags used by open-androidauto
expecting: [AASession] is missing from the list
next_action: Verify and enumerate all missing tags

## Symptoms

expected: In quiet mode, library debug messages like "[AASession] RX ch X msgId Y len Z" should be suppressed
actual: These messages flood the log output as "[D] [App]" during active AA sessions
errors: No errors - just log spam
reproduction: Start AA session, observe log output in quiet mode
started: Since quiet mode filtering was implemented

## Eliminated

(none yet)

## Evidence

- timestamp: 2026-03-01T00:01:00Z
  checked: How open-androidauto emits logs
  found: Library uses bare qDebug()/qInfo()/qWarning() with NO logging category - just bracket-tagged strings like "[AASession] ..."
  implication: Messages hit Qt's "default" category (nullptr/empty), which shortTag() maps to "App". The oaa.* category prefix check won't match. Only the bracket tag heuristic in isLibraryMessage() can catch these.

- timestamp: 2026-03-01T00:02:00Z
  checked: g_libraryTags list vs actual library tags
  found: |
    g_libraryTags has: [TCPTransport], [ControlChannel], [Messenger], [Session], [MediaChannel], [InputChannel], [SensorChannel], [AudioChannel], [VideoChannel], [BluetoothChannel], [WiFiChannel], [AVInputChannel], [FrameParser], [CryptoLayer]
    Actual tags in library: [AASession], [TCPTransport], [ControlChannel], [BluetoothChannel], [AudioChannel], [AVInputChannel], [VideoChannel], [SensorChannel], [WiFiChannel], [PhoneStatusChannel], [NavChannel], [MediaStatusChannel], [InputChannel]
    MISSING from g_libraryTags: [AASession], [PhoneStatusChannel], [NavChannel], [MediaStatusChannel]
    IN g_libraryTags but NOT used by library: [Session], [MediaChannel], [FrameParser], [CryptoLayer]
  implication: [AASession] is the PRIMARY high-volume tag (every RX message) and it's missing from the filter list. Three other tags are also missing.

- timestamp: 2026-03-01T00:03:00Z
  checked: Why ctx.file path check doesn't catch these either
  found: Qt's QMessageLogContext only populates file/line/function when QT_MESSAGELOGCONTEXT is defined (typically only in debug builds). In release builds, ctx.file is nullptr.
  implication: The file path heuristic ("open-androidauto" in file path) is unreliable - may work in dev builds but not production. The bracket tag check is the real fallback.

- timestamp: 2026-03-01T00:04:00Z
  checked: Messenger.cpp and FrameAssembler.cpp log format
  found: These two files use bare format WITHOUT bracket tags - e.g. "Messenger: assembled message too short" and "FrameAssembler: duplicate FIRST on channel"
  implication: These messages would also escape filtering entirely since they match no heuristic

## Resolution

root_cause: |
  The g_libraryTags[] array in Logging.cpp is incomplete. It's missing [AASession] (the highest-volume tag during active sessions), plus [PhoneStatusChannel], [NavChannel], and [MediaStatusChannel]. Additionally, some entries in the list ([Session], [MediaChannel], [FrameParser], [CryptoLayer]) don't match any actual library output - they appear to be guesses rather than verified tags.

  Secondary issue: Messenger.cpp and FrameAssembler.cpp use a different format ("Messenger: ...", "FrameAssembler: ...") without brackets, so they bypass all heuristics.

fix: |
  Already applied in commit b1a0dca:
  1. Removed stale tags: [Session], [MediaChannel], [FrameParser], [CryptoLayer]
  2. Added missing tags: [AASession], [PhoneStatusChannel], [NavChannel], [MediaStatusChannel]
  3. Added g_libraryPrefixes[] array for "Messenger:" and "FrameAssembler:" colon-format messages
  4. Added startsWith check in isLibraryMessage() for colon-prefixed patterns
  Follow-up commit 7aa0133 added bracket-tag stripping before lifecycle keyword check.
verification: test_logging passes (1/1). Code already committed to main.
files_changed:
  - src/core/Logging.cpp
