# HUDIY Parity — Gap Analysis & Roadmap Proposal (v2)

**Date:** 2026-07-02
**Status:** DRAFT v2 — awaiting Matthew's review.
**v2 note:** v1 of this doc was written against a checkout that turned out to be **1407 commits behind origin** and has been discarded. This version is grounded against origin/main at `0b91e3d` (v0.7.0 milestone, phase 28 planning).

## 1. Goal & Framing

Bring OpenAuto Prodigy to functional parity with HUDIY (hudiy.eu, Wiboma Sp. z o.o.) — the closed-source paid successor to OpenAuto Pro.

**Working goal: personal first, community-ready** — prioritized by what Matthew will use in the Miata (Pi 4, DFRobot 1024×600), architected so prodigy can be the open-source HUDIY alternative. "Parity" means the capabilities that make HUDIY compelling, not checkbox-cloning its marketing page.

This doc **feeds the project's existing governance**: new items below are wishlist candidates (`docs/wishlist.md`), to be promoted into `docs/roadmap-current.md` when committed. It does not replace either.

## 2. Reference Materials & Licensing Stance

- **HUDIY web pages (saved):** `E:\tmp\hudiy` — marketing + feature overview.
- **HUDIY GitHub repo (cloned):** `personal/openautopro/hudiy-reference/` — wiboma/hudiy.

The wiboma/hudiy repo contains **no application source** — only the integration surface: `api/Api.proto` (826 lines, ~50 message types), JSON config schemas, HTML/JS examples, i18n, docs. The application is closed. **The repo has no LICENSE file → all rights reserved.** Consequences:

1. Reading their public docs to understand capabilities is fine (facts/ideas aren't protected).
2. Do **not** copy their `.proto`, config JSON, or example code into prodigy or open-android-auto.
3. Our external API, config formats, and any JS bridge are original designs. HUDIY wire-compatibility is a non-goal.

## 3. Where Prodigy Already Stands (vs. HUDIY)

Prodigy has independently converged on much of HUDIY's architecture — and exceeds it in places (community protocol repo, web config panel, prebuilt installer).

| HUDIY capability | Prodigy status |
|---|---|
| Wireless Android Auto | **DONE** — BT discovery → WiFi → TCP verified end-to-end on clean install; multi-codec video (H.264/H.265/VP9/AV1), hw/sw decode, DMA-BUF |
| Touch + gestures | **DONE** — evdev MT with auto-discovery, letterbox-aware mapping, 3-finger gesture |
| Audio engine (PipeWire) | **DONE** — 3 streams (media/nav/phone) + audio focus |
| BT A2DP/AVRCP + pairing UI | **DONE** — BlueZ D-Bus, PairedDevicesModel, PairingDialog, auto-reconnect |
| Hands-free phone (HFP) | **PARTIAL** — dialer + incoming-call overlay done; call *audio* routing is a current "Now" roadmap item |
| 15-band EQ + presets | **IN PROGRESS** — `src/plugins/equalizer/` exists; "Now" roadmap item (completeness unverified) |
| Settings UI | **DONE** — plus Flask web config panel (HUDIY has no web panel), though panel is currently broken (wishlist bug) |
| Native extensibility | **DONE (different approach)** — C++ plugin system (static + dynamic `.so` + `plugin.yaml`), typed dashboard contributions, provider interfaces |
| Install/distribution | **DONE** — interactive installer + prebuilt release tarballs |
| Companion app | **EXISTS (different focus)** — [mrmees/openauto-companion](https://github.com/mrmees/openauto-companion) (Kotlin): GPS, time, battery, internet sharing over WiFi, plus a theme builder with wallpaper crop and palette transfer. HUDIY's companion does phone notifications + time sync over BT; notification display is the remaining gap |
| Actions system | **PLANNED** — `docs/plans/active/2026-02-21-architecture-extensibility-plan.md` (EventBus + ActionRegistry + notifications + config-driven launcher), explicitly "hudiy-style extensibility," status NOT STARTED; likely needs rebasing against the completed v0.6 refactor |
| OBD-II, reverse camera, GPIO | **PLANNED** — roadmap "Later" (plugin system expansion) |
| Theme engine / user theme selection | **PLANNED** — roadmap "Later," scope undefined |
| Multi-display / resolutions | **PLANNED** — roadmap "Later" |
| USB AA, CarPlay, non-Pi-4 hardware | **DEFERRED** — explicitly out of scope in roadmap-current |

## 4. Genuine Gaps (HUDIY has it; prodigy has nothing planned)

1. **External API** — HUDIY exposes protobuf over TCP + WebSocket: status streams (media/nav/projection/phone), action dispatch, notifications/toasts, theme switching, overlay control, EQ presets, OBD queries, cover-art injection. Prodigy's only comparable surface is the local Unix-socket IPC for the web config panel — a seed, but not an external API. This is HUDIY's biggest integration moat, and prodigy is well positioned: protobuf already in-stack, and the API could become part of the open-android-auto community story. Original schema design (see §2).
2. **User-composable dashboards** — prodigy's widget/dashboard machinery (`WidgetRegistry`, `DashboardContributionKind`) is plugin-facing, not user-facing. HUDIY lets users arrange widgets (2 widths × 3 heights) into multiple dashboards via config.
3. **Overlay framework** — prodigy has purpose-built overlays (incoming call, pairing, gesture); HUDIY has a general system: user-defined overlays with position/size config, drag-to-move, visibility via actions/API, split-screen layouts.
4. **HTML/JS custom content** — HUDIY's headline extensibility: Chromium web views as widgets/apps/overlays with a `hudiy` JS object (theme, input, API, Media Session). Prodigy equivalent would be Qt WebEngine + a `prodigy` JS bridge. Needs a Pi 4 memory/perf spike before committing. Prodigy's native plugin SDK is arguably the better foundation; HTML/JS is the low-floor community on-ramp.
5. **Local file media playback** — HUDIY plays local files with metadata/cover art. Prodigy has BT audio only; no media player plugin.
6. **FM radio** — RTL-SDR + RDS decoding in HUDIY. Nothing in prodigy. *(Decided: nice-to-have, deferred to long tail.)*
7. **Companion notifications** — HUDIY's companion displays phone notifications on the head unit. Prodigy's companion app exists (see §3) but doesn't do notifications; depends on the head-unit notification service from the extensibility plan (Priority 3).
8. **Key-event navigation map** — HUDIY has full keyboard/button bindings (focus movement, media keys, projection focus toggle). Prodigy is touch-first; unverified whether any key bindings exist. Relevant for steering-wheel buttons via GPIO/keyboard HID.

## 5. Sequencing (decided 2026-07-02: interleaved with v0.7.0)

Parity work is **interleaved** with the v0.7.0 Kiosk milestone rather than queued behind it — alternate between kiosk phases and parity items as motivation dictates, keeping reliability work moving.

1. **Finish the "Now" items** — HFP call audio, equalizer. Both are already parity items.
2. **Revive the extensibility plan** (actions/EventBus/notifications/config-driven launcher) — rebase it against post-v0.6 code first; parts of Priority 1/3 may already be superseded. This is the substrate for dashboards, overlays, the API, and the JS bridge.
3. **HTML/JS runtime spike** *(new, pulled early per decision)* — Qt WebEngine memory/perf on Pi 4 go/no-go. Matthew wants HTML/JS as a primary way to develop new features going forward, so this spike gates architecture decisions and runs early — cheap, informative, parallel-friendly.
4. **External API v1** *(new)* — original protobuf schema over TCP + WebSocket; status streams + action dispatch + notifications first. **Prodigy-private** (not under the open-android-auto umbrella). Also the backbone the JS bridge talks to.
5. **HTML/JS runtime proper** *(new, if spike passes)* — WebEngine widgets/apps/overlays with a `prodigy` JS object (theme tokens, input events, API access).
6. **User-composable dashboards + overlay framework** *(new)* — grow the existing widget registry into user-facing config; generalize the overlay pattern (position/size/visibility via actions + API). HTML/JS widgets become first-class dashboard citizens here.
7. **Media player plugin** *(new)* — local files via Qt Multimedia, metadata + cover art, now-playing integration with existing MediaStatusService.
8. **OBD-II plugin** *(already "Later")* — ELM327 (BT/USB serial), PID polling, gauge widgets onto the new dashboards. Personal kicker: MS3Pro research spike (CAN broadcast / TunerStudio serial vs. OBD-II-over-CAN) for real boost/AFR/IAT gauges in the Miata.
9. **Theme engine** *(already "Later")* — when scoped, consider HUDIY's model: dark/light with user source color + contrast, Material-3-style tonal palette generation, per-menu backgrounds, icon fonts. The companion app's theme builder is the natural front-end.
10. **Long tail** — FM radio (RTL-SDR + RDS, explicitly deferred), reverse camera, key-binding map, companion notifications, multi-display.

Items 3–7 and the long-tail additions are new wishlist entries; the rest already exist in project planning in some form.

## 6. Non-Goals

- HUDIY wire/config compatibility (licensing + freedom)
- CarPlay, USB AA, non-Pi-4 hardware (already deferred by roadmap-current)
- DRM media support

## 7. Risks

| Risk | Mitigation |
|---|---|
| Extensibility plan is stale vs. v0.6 refactor | Rebase/re-review the plan before executing; don't run it blind |
| Qt WebEngine too heavy on Pi 4 | Spike before committing; native plugin SDK remains the primary extensibility story |
| API scope creep (~50 HUDIY message types) | v1 = status streams + actions + notifications only; grow by demand |
| Parity work starves v0.7.0 reliability milestone | Wishlist-then-promote governance; finish kiosk milestone unless Matthew reprioritizes |
| This doc goes stale like v1 did | It feeds wishlist/roadmap-current and then dies; those stay canonical |

## 8. Decisions (Matthew, 2026-07-02)

1. **Interleave** parity work with the v0.7.0 kiosk milestone.
2. External API stays **prodigy-private** — not part of the open-android-auto umbrella.
3. HTML/JS runtime matters: it's intended as a primary path for developing new features going forward → spike pulled early (§5.3).
4. FM radio: nice-to-have, **deferred** to long tail.
5. Companion app **already exists** ([mrmees/openauto-companion](https://github.com/mrmees/openauto-companion)) — GPS/time/battery/internet sharing + theme builder. Remaining parity gap is notification display, dependent on the head-unit notification service.
