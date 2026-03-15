---
status: resolved
trigger: "--verbose mode shows no extra debug output"
created: 2026-03-01T00:00:00Z
updated: 2026-03-01T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED - log migration converted nearly all qCDebug to qCInfo
test: counted qCDebug vs qCInfo across src/
expecting: qCDebug should be majority of operational messages
next_action: report root cause

## Symptoms

expected: --verbose shows [D] debug lines with per-operation detail
actual: Only [I], [W], [C] lines — identical to quiet mode
errors: none — flag is recognized, filter rules applied correctly
reproduction: launch with --verbose, observe output
started: after log migration (phase 01-02)

## Eliminated

(none needed — hypothesis confirmed on first test)

## Evidence

- timestamp: 2026-03-01
  checked: qCDebug vs qCInfo counts across src/
  found: 16 qCDebug, 158 qCInfo, 59 qCWarning, 27 qCCritical
  implication: 90%+ of operational messages are Info level — verbose filter enables Debug but there's almost nothing at Debug level

- timestamp: 2026-03-01
  checked: Logging.cpp setVerbose() implementation
  found: correctly calls QLoggingCategory::setFilterRules("oap.*.debug=true\noaa.*.debug=true\n") — the plumbing works fine
  implication: the filter correctly enables debug messages, but there are only 16 debug calls to enable

- timestamp: 2026-03-01
  checked: categories defined with QtInfoMsg threshold (Logging.cpp lines 9-14)
  found: Q_LOGGING_CATEGORY(lcAA, "oap.aa", QtInfoMsg) — all 6 categories default to Info threshold
  implication: correct — quiet mode suppresses debug. But verbose enables it and there's still nothing to show.

- timestamp: 2026-03-01
  checked: which files have chatty per-operation qCInfo that should be qCDebug
  found: EvdevTouchReader.cpp (19 qCInfo — per-slot DOWN/UP, grab/ungrab, resolution updates), BluetoothDiscoveryService.cpp (16 qCInfo — per-message hex dumps, proto debug strings), VideoDecoder.cpp (15 qCInfo — per-packet codec detection, per-frame perf), BluetoothManager.cpp (38 qCInfo — per-attempt auto-connect, per-property-change), AndroidAutoOrchestrator.cpp (22 qCInfo — per-watchdog-poll TCP state)
  implication: these are the messages that spam at Info level in quiet mode AND provide no differentiation when verbose is enabled

## Resolution

root_cause: The log migration (phase 01-02) converted virtually all qDebug() calls to qCInfo() instead of properly triaging between qCInfo() (significant events) and qCDebug() (per-operation/per-packet detail). Result: 158 qCInfo vs 16 qCDebug. The verbose flag correctly enables debug-level output via QLoggingCategory filter rules, but there are almost no qCDebug calls to surface.

fix: Downgrade per-operation/per-packet/per-frame messages from qCInfo to qCDebug

verification: n/a — diagnosis only

files_changed: []
