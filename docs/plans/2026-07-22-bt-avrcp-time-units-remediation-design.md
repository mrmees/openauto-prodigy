# Bluetooth AVRCP Time-Unit Remediation — Design

Status: ACTIVE

**Date:** 2026-07-22
**Grounded against:** `origin/main` at `19ec68f6`
**Publication:** one standalone branch and draft pull request

## 1. Outcome

Preserve BlueZ `org.bluez.MediaPlayer1` `Position` and `Track.Duration`
values as milliseconds from D-Bus ingestion through the Bluetooth view,
`MediaStatusService`, shared now-playing surfaces, and External API media
status. Initial player adoption and later property changes use the same unit,
type, validity, and notification contract.

This is a bounded Bluetooth media-state correction. It does not change A2DP
audio routing, AVRCP controls, Android Auto, telephony, public protobuf
definitions, or QML presentation logic.

## 2. Authoritative Contract and Root Cause

BlueZ 5.82 documents `MediaPlayer1.Position` and the `Duration` member of
`MediaPlayer1.Track` as `uint32` milliseconds. The target Pi's installed
BlueZ 5.82 manual states the same contract, and BlueZ converts MPRIS
microseconds to milliseconds before publishing AVRCP state.

`BtAudioPlugin::updatePlayerProperties()` currently describes both values as
microseconds and divides each by 1000. A duration of 215000 ms therefore
becomes 215 ms, and a position of 61000 ms becomes 61 ms. The same function is
used by startup enumeration, player adoption, and later
`PropertiesChanged`, so every delivery path is affected.

Two same-contract propagation defects are included:

- duration is assigned only when title, artist, or album changes, so a
  duration-only Track update remains stale;
- Bluetooth monitoring starts before `MediaStatusService` wiring, and the
  composition-root catch-up path seeds metadata and playback state but not
  progress, so correctly adopted startup values can remain absent downstream.

## 3. Decisions Locked

- **Fix the BlueZ boundary.** No consumer receives a compensating unit
  conversion. BlueZ milliseconds remain milliseconds.
- **Preserve the full D-Bus type range.** Store times as `qint64`; valid BlueZ
  `uint32` values cannot wrap through signed `int` conversion.
- **Accept both QtDBus shapes.** `Track` may arrive as an already-converted
  `QVariantMap` or an unconverted `QDBusArgument`. The latter is manually
  demarshaled with `beginMap()`/`endMap()` and `QDBusVariant`, as required by
  `src/AGENTS.md`.
- **Share player adoption.** Startup `GetManagedObjects` and hot
  `InterfacesAdded` call one player-adoption helper. The carried
  `InterfacesAdded` property map is authoritative; the existing synchronous
  read-back remains only as a missing-payload fallback.
- **Duration is independent metadata.** A changed `Duration` is applied even
  when title, artist, and album are unchanged.
- **Notifications describe observable changes.** Metadata, duration, and
  position property notifications are independent and edge-only. One coherent
  progress notification is emitted after every changed field in a delivered
  batch has been assigned, so downstream readers never observe a half-updated
  pair.
- **Unknown values remain safe.** Direct Bluetooth UI time properties retain
  their existing zero-safe behavior. Shared media state retains `-1` for an
  unknown position and `0` for an unknown duration until valid values have
  been reported. Missing fields do not invent values; explicitly invalid time
  fields return that field to its unknown/zero state.
- **Seed downstream state.** Initial and connection catch-up paths publish the
  current valid progress snapshot to `MediaStatusService`; later coherent
  progress notifications use the same path.
- **Consumers remain unchanged.** The Bluetooth view formats milliseconds by
  dividing by 1000 only when rendering seconds. Progress ratios are
  unit-neutral. Shared widgets and External API serialization already consume
  `qint64` milliseconds.
- **Document the owner boundary.** The architecture map records BlueZ
  millisecond ingestion and unchanged downstream propagation.

## 4. Delivery Contract

1. BlueZ exposes a `MediaPlayer1` object through startup enumeration or
   `InterfacesAdded`.
2. The player path and carried properties are adopted through one helper.
3. `Track` is demarshaled from either supported QtDBus representation.
4. Valid `uint32` Duration and Position values are widened to `qint64` without
   scaling.
5. All changed fields are assigned before edge-only property notifications and
   one coherent progress notification are emitted.
6. Sender-filtered `PropertiesChanged` updates use the same parser and update
   rules.
7. `main.cpp` seeds already-adopted progress after constructing
   `MediaStatusService`, republishes it on Bluetooth connection catch-up, and
   forwards later coherent progress updates directly.
8. `MediaStatusService`, QML, shared widgets, and External API retain the same
   millisecond contract.

## 5. Acceptance Matrix

| Concern | Automated proof | Live proof |
|---|---|---|
| Unit preservation | Adopt and update Duration=215000 and Position=61000; assert exact values | Compare BlueZ properties with Bluetooth/shared/API values for a known track |
| Startup/adoption | Drive the real `onInterfacesAdded` slot with player properties and verify the shared adoption helper | Start the app while an AVRCP player already exists, if naturally available |
| Later delivery | Drive sender-filtered `PropertiesChanged` through the meta-object | Observe advancing Position during playback |
| Duration-only update | Same metadata, changed Duration updates once | Track/player change where duration arrives separately, if naturally observed |
| Types/overflow | Exercise `uint32` values above `INT_MAX` and assert `qint64` preservation | No special long-duration media required |
| Notifications | Exact duration, position, and coherent-progress signal counts for changed and repeated values | No duplicate-event instrumentation required |
| Downstream status | Connect coherent plugin progress to `MediaStatusService`; assert exact millisecond state | Compare shared widget and External API status |
| Consumers | Serializer regression and static inspection of existing QML ratios/formatting | Correct labels and progress ratio on the Pi |

## 6. Explicitly Out of Scope

- Logging configuration registration or persistence
- Playback-state reset when a MediaPlayer1 object is removed
- General conversion of blocking D-Bus reads to asynchronous calls
- A2DP transport selection, sender filtering, multi-transport policy, playback
  controls, EQ tap behavior, focus arbitration, or PipeWire routing
- QML layout, formatting, or visual redesign
- Android Auto protocol or transport code
- Any file under `libs/prodigy-oaa-protocol/proto/`
- `proto/api/`, External API schema changes, or JS shim changes
- Bluetooth daemon restart, re-pairing, HFP testing, or unrelated Pi changes
- Tagging, release publication, or an unapproved Pi deployment

New discoveries follow wishlist-then-promote and do not expand this tranche.

## 7. Verification and Live Policy

The implementation runs focused Bluetooth, media-status, and API serializer
tests, then the full local build, explicit `openauto-prodigy` target, full
CTest suite, documentation links, `git diff --check`, and the repository
review gate with every finding adjudicated.

After the repository gate, `./cross-build.sh` produces the candidate aarch64
binary. Any Pi deployment, application restart, Bluetooth playback operation,
or other live state change remains a separate Matthew approval boundary.

Required approved live validation compares BlueZ Duration/Position with the
Bluetooth view, shared media state, and External API for a known multi-minute
track. After deployment it also verifies one application process, responsive
IPC, normal A2DP playback, correct progress/time display, and unchanged
hostapd/Bluetooth PIDs. Pause/resume, track change, and a second player are
optional. Bluetooth restart, re-pairing, HFP calls, and AA capture are not
required.
