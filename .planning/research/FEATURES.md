# Feature Landscape: Kiosk Session & Boot Experience

**Domain:** Embedded Linux kiosk boot/session management for automotive head unit
**Researched:** 2026-03-22
**Target:** RPi 4 running RPi OS Trixie with labwc compositor, LightDM display manager

## Context: What Already Exists

This milestone transforms the app from "a Qt application that runs on a Pi desktop" into "a dedicated appliance that owns the screen from boot." The Pi already boots, runs labwc, auto-starts the app via systemd, and has a 3-finger gesture overlay. The problem is the 3-second desktop flash between compositor ready and app first frame, and the lack of branded boot experience.

| Component | Status | v0.7.0 Change |
|-----------|--------|---------------|
| systemd service | Exists: ExecStartPre preflight, auto-restart, panel kill/restore | **Simplify.** Kiosk session has no panel to kill. |
| labwc config | Exists: mouseEmulation="no" for multi-touch | **Fork.** Kiosk-specific config (no panel, no taskbar). |
| LightDM autologin | Exists: autologin to `rpd-labwc` session | **Redirect.** Autologin to `openauto-kiosk` session. |
| swaybg | Exists: installed as dependency | **Promote.** From unused dependency to compositor-owned splash. |
| 3-finger GestureOverlay | Exists: volume, brightness, home, day/night, close | **Extend.** Add Exit-to-Desktop button. |
| install.sh | Exists: 9-step TUI, labwc config, system optimization | **Extend.** Add kiosk session creation, Plymouth theme, config.txt edits. |
| config.txt / cmdline.txt | Exists: default RPi OS values | **Modify.** disable_splash, quiet splash, console redirect. |
| Plymouth | Exists: default RPi `pix` theme with raspberry logo | **Replace.** Custom `prodigy` theme with branded splash PNG. |

---

## Table Stakes

Features users expect from a dedicated head unit appliance. Missing any of these makes the product feel unfinished or amateurish.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|--------------|------------|--------------|-------|
| **No visible boot text** | Kernel/systemd messages look broken, not polished | Low | `cmdline.txt` edits (`quiet splash`, `console=tty3`) | Already partially handled by RPi OS defaults. Need `console=tty3` to redirect remaining messages to invisible VT. |
| **Custom boot splash logo** | Rainbow screen + default raspberries = "hobbyist Pi project" not "product" | Low | Custom Plymouth theme | Replace default `pix` theme splash.png. Create custom theme dir to survive apt upgrades. Rebuild initrd with `plymouth-set-default-theme --rebuild-initrd prodigy`. |
| **No desktop flash between compositor and app** | 3s of panel/taskbar/wallpaper destroys appliance illusion | Medium | Kiosk XDG session, stripped labwc config, swaybg splash | The core problem this milestone solves. Requires all three: session file, stripped compositor config, and splash client. |
| **App auto-starts on power-on** | Head unit user never types commands or selects sessions | Low | LightDM autologin to kiosk session (partially exists) | Change `autologin-session=rpd-labwc` to `autologin-session=openauto-kiosk` in lightdm.conf. |
| **No rainbow GPU test screen** | Product boot screen, not hardware diagnostic | Low | `disable_splash=1` in `/boot/firmware/config.txt` | One-liner. Already confirmed working on Pi 4. |
| **No screen blanking in kiosk mode** | Head unit must never go dark while car is running | Low | Omit swayidle from kiosk autostart, no DPMS config | RPi desktop session runs swayidle by default. Kiosk autostart simply does not include it. No swayidle = no idle timeout = screen stays on. |
| **Crash shows splash, not bare desktop** | If app crashes, user sees branded image, not error text or login | Low | swaybg as compositor-owned background (survives app crash) | labwc autostart launches swaybg before app. App crash = swaybg remains visible. systemd restart brings app back. |

---

## Differentiators

Features that elevate beyond "hobbyist kiosk" into "automotive appliance" territory. Not expected, but valued.

| Feature | Value Proposition | Complexity | Dependencies | Notes |
|---------|-------------------|------------|--------------|-------|
| **Seamless splash-to-app handoff** | App dismisses splash on first rendered frame (`frameSwapped`), not a timer. No black flash, no flicker between splash and app. | Medium | `QQuickWindow::frameSwapped` signal, app sends `pkill swaybg` | The signal fires from the render thread -- connect with `Qt::QueuedConnection` to marshal kill to main thread. One-shot connection, disconnect after first fire. Already prototyped as app-owned child process; moving to compositor-owned + app-dismissed is the improvement. |
| **Exit-to-desktop** | 3-finger overlay "Exit" button returns user to normal RPi desktop (panel, taskbar, browser, etc.) for maintenance/configuration | Medium | GestureOverlay Exit button + session switch mechanism | Escape hatch so user is never trapped in kiosk. Two-step: flip LightDM default session to `rpd-labwc`, then terminate current session via `loginctl`. LightDM restarts into desktop. |
| **Session-aware service management** | systemd service adapts behavior to kiosk vs desktop (no panel kill/restore in kiosk mode) | Low | `$XDG_SESSION_DESKTOP` env check in service ExecStartPre | Current service kills wf-panel-pi on start and restores on clean stop. Kiosk session has no panel -- these lines should be conditional. |
| **Installer session revert capability** | User can switch back to desktop session without reinstalling | Low | Helper script or settings toggle | Write `openauto-session-toggle` script that swaps LightDM autologin-session between `openauto-kiosk` and `rpd-labwc`. Callable from SSH/web config/settings UI. |
| **Branded compositor background** | If swaybg and app both fail, labwc shows Prodigy-branded color instead of black | Low | labwc `<core><background>` in rc.xml | labwc can set its own background color. Dark branded color instead of pure black provides visual identity even in failure state. |

---

## Anti-Features

Features to explicitly NOT build for this milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Boot speed optimization** | Dangerous rabbit hole with diminishing returns. Silent boot hides perceived latency. Actual boot time involves firmware, kernel, systemd, compositor startup -- each optimizable but none trivially. | Ship silent boot first. Existing optimizations (disabled cloud-init, wait-online, ModemManager) already shave ~15s. Profile boot time as a future milestone if users report it as painful. |
| **Animated boot splash (Plymouth scripted theme)** | Plymouth scripted themes (spinner, throbber) are fragile, kernel-version-sensitive, and break across apt upgrades. Trixie already has documented Plymouth breakage after `apt full-upgrade`. | Static logo PNG in a custom Plymouth theme. Simple, reliable, survives most updates. Rebuild initrd as part of install. |
| **greetd replacement for LightDM** | greetd is architecturally cleaner but RPi OS ships LightDM. Replacing it means fighting upstream packaging, breaking `raspi-config`, and maintaining a non-standard display manager. | Work within LightDM. Custom XDG session file. `dm-tool` / `loginctl` for session switching. |
| **cage (single-app Wayland compositor)** | cage is purpose-built for kiosk but means abandoning labwc. labwc is what RPi OS ships, what the app is tested on, and what handles multi-touch correctly with mouseEmulation="no". | Strip labwc config for kiosk mode. Same compositor, different config profile. |
| **Custom display manager greeter** | Building a LightDM greeter is massive scope for zero value in a single-user automotive context. | Use stock `lightdm-autologin-greeter` or default gtk-greeter. User never sees it in kiosk mode. |
| **U-Boot / early framebuffer splash** | Industrial-grade early boot display requires firmware-level DRM handoff plumbing. Overkill for RPi 4 where the 1-2s firmware gap is acceptable (user is starting the car). | Plymouth covers post-kernel. `disable_splash=1` removes rainbow. The gap is fine. |
| **Wayland lock screen** | Screen locking makes no sense in a car dashboard. Nobody unlocks their head unit. | Omit swaylock/swayidle entirely from kiosk session. |
| **Power management / suspend** | Car head units should not suspend. Power is cut by ignition circuit (relay, timer relay, or UPS HAT). Software suspend introduces wake-up latency and state confusion. | No power management in kiosk session. If needed later, separate "power management" milestone with GPIO/UPS integration. |
| **Power off button** | Seems useful but is dangerous -- a car head unit powered off while driving loses navigation, audio, phone. Hard shutdown risks filesystem corruption. | Defer to future milestone if there's demand. Car ignition handles power lifecycle. The Exit-to-Desktop button covers the "get out of kiosk" use case. |

---

## Feature Dependencies

```
disable_splash=1 (config.txt) ──────┐
                                      ├──> Clean kernel boot (no rainbow, no text)
quiet splash console=tty3 (cmdline) ─┘

Custom Plymouth theme ──────────────> Branded kernel boot splash
                                        │
                                        v
XDG session file (.desktop) ────────> LightDM autologin into kiosk session
                                        │
                                        v
Stripped labwc config ──────────────> No panel, no taskbar, no wallpaper daemon
(rc.xml + autostart)                    │
                                        v
swaybg in labwc autostart ──────────> Branded splash as first Wayland surface
                                        │
                                        v
App launch (systemd or labwc autostart) > App initializes behind splash
                                        │
                                        v
frameSwapped signal ────────────────> App kills swaybg on first rendered frame
                                        │
                                        v
Running application                     Seamless experience from boot to app


Exit-to-desktop (3-finger overlay) ──> Independent feature, parallel buildable
    │
    ├── Flip LightDM default session to rpd-labwc
    └── loginctl terminate-session
        └── LightDM restarts into desktop session
```

---

## Detail: Boot Splash Chain

The full boot experience is a 4-stage handoff. The goal is visual continuity -- user sees black, then logo, then app. No intermediate states.

| Stage | What Shows | Duration | Owner | Implementation |
|-------|-----------|----------|-------|----------------|
| 1. Firmware | Nothing (rainbow disabled) | ~1s | RPi firmware | `disable_splash=1` in config.txt |
| 2. Kernel + Plymouth | Prodigy logo (static PNG) | ~3-5s | Plymouth | Custom theme at `/usr/share/plymouth/themes/prodigy/` |
| 3. Compositor splash | Same Prodigy logo (swaybg) | ~1-3s | labwc autostart | `swaybg -i /path/to/splash.png -m fill &` |
| 4. Application | App UI | rest of session | openauto-prodigy | Kills swaybg on `frameSwapped` |

Stages 2 and 3 use the **same image** so the transition is invisible. Plymouth exits when the display manager takes over (built-in Plymouth behavior -- `plymouth quit` runs automatically). swaybg starts immediately in labwc autostart, covering the gap.

## Detail: XDG Session Architecture

Two session files coexist in `/usr/share/wayland-sessions/`:

| Session | File | Purpose | What Runs |
|---------|------|---------|-----------|
| Desktop (stock RPi) | `rpd-labwc.desktop` | Normal Pi desktop | wf-panel-pi, lwrespawn, pcmanfm, swaybg wallpaper |
| Kiosk (ours) | `openauto-kiosk.desktop` | Prodigy appliance mode | swaybg splash, openauto-prodigy (no panel, no desktop apps) |

The kiosk session needs its own labwc config. Two approaches:

**Option A: Separate config directory.** Set `LABWC_CONFIG_DIR=/etc/openauto/labwc` in the session `.desktop` `Exec=` environment. Kiosk gets its own `rc.xml` and `autostart`. User's `~/.config/labwc/` untouched for desktop session.

**Option B: XDG_CONFIG_HOME override.** Session `.desktop` sets `XDG_CONFIG_HOME=/etc/openauto` and labwc reads `/etc/openauto/labwc/`. Broader override, might affect other XDG apps.

Recommendation: **Option A** is cleaner. The `.desktop` file's `Exec=` can be a wrapper script that sets the env var and execs labwc.

## Detail: Kiosk labwc Config

The kiosk `rc.xml` needs:
- `mouseEmulation="no"` (already required for multi-touch)
- No keyboard shortcuts for desktop actions (no Alt-Tab, no terminal shortcut)
- Optional: window rule to fullscreen `openauto-prodigy` by app_id
- Optional: `<core><background>#1C1B1F</background></core>` (M3 surface color as fallback)

The kiosk `autostart` needs:
- `swaybg -i /usr/share/openauto/splash.png -m fill &` (splash as first surface)
- App launch (either direct or via systemd `--user` service)
- Explicitly NO: `wf-panel-pi`, `lwrespawn`, `pcmanfm`, `swayidle`

## Detail: Exit-to-Desktop Mechanism

Three approaches evaluated, in order of reliability:

1. **Config flip + session terminate (recommended).** App writes `autologin-session=rpd-labwc` to a LightDM override file, then calls `loginctl terminate-session $XDG_SESSION_ID`. LightDM restarts and auto-logins to desktop. Requires sudo/polkit for the LightDM config write -- the existing system service (root daemon) can handle this via IPC.

2. **`dm-tool switch-to-greeter`.** Cleanest API but has documented reliability issues. May silently do nothing. Not recommended as primary mechanism.

3. **App exit + systemd Restart=no override.** App exits, systemd does NOT restart (because exit was intentional, not crash). LightDM stays on kiosk session. User must manually switch. Least useful.

The config-flip approach is best because it ensures the desktop session comes up after the kiosk terminates. The system service (root daemon with IPC socket) already exists and can write `/etc/lightdm/lightdm.conf.d/50-openauto-session.conf`.

## Detail: GestureOverlay Changes

Current 3-finger overlay buttons: Home, Day/Night, Close.

Add for this milestone:
- **Exit to Desktop** -- Material Symbols `desktop_windows` (\ue30a) or `logout` (\ue9ba). Requires confirmation (accidental exit while driving is bad). Two-tap: first tap shows "Exit?" label, second tap within 3s confirms. Triggers session switch mechanism above.

The existing `ElevatedButton` pattern and `ActionRegistry.dispatch()` flow are reusable.

## Detail: Plymouth Custom Theme

Create `/usr/share/plymouth/themes/prodigy/`:
```
prodigy/
  prodigy.plymouth    # Theme descriptor
  splash.png          # 1024x768 or 1920x1080 centered logo on dark bg
```

`prodigy.plymouth` contents:
```ini
[Plymouth Theme]
Name=Prodigy
Description=OpenAuto Prodigy boot splash
ModuleName=script
# OR for simple static image:
# ModuleName=two-step (if only static image needed)
```

Simpler approach: just modify the `pix` theme by creating `/usr/share/plymouth/themes/prodigy/` as a copy of `pix` with our PNG, then `plymouth-set-default-theme --rebuild-initrd prodigy`. The `pix` module handles static centered images natively.

Key: **create a new theme directory** rather than editing `pix` in-place. Editing `pix` gets overwritten by `plymouth-themes` package updates.

---

## MVP Recommendation

Build in this order based on dependencies and incremental value:

1. **XDG kiosk session + stripped labwc config** -- Core infrastructure. Everything else depends on this. Create `.desktop` file, kiosk `rc.xml`, kiosk `autostart` (with swaybg + app launch). Test that LightDM can autologin to it.

2. **swaybg splash in kiosk autostart** -- Eliminates the desktop flash. Immediately visible result. Validates the compositor-owns-splash architecture.

3. **App frameSwapped splash dismissal** -- Completes the seamless handoff chain. Connect to `QQuickWindow::frameSwapped`, one-shot `pkill swaybg`, verify no black flash between splash death and app render.

4. **config.txt / cmdline.txt cleanup** -- `disable_splash=1`, `quiet splash`, `console=tty3`. Quick wins that clean up pre-Plymouth boot.

5. **Custom Plymouth theme** -- Branded kernel boot. Create theme, rebuild initrd. Test survives reboot.

6. **Exit-to-desktop in GestureOverlay** -- Escape hatch. Add button, implement session switch via system service IPC, test round-trip kiosk->desktop->kiosk.

7. **Installer integration** -- Creates kiosk session, Plymouth theme, config.txt edits, LightDM config. Wraps everything built above into the existing install.sh TUI flow.

**Defer:**
- Boot speed optimization -- separate concern, separate milestone.
- Power off button -- car ignition handles power lifecycle.
- Session-aware systemd conditionals -- nice cleanup but not gating.

---

## Existing Assets to Leverage

| Asset | How It Helps |
|-------|-------------|
| swaybg already installed | No new dependency for compositor splash |
| systemd service with ExecStartPre | Pre-flight checks already run; splash bridges that startup gap |
| 3-finger GestureOverlay | Exit button slots into existing UI pattern with ElevatedButton |
| install.sh TUI with labwc step | Kiosk session creation extends existing labwc configuration step |
| `optimize_system()` in installer | config.txt/cmdline.txt changes fit naturally here |
| wf-panel-pi kill/restore in service | Kiosk session eliminates this hack entirely (no panel exists to manage) |
| System service (root daemon + IPC) | Can handle privileged LightDM config writes for session switching |
| ActionRegistry | Exit action dispatched via existing `ActionRegistry.dispatch("app.exit-to-desktop")` |

---

## Confidence Assessment

| Finding | Confidence | Reason |
|---------|------------|--------|
| Plymouth custom theme approach | HIGH | Official RPi docs + multiple verified community implementations |
| `disable_splash=1` on Pi 4 | HIGH | Official RPi config.txt documentation |
| labwc autostart for swaybg splash | HIGH | labwc docs + multiple RPi kiosk implementations confirmed |
| XDG session file in `/usr/share/wayland-sessions/` | HIGH | LightDM + ArchWiki + RPi forum + Debian docs all agree |
| `frameSwapped` for first-frame detection | HIGH | Qt 6 official QQuickWindow docs |
| Plymouth stability on Trixie after updates | MEDIUM | Forum reports of breakage after apt full-upgrade; may be version-specific |
| `dm-tool switch-to-greeter` reliability | LOW | Documented bugs, reports of silent no-ops; avoid as primary mechanism |
| `loginctl terminate-session` for session exit | MEDIUM | Standard systemd/logind, but untested with this LightDM + labwc + Wayland combo |
| `LABWC_CONFIG_DIR` env override | MEDIUM | Consistent with labwc XDG handling but needs verification on Trixie |
| Same splash image across Plymouth + swaybg | HIGH | Both tools render static PNGs; same file, same visual |

## Sources

- [RPi config.txt documentation](https://www.raspberrypi.com/documentation/computers/config_txt.html)
- [labwc configuration reference (rc.xml)](https://labwc.github.io/labwc-config.5.html)
- [labwc integration guide (swayidle, swaybg)](https://labwc.github.io/integration.html)
- [LightDM - ArchWiki](https://wiki.archlinux.org/title/LightDM)
- [LightDM - Gentoo wiki](https://wiki.gentoo.org/wiki/LightDM)
- [Labwc - ArchWiki](https://wiki.archlinux.org/title/Labwc)
- [RPi kiosk with labwc (forum)](https://forums.raspberrypi.com/viewtopic.php?t=378883)
- [labwc autostart (forum)](https://forums.raspberrypi.com/viewtopic.php?t=379321)
- [Plymouth boot splash breakage on Trixie](https://forums.raspberrypi.com/viewtopic.php?t=393289)
- [Custom RPi boot splash (Tom's Hardware)](https://www.tomshardware.com/how-to/custom-raspberry-pi-splash-screen)
- [QQuickWindow::frameSwapped (Qt 6 docs)](https://doc.qt.io/qt-6/qquickwindow.html)
- [RPi Kiosk Display System (GitHub)](https://github.com/TOLDOTECHNIK/Raspberry-Pi-Kiosk-Display-System)
- [labwc kiosk discussion (GitHub)](https://github.com/labwc/labwc/discussions/2301)
- [labwc DPMS/idle (GitHub issue)](https://github.com/labwc/labwc/issues/1477)
- [swaybg repository](https://github.com/swaywm/swaybg)
- [Gentoo Flicker Free Boot wiki](https://wiki.gentoo.org/wiki/Flicker_Free_Boot)
- [labwc rc.xml.all reference (GitHub)](https://github.com/labwc/labwc/blob/master/docs/rc.xml.all)

---

*Feature research for: OpenAuto Prodigy v0.7.0 Kiosk Session & Boot Experience*
*Researched: 2026-03-22*
