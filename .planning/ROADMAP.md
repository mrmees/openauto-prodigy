# Roadmap: OpenAuto Prodigy

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2015-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

<details>
<summary>v0.4.1 Audio Equalizer (Phases 1-3) - SHIPPED 2015-03-02</summary>

See .planning/milestones/v0.4.1/ for archived details.

</details>

<details>
<summary>v0.4.2 Service Hardening (Phases 1-3) - SHIPPED 2015-03-02</summary>

See .planning/milestones/v0.4.2/ for archived details.

</details>

<details>
<summary>v0.4.3 Interface Polish & Settings Reorganization (Phases 1-4) - SHIPPED 2015-03-03</summary>

See .planning/milestones/v0.4.3/ for archived details.

</details>

<details>
<summary>v0.4.4 Scalable UI (Phases 1-5) - SHIPPED 2015-03-03</summary>

See .planning/milestones/v0.4.4/ for archived details.

</details>

<details>
<summary>v0.4.5 Navbar Rework (Phases 1-4) - SHIPPED 2015-03-05</summary>

See .planning/milestones/v0.4.5/ for archived details.

</details>

<details>
<summary>v0.5.0 Protocol Compliance (Phases 1-4) - SHIPPED 2015-03-08</summary>

See .planning/milestones/v0.5.0-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.1 DPI Sizing & UI Polish (Phases 1-4 + insertions) - SHIPPED 2015-03-10</summary>

See .planning/milestones/v0.5.1-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.2 Widget System & UI Polish (Phases 1-3) - SHIPPED 2015-03-11</summary>

See .planning/milestones/v0.5.2-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.3 Widget Grid & Content Widgets (Phases 04-08) - SHIPPED 2015-03-13</summary>

See .planning/milestones/v0.5.3-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.1 Widget Framework & Layout Refinement (Phases 09-12) - SHIPPED 2015-03-15</summary>

See .planning/milestones/v0.6-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.2 Theme Expression & Wallpaper Scaling (Phases 13-14.1) - SHIPPED 2026-03-16</summary>

See .planning/milestones/v0.6.2-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.3 Proxy Routing Fix (Phases 15-18) - SHIPPED 2026-03-16</summary>

- [x] Phase 15: Privilege Model & IPC Lockdown (2/2 plans) -- completed 2026-03-16
- [x] Phase 16: Routing Correctness & Idempotency (4/4 plans) -- completed 2026-03-16
- [x] Phase 17: Status Reporting Hardening (2/2 plans) -- completed 2026-03-16
- [x] Phase 18: Hardware Validation (1/1 plan) -- completed 2026-03-16

See .planning/milestones/v0.6.3-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.4 Widget Work (Phases 19-21) - SHIPPED 2026-03-17</summary>

- [x] Phase 19: Widget Instance Config (2/2 plans) -- completed 2026-03-17
- [x] Phase 20: Simple Widgets (2/2 plans) -- completed 2026-03-17
- [x] Phase 21: Clock Styles & Weather (2/2 plans) -- completed 2026-03-17

See .planning/milestones/v0.6.4-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.5 Widget Refinement (Phases 22-24) - SHIPPED 2026-03-21</summary>

- [x] Phase 22: Date Widget & Clock Cleanup (2/2 plans) -- completed 2026-03-21
- [x] Phase 23: Widget Visual Refinement (interactive review) -- completed 2026-03-21
- [x] Phase 24: Hardware Verification -- completed 2026-03-21

See .planning/milestones/v0.6.5-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.6 Homescreen Layout & Widget Settings Rework (Phases 25-27) - SHIPPED 2026-03-22</summary>

- [x] Phase 25: Selection Model & Interaction Foundation (1/1 plan) -- completed 2026-03-21
- [x] Phase 26: Navbar Transformation & Edge Resize (2/2 plans) -- completed 2026-03-21
- [x] Phase 27: Widget Picker & Page Management (2/2 plans) -- completed 2026-03-21

See .planning/milestones/v0.6.6-ROADMAP.md for archived details.

</details>

## v0.7.0 Kiosk Session & Boot Experience (Phases 28-32)

**Goal:** Replace the default RPi desktop session with a dedicated kiosk session so the user never sees the bare desktop — from power-on to app, only branded splash and the app are visible.

### Phases

- [x] **Phase 28: Kiosk Session Infrastructure** — XDG session entry, stripped labwc config, LightDM autologin drop-in, simplified systemd service (completed 2026-03-22)
- [x] **Phase 29: Compositor Splash Handoff** — swaybg splash in kiosk autostart, frameSwapped-based dismissal in main.cpp (completed 2026-03-22)
- [x] **Phase 30: RPi Boot Splash** — rpi-splash-screen-support TGA logo, cmdline.txt repair, Plymouth masking, initramfs verification (completed 2026-03-22)
- [x] **Phase 31: Exit-to-Desktop** — ApplicationController::exitToDesktop(), ActionRegistry wiring, GestureOverlay desktop button, dm-tool session switch (completed 2026-03-23)
- [x] **Phase 32: Installer Integration** — configure_boot_splash() and create_kiosk_session() installer functions, kiosk mode prompt, idempotent upgrade (completed 2026-03-23)

### Phase Details

#### Phase 28: Kiosk Session Infrastructure

**Goal:** Core session infrastructure everything else depends on. No app code changes — pure config files.
**Depends on:** Nothing (first phase)
**Delivers:** `openauto-kiosk.desktop` XDG session, `/etc/openauto-kiosk/labwc/` config (rc.xml + autostart + environment), LightDM drop-in (`50-openauto-kiosk.conf`), simplified systemd service. Pi boots into kiosk session with app running fullscreen, no desktop chrome.
**Canonical refs:** `.planning/research/ARCHITECTURE.md`, `.planning/research/PITFALLS.md`
**Plans:** 1/1 plans complete

Plans:
- [ ] 28-01-PLAN.md — Kiosk session config files + systemd service simplification

#### Phase 29: Compositor Splash Handoff

**Goal:** Eliminate the black gap between compositor start and app first frame.
**Depends on:** Phase 28 (kiosk autostart file must exist)
**Delivers:** swaybg in kiosk autostart displaying splash PNG, `QQuickWindow::frameSwapped` one-shot handler in `main.cpp` with 500ms delayed `pkill -f "swaybg.*splash"`, splash artwork at `/usr/share/openauto-prodigy/`.
**Canonical refs:** `.planning/research/ARCHITECTURE.md`, `.planning/research/PITFALLS.md`
**Plans:** 1/1 plans complete

Plans:
- [x] 29-01-PLAN.md -- swaybg splash in kiosk autostart + frameSwapped dismissal in main.cpp

#### Phase 30: RPi Boot Splash

**Goal:** Prodigy logo visible from kernel boot onward. Highest-risk phase — hardware validation required.
**Depends on:** Phase 29 (same splash artwork, TGA conversion)
**Delivers:** `rpi-splash-screen-support` configured, TGA generated via ImageMagick, cmdline.txt repaired after configure-splash, Plymouth masked.
**Canonical refs:** `.planning/research/PITFALLS.md`, `.planning/research/STACK.md`
**Research flag:** Requires Pi 4 Trixie hardware validation. raspberrypi/linux#7081 is open.
**Plans:** 1/1 plans complete

Plans:
- [x] 30-01-PLAN.md — Pre-converted TGA + boot splash deployment documentation

#### Phase 31: Exit-to-Desktop

**Goal:** Safety escape hatch from kiosk mode. Can be developed in parallel with Phase 30.
**Depends on:** Phase 28 (both sessions must exist for switch)
**Delivers:** `ApplicationController::exitToDesktop()`, `app.exitToDesktop` ActionRegistry action, "Desktop" button in GestureOverlay, `dm-tool switch-to-user` with 500ms delayed quit.
**Canonical refs:** `.planning/research/ARCHITECTURE.md`, `.planning/research/PITFALLS.md`
**Research flag:** dm-tool with Wayland sessions is MEDIUM confidence. Round-trip hardware test required.
**Plans:** 1/1 plans complete

Plans:
- [x] 31-01-PLAN.md — exitToDesktop() + ActionRegistry + GestureOverlay Desktop button

#### Phase 32: Installer Integration

**Goal:** Wrap all validated Phase 28-31 work into install.sh for automated setup.
**Depends on:** Phases 28-31 (must automate proven work only)
**Delivers:** `configure_boot_splash()` and `create_kiosk_session()` installer functions, kiosk mode prompt, idempotent upgrade handling, revert instructions.
**Canonical refs:** `.planning/research/FEATURES.md`, `.planning/research/PITFALLS.md`, `install.sh`
**Plans:** 2/2 plans complete

Plans:
- [x] 32-01-PLAN.md -- install.sh kiosk session creation + boot splash + kiosk mode prompt
- [x] 32-02-PLAN.md -- install-prebuilt.sh kiosk session creation + boot splash + kiosk mode prompt
