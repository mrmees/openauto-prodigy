# Companion + External API settings merge — design

Status: COMPLETED 2026-07-14
Date: 2026-07-14
Approved: Matthew, 2026-07-14 (option "Merge into one Companion page")

## Problem

Settings has two overlapping sections for one feature area. "Companion"
(CompanionSettings.qml) is half dead: its `companion.enabled` toggle and
"Generate Pairing Code" button drive the DISABLED legacy 9876 listener — a
user can "pair" against a service that is not running. Its Status rows are
live (migrated to `CompanionState`, API v1). "External API" (ApiSettings.qml)
holds the real toggles and the new QR+PIN pairing.

## Design

One page, one menu entry named **Companion** (pageId `api` retained — no
external deep links to `companion` exist; the ThemeService "companion" string
is an unrelated theme-source tag).

**ApiSettings.qml** becomes the merged page, three sections in order:

1. **Remote Client Pairing** — existing PIN row + Start/Cancel + QR image,
   unchanged.
2. **Phone Status** — five rows ported verbatim from CompanionSettings
   (connected indicator without the legacy button; GPS with
   `!gpsStale` visibility; battery + charging; internet proxy; route-active
   with `SystemService.routeState` colors). All bind `CompanionState`
   (ApiInboundState — survives B2) and `SystemService`; page gains a
   `hasState` guard alongside the existing `hasService`.
3. **Advanced** — existing toggles; "External API Enabled" gains sublabel
   "Powers companion, web widgets, and remote clients" (the toggle is not
   companion-only); "Allow LAN Clients" unchanged.

**Deletions:** CompanionSettings.qml (page + legacy toggle + legacy pairing
button + legacy QR dialog — all content already annotated "removed at B2",
just earlier), its two src/CMakeLists.txt registrations, and the `companion`
menu entry/component/map rows in SettingsMenu.qml. The `api` menu entry is
relabeled "Companion" and takes the phone icon (``).

**Unchanged on purpose:** `companion.enabled` config key (loses its UI knob
only; runtime behavior identical until B2 retires the namespace); the legacy
`CompanionListenerService` and its `CompanionService` context property in
main.cpp (now zero QML consumers — comment updated; B2 sweeps both).

**Docs:** `docs/reference/settings-tree.md` Companion/External API sections
rewritten to the merged page, same commit.

## Verification

App-target build (qmlcachegen catches QML syntax), full suite green,
`scripts/check-doc-links.py`, Codex gate, cross-build + Pi deploy, on-device
eyeball: menu shows single "Companion" entry; pairing + status render.
