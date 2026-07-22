# Android Auto Night Initial-State Remediation — Design

Status: COMPLETED 2026-07-22

**Date:** 2026-07-22
**Grounded against:** `origin/main` at `e81c0686`
**Publication:** one standalone branch and draft pull request

## 1. Outcome

Make the first Android Auto NIGHT_DATA indication report the authoritative
provider state even when that state was established before the sensor channel
opened or the phone subscribed. Preserve normal later transitions and deliver
the latest state again after channel close, reopen, and resubscription.

This is a bounded AA sensor-delivery correction. It changes the in-tree sensor
handler outside the protocol-definition submodule and the orchestrator's
provider seeding. It does not change protobuf definitions or public APIs.

## 2. Root Cause

The selected provider starts before the AA handshake and may establish or emit
the correct state while the sensor channel is closed. `SensorChannelHandler`
currently discards that update because `pushNightMode()` returns before storing
it. When NIGHT_DATA is later subscribed, the handler sends a hardcoded day
value. A phone connecting at night can therefore remain in day presentation
until the provider observes a later real transition.

## 3. Decisions Locked

- **Provider state is independent of wire readiness.** The sensor handler
  retains the latest night value even while the channel is closed or NIGHT_DATA
  is unsubscribed.
- **Wire gating remains strict.** Retaining an update does not send it before
  both channel open and NIGHT_DATA subscription.
- **Subscription uses the snapshot.** The initial NIGHT_DATA indication is
  encoded from the retained value, never a hardcoded default.
- **The orchestrator seeds explicitly.** After starting the selected provider
  and before starting the AA session, the orchestrator pushes
  `NightModeProvider::isNight()` into the handler. Correctness does not depend
  on an initial transition signal.
- **Close clears subscriptions, not state.** Channel close/reopen preserves the
  latest night snapshot so resubscription receives it.
- **Other sensor defaults stay unchanged.** Driving status remains unrestricted
  and parking brake remains engaged on initial subscription.
- **Current behavior documentation changes with its owner.** The architecture
  map records the provider-to-handler snapshot and subscription lifecycle.

## 4. Lifecycle Contract

1. The orchestrator creates the configured time or GPIO provider.
2. Provider transitions are connected to the sensor handler.
3. The provider starts and evaluates its current state.
4. The orchestrator explicitly seeds the handler from `isNight()`.
5. The AA session starts; pre-open updates are retained without transmission.
6. Channel open clears the prior subscription set but preserves night state.
7. NIGHT_DATA subscription receives start OK followed by the cached state.
8. Later transitions update the cache and transmit while subscribed.
9. Channel close clears subscriptions while preserving the latest cache.

## 5. Acceptance Matrix

| Concern | Automated proof | Live proof |
|---|---|---|
| Initial state | Decode `SensorEventIndicationMessage` for seeded day and night values | Capture and decode the first outgoing indication in forced day and forced night sessions |
| Pre-open/subscription updates | Prove retention with zero premature `sendRequested` emissions | Covered by decoded initial indication; protocol capture must show no earlier sensor indication |
| Transitions | Decode later day/night emissions | Immediate phone presentation is secondary confirmation |
| Reconnect | Close/reopen/resubscribe test receives the latest state | Optional unchanged-state reconnect reinforcement |
| Non-night sensors | Decode unchanged driving-status and parking-brake defaults | No dedicated live row required |

## 6. Explicitly Out of Scope

- Bluetooth AVRCP duration or position units
- Logging configuration registration or persistence
- Night-source selection redesign, including the existing `none` behavior
- ThemeNightMode activation or force-dark semantics
- GPIO subsystem modernization or physical GPIO bench coverage
- Any file under `libs/prodigy-oaa-protocol/proto/`
- `proto/api/`, External API, JS shim, dashboards, overlays, or frozen numerics
- USB Android Auto, HFP roles, ofono, audio routing, codecs, or QML
- General AA session teardown or duplicate-connection redesign
- Tagging, release publication, or an unapproved Pi deployment

## 7. Verification Policy

The implementation task runs decoded handler regressions, the existing night
provider and AA integration tests, the full local build, the explicit
`openauto-prodigy` target, and the full CTest suite. Documentation links,
`git diff --check`, forbidden-path checks, and the repository review gate must
pass before cross-build.

Pi validation is a separate approval boundary. Closure requires approved live
day and night captures decoded from the first NIGHT_DATA indication. If live
validation is not approved, the plan remains ACTIVE and the work is not called
complete.
