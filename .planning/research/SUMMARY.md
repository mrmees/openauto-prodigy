# Project Research Summary

**Project:** OpenAuto Prodigy v0.7.0 — Kiosk Session & Boot Experience
**Domain:** Embedded Linux kiosk session management and boot splash for automotive head unit
**Researched:** 2026-03-22
**Confidence:** HIGH (all four research areas supported by official documentation and codebase analysis; boot splash layer MEDIUM due to sparse rpi-splash-screen-support Trixie testing)

## Executive Summary

This milestone is fundamentally a system-configuration exercise, not a software development one. The goal is to transform the Pi from "a computer running an app" into "a dedicated appliance that owns the screen from boot." Almost no new C++ or QML is required — the work is creating five configuration files, two installer functions, adding ~10 lines to `main.cpp`, and one new button in the 3-finger overlay. The core deliverable is a dedicated XDG session (`openauto-kiosk`) that replaces the default `rpd-labwc` desktop session with a stripped compositor config that runs nothing but a splash screen and the app itself. This eliminates the 3-second desktop flash that currently betrays the "it's just a Pi running stuff" origin.

The recommended approach is a three-layer boot handoff: kernel splash (rpi-splash-screen-support TGA image) covers post-firmware early boot, compositor splash (swaybg in labwc autostart) covers the gap between kernel framebuffer handoff and app readiness, and the app dismisses swaybg on its first rendered frame via `QQuickWindow::frameSwapped`. All three layers display the same Prodigy logo so transitions are invisible. The critical architectural decision is that swaybg is owned by the compositor (labwc autostart), not by the app — this ensures the branded splash persists through app crashes, while systemd's restart logic brings the app back cleanly.

The main risks are in the boot splash layer. The `rpi-splash-screen-support` kernel boot package has an open issue (raspberrypi/linux#7081) where kernel boot messages can overlay the splash image — mitigated with `loglevel=0` in cmdline.txt but requires hardware verification. Additionally, `configure-splash` strips `quiet` from cmdline.txt, requiring the installer to re-add it. The exit-to-desktop feature requires a true LightDM session switch (`dm-tool switch-to-user`), not just an app kill — killing the app in the kiosk session leaves nothing but a black screen since there is no desktop infrastructure underneath.

## Key Findings

### Recommended Stack

This milestone adds exactly two new packages (`rpi-splash-screen-support` and `imagemagick`, both install-time only) and touches no production runtime dependencies. The existing stack — Qt 6.8, labwc, LightDM, swaybg, systemd — handles everything needed. The implementation involves configuration files and heredocs in `install.sh` more than any new compiled code.

**Core technologies (existing, repurposed):**
- **labwc with `-C` flag**: purpose-built kiosk config at `/etc/openauto-kiosk/labwc/` — avoids polluting the user's desktop config and survives labwc package upgrades
- **swaybg**: promoted from unused dependency to compositor-owned Wayland splash surface — started in labwc autostart, lives on the `background` wlr-layer-shell layer beneath the Qt app window, killed by the app after first frame
- **LightDM drop-in config**: `/etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf` overrides autologin session without touching `lightdm.conf` directly — survives raspi-config edits to the base file
- **rpi-splash-screen-support** (new, install-time): installs a TGA into initramfs and sets `fullscreen_logo=1` in cmdline.txt — replaces the four-raspberry kernel framebuffer logo
- **`QQuickWindow::frameSwapped`**: stable Qt 5+ signal, fires after first frame is submitted to the compositor — used for one-shot swaybg cleanup after a 200-500ms grace period

**Technologies explicitly NOT adding:**
- **Plymouth custom theme**: Plymouth on Trixie has documented breakage after `apt full-upgrade`; static kernel splash via rpi-splash-screen-support is more reliable
- **cage (kiosk compositor)**: would require abandoning labwc which is already tested and handles multi-touch correctly
- **greetd**: LightDM is already installed; replacing it fights upstream RPi packaging
- **swayidle/swaylock**: car head units must never sleep; omitting these from kiosk autostart is the entire solution

### Expected Features

The research clearly separates what must be done from what adds polish from what to skip entirely.

**Must have (table stakes):**
- No visible boot text — `quiet loglevel=0 vt.global_cursor_default=0` in cmdline.txt
- Custom boot splash logo — replaces default raspberry logo via rpi-splash-screen-support TGA
- No desktop flash between compositor and app — kiosk XDG session with stripped labwc config
- App auto-starts on power-on — LightDM autologin to `openauto-kiosk` session
- No rainbow GPU test screen — `disable_splash=1` in config.txt (one-liner)
- No screen blanking in kiosk mode — omit swayidle from kiosk autostart entirely
- Crash shows splash not bare desktop — compositor-owned swaybg survives app crashes

**Should have (differentiators):**
- Seamless splash-to-app handoff — frameSwapped-based dismissal, not a timer; no black flash
- Exit-to-desktop — 3-finger overlay button for maintenance access via `dm-tool switch-to-user`
- Installer session revert capability — documented revert path, helper script or settings toggle

**Defer to v2+:**
- Boot speed optimization — profiling is a separate milestone; silent boot already masks perceived latency
- Animated Plymouth splash — fragile on Trixie; static PNG is reliable and sufficient
- Power off button — car ignition handles power lifecycle; risk of filesystem corruption during drive

### Architecture Approach

The architecture follows four design patterns consistently across all research files: (1) drop-in configs over base file modifications, (2) compositor-owned splash with app-owned dismissal, (3) session switch via `dm-tool` rather than logout/login, and (4) systemd as the sole app lifecycle manager with labwc autostart handling only compositor-level concerns. These patterns together ensure clean session separation, crash-safe splash behavior, and no double-launch conflicts.

**Major components:**
1. **openauto-kiosk.desktop** — XDG session entry pointing labwc at `/etc/openauto-kiosk/labwc/` via `-C` flag; LightDM autologin target
2. **Kiosk labwc config** — three files: `rc.xml` (mouseEmulation="no", no decorations, fullscreen windowRule for the app), `autostart` (swaybg splash only — one line), `environment` (Qt Wayland platform hint)
3. **rpi-splash-screen-support config** — TGA conversion via ImageMagick, `configure-splash` invocation, cmdline.txt repair (re-add `quiet` after configure-splash strips it)
4. **`main.cpp` frameSwapped handler** — one-shot connection to `QQuickWindow::frameSwapped`, `QTimer::singleShot(500ms)` then `pkill -f "swaybg.*splash"` (pattern avoids killing desktop wallpaper swaybg on other VTs)
5. **ApplicationController::exitToDesktop()** — `dm-tool switch-to-user matt rpd-labwc` + 500ms delayed `QCoreApplication::quit()`
6. **GestureOverlay.qml** — fourth ElevatedButton ("Desktop"), dispatches `app.exitToDesktop` via ActionRegistry
7. **install.sh** — new `configure_boot_splash()` and `create_kiosk_session()` functions, simplified service file (remove wf-panel-pi kill/restore lines that are irrelevant in kiosk mode)

### Critical Pitfalls

Full detail in `PITFALLS.md`. Top five requiring explicit mitigation:

1. **Exit-to-desktop requires a real session switch, not an app kill** — In kiosk mode there is no desktop underneath the app. Killing the app leaves a black screen. Must use `dm-tool switch-to-user USERNAME rpd-labwc` for an actual LightDM session switch. A 500ms delay before `QCoreApplication::quit()` gives LightDM time to register the switch before the kiosk session ends.

2. **configure-splash strips `quiet` from cmdline.txt** — The tool intentionally removes `quiet` and `plymouth.ignore-serial-consoles`. Installer must re-add `quiet loglevel=0 logo.nologo vt.global_cursor_default=0` after running configure-splash. Failure causes kernel messages to overlay the splash.

3. **raspi-config overwrites LightDM session config** — raspi-config modifies `/etc/lightdm/lightdm.conf` directly. Use a drop-in file in `lightdm.conf.d/` (which raspi-config does not touch) for the session override. Document clearly that users must not use raspi-config for boot/login settings after installation.

4. **Double app launch from systemd + labwc autostart** — labwc autostart must contain ONLY `swaybg` (compositor concerns). The systemd service is the sole app launcher. Any app invocation in autostart causes socket conflicts or double instances.

5. **Kernel update clears initramfs splash** — `apt upgrade` triggers `update-initramfs` which rebuilds without the TGA if the hook was corrupted or TGA moved. Installer should verify hook existence and executability after configure-splash. Detection: `lsinitramfs /boot/firmware/initrd.img-$(uname -r) | grep logo.tga`.

## Implications for Roadmap

The dependency chain is clear and maps directly to a five-phase build order. Each phase is independently testable and produces a shippable intermediate state.

### Phase 1: Kiosk Session Infrastructure
**Rationale:** Core infrastructure everything else depends on. No app code changes — pure config files. Lowest risk phase; validates the session architecture before touching boot splash or app code.
**Delivers:** `openauto-kiosk.desktop`, `/etc/openauto-kiosk/labwc/` config directory (rc.xml + autostart + environment), LightDM drop-in (`50-openauto-kiosk.conf`), simplified systemd service (wf-panel-pi kill/restore lines removed). Pi boots into kiosk session with app running fullscreen, no desktop chrome visible.
**Addresses:** "no desktop flash" (table stakes), "app auto-starts" (table stakes), "no screen blanking" (table stakes)
**Avoids:** Pitfall 8 (kiosk config contaminating desktop session) via labwc `-C` flag; Pitfall 10 (double launch) by keeping systemd as sole launcher; Pitfall 16 (apt upgrade overwrites) by using installer-owned config directories only

### Phase 2: Compositor Splash Handoff
**Rationale:** Depends on the kiosk session from Phase 1 (needs the autostart file to exist). Delivers the most visible UX improvement — eliminates the black gap between labwc start and app first frame. Also moves swaybg from fragile app-child-process ownership to compositor ownership.
**Delivers:** swaybg in kiosk autostart, `frameSwapped` handler in `main.cpp` (one-shot, 500ms delay, pattern-matched pkill), splash PNG deployed to `/usr/share/openauto-prodigy/`. Seamless transition from labwc start through app first frame with no flash.
**Uses:** `QQuickWindow::frameSwapped`, `pkill -f "swaybg.*splash"` (avoids killing desktop VT swaybg)
**Avoids:** Pitfall 5 (layer-shell stacking misunderstanding — swaybg on background layer is occluded automatically by app window; pkill is cleanup not visual handoff); Pitfall 6 (frameSwapped timing — 500ms grace period ensures compositor has composited the frame); Pitfall 4 (autostart race — swaybg is the first and only autostart command, lightweight enough to win the race)

### Phase 3: RPi Boot Splash
**Rationale:** Depends on splash artwork from Phase 2 (same image deployed as TGA). Isolated to boot configuration — a mistake here doesn't break the running app or kiosk session. Highest-risk phase due to hardware dependency; test in isolation before proceeding to Phase 4.
**Delivers:** Prodigy logo visible from kernel boot onward. `rpi-splash-screen-support` installed and configured, TGA generated from splash.png with exact ImageMagick parameters, cmdline.txt repaired post configure-splash, Plymouth masked (`systemctl mask plymouth-start.service`).
**Avoids:** Pitfall 12 (TGA conversion — exact `magick` command with `-flip -colors 224 -alpha off -compress none`, pre-validate with `file` and `identify`); Pitfall 13 (re-add `quiet loglevel=0` after configure-splash strips them); Pitfall 3 (initramfs hook verification after configure-splash runs)
**Research flag:** Requires hardware validation on Pi 4 Trixie. raspberrypi/linux#7081 (console text overlay) is an open issue; `loglevel=0` is the expected mitigation but must be confirmed on real hardware. Test by watching a full cold boot cycle before marking this phase complete.

### Phase 4: Exit-to-Desktop
**Rationale:** Depends on Phase 1 (both sessions must exist for the switch to make sense). Pure app code — can be developed in parallel with Phase 3 since they touch different subsystems. Critical safety feature; validates the full round-trip before installer locks users into kiosk mode.
**Delivers:** `ApplicationController::exitToDesktop()`, `app.exitToDesktop` ActionRegistry registration, "Desktop" ElevatedButton in GestureOverlay.qml, full round-trip kiosk→desktop→kiosk verified on hardware.
**Implements:** `dm-tool switch-to-user` pattern with 500ms delayed quit, ActionRegistry dispatch wiring
**Avoids:** Pitfall 9 (black screen after app exit — this entire phase IS the prevention; session switch not app kill)
**Research flag:** `dm-tool switch-to-user` with Wayland sessions is MEDIUM confidence. Round-trip hardware test is the gate for Phase 5. If dm-tool is unreliable, fallback is config-flip + `loginctl terminate-session` (also documented in PITFALLS.md).

### Phase 5: Installer Integration
**Rationale:** Final phase — wraps all validated Phase 1-4 work into `install.sh` so a fresh install produces the full kiosk experience automatically. Must not proceed until Phase 1-4 are verified on hardware, because the installer must faithfully automate what has been proven to work.
**Delivers:** `configure_boot_splash()` and `create_kiosk_session()` installer functions, kiosk mode prompt in install flow ("Enable kiosk mode? [Y/n]"), idempotent upgrade handling (overwrite existing kiosk config), revert instructions in installer summary, `desktop-file-validate` on session file before enabling autologin.
**Avoids:** Pitfall 7 (desktop file syntax errors — validate before enabling autologin, keep `rpd-labwc.desktop` intact); Pitfall 2 (raspi-config overwrite — drop-in strategy correctly implemented); Pitfall 1 (black flash between kernel and compositor splash — accepted and documented; `loglevel=0` minimizes it but cannot eliminate the DRM/KMS mode-set gap)

### Phase Ordering Rationale

- Phase 1 before all others: the XDG session is the load-bearing foundation — splash, exit, and installer all assume it exists
- Phase 2 before Phase 3: both phases need the same splash artwork; Phase 2 establishes the compositor-level splash pattern that Phase 3 merely prepends to
- Phase 3 isolated to boot config so high-risk cmdline.txt mutations don't block Phase 4 development
- Phases 3 and 4 are parallel-capable (different subsystems: boot config vs. app code)
- Phase 5 last because it must automate validated Phase 1-4 work — installer that ships broken config is worse than no installer

### Research Flags

Phases requiring hardware validation before sign-off:
- **Phase 3 (boot splash):** rpi-splash-screen-support behavior on Trixie is MEDIUM confidence. The raspberrypi/linux#7081 console overlay issue is open and unresolved. Also need to confirm `configure-splash` cmdline.txt mutations on the actual Pi. Do not proceed to Phase 5 until a full cold boot cycle from power-on shows the expected splash behavior.
- **Phase 4 (exit-to-desktop):** `dm-tool switch-to-user` with Wayland sessions has documented reliability concerns. Full round-trip test (kiosk→desktop→kiosk) on Pi hardware is the gate for the Phase 5 installer.

Phases with well-documented patterns (standard implementation, no additional research needed):
- **Phase 1:** XDG session files and LightDM drop-in configs are stable freedesktop.org standards with HIGH confidence documentation.
- **Phase 2:** `QQuickWindow::frameSwapped` and swaybg layer-shell behavior are both well-understood and HIGH confidence. This phase follows the exact `pkill` + `QTimer::singleShot` pattern already prototyped in the codebase.
- **Phase 5:** Installer idempotency patterns are established within this codebase's existing install.sh structure.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Two new packages with clear purposes; all other tools already in use and understood in this project |
| Features | HIGH | Table stakes unambiguous; anti-features well-reasoned with clear alternatives documented |
| Architecture | HIGH | Direct codebase analysis + official docs; all patterns consistent with existing project conventions |
| Pitfalls | MEDIUM-HIGH | Critical pitfalls are HIGH confidence (boot sequence, session switching, double-launch); boot splash pitfalls MEDIUM due to sparse rpi-splash-screen-support Trixie documentation |

**Overall confidence:** HIGH for app code and session configuration; MEDIUM for kernel boot splash layer (hardware validation required before installer ships it)

### Gaps to Address

- **rpi-splash-screen-support on Trixie:** The package was designed for Bookworm. If `configure-splash` fails or produces unexpected cmdline.txt results on Trixie, the fallback is manual cmdline.txt editing with direct initramfs hook installation. Validate early in Phase 3 implementation.

- **Qt app_id for labwc window rule:** The windowRule `identifier="openauto-prodigy"` in kiosk rc.xml depends on Qt setting its Wayland app_id to match. May require `app.setDesktopFileName("openauto-prodigy")` in `main.cpp`, or use wildcard pattern `identifier="openauto*"` as a safer fallback. Verify on hardware during Phase 1.

- **dm-tool VT switching behavior:** The switch between kiosk (VT 8) and desktop (VT 7+) sessions may produce a brief black flash as VTs change. This is a LightDM limitation and acceptable for a maintenance escape hatch, but should be observed on Pi hardware during Phase 4.

- **swaybg layer stacking confirmation:** Research confirms swaybg runs on `background` wlr-layer-shell layer and the Qt app window occludes it automatically once mapped. The `pkill` after frameSwapped is cleanup, not a visual handoff trigger. Verify visually during Phase 2 — if there is a flash between splash and app, the 500ms grace period may need tuning.

- **Black flash between kernel and compositor splash (Pitfall 1):** This is a known DRM/KMS limitation — when labwc takes ownership of the DRM device it discards the kernel framebuffer, creating a brief black gap before swaybg's first Wayland surface renders (~200-500ms). This cannot be fully eliminated. Accept it, minimize with early swaybg placement in autostart, document as known behavior.

## Sources

### Primary (HIGH confidence)
- labwc configuration manual (labwc.github.io) — `-C` flag, rc.xml format, autostart, environment, windowRules, LABWC_PID
- LightDM ArchWiki — autologin-session, wayland-sessions directory, conf.d override mechanism, dm-tool
- Qt 6 QQuickWindow docs (doc.qt.io) — frameSwapped signal semantics and timing
- rpi-splash-screen-support configure-splash source (GitHub) — cmdline.txt mutations, TGA requirements, initramfs hook
- dm-tool man page (manpages.debian.org) — switch-to-user, switch-to-greeter commands
- swaybg GitHub — layer-shell background usage, image modes, process lifecycle
- wlr-layer-shell protocol spec — layer ordering (background/bottom/top/overlay)
- Existing codebase: `install.sh`, `src/main.cpp`, `src/ui/ApplicationController.cpp`, `qml/components/GestureOverlay.qml`, `docs/pi-config/`

### Secondary (MEDIUM confidence)
- raspberrypi/linux#7081 — fullscreen_logo console text overlay issue (open, unresolved)
- labwc kiosk discussion #2301 (GitHub) — autostart kiosk patterns, window rules
- labwc autostart race discussion #2515 (GitHub) — first-boot ordering issue
- RPi Forums: kiosk with labwc — autostart patterns, panel suppression, swaybg splash
- RPi Forums: splash breakage after apt upgrade — initramfs hook vulnerability on kernel updates
- `.planning/notes/kiosk-session-splash.md` — Codex architecture recommendation (project-internal)

### Tertiary (LOW confidence — needs hardware validation)
- `loginctl terminate-session` for session exit — standard systemd/logind API, untested with this exact labwc+LightDM+Wayland combination; fallback if dm-tool is unreliable
- `LABWC_CONFIG_DIR` environment override — alternate to `-C` flag, consistent with XDG handling but not tested in this project's Trixie setup

---
*Research completed: 2026-03-22*
*Ready for roadmap: yes*
