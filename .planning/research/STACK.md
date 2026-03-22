# Technology Stack

**Project:** OpenAuto Prodigy v0.7.0 -- Kiosk Session & Boot Experience
**Researched:** 2026-03-22
**Confidence:** MEDIUM-HIGH -- multiple areas verified against official sources and live Pi config, but `rpi-splash-screen-support` behavior on Trixie has a known kernel issue that needs hardware validation

---

## Stack Verdict: System-Level Configuration, Not Application Code

This milestone is fundamentally different from previous ones. It adds almost no C++ or QML code. Instead it creates system configuration files (XDG session, labwc config, LightDM config, kernel boot parameters) and adds a single `QProcess::startDetached("pkill")` call for splash dismissal. The app binary gains one signal connection (~5 lines of C++). Everything else is installer work.

**New packages:** 2 (`rpi-splash-screen-support`, `imagemagick` -- both install-time only)
**New config files:** 5 (session .desktop, kiosk rc.xml, kiosk autostart, kiosk environment, LightDM override)
**New C++ code:** ~10 lines (frameSwapped -> kill swaybg)

---

## Locked Stack (Unchanged)

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| Qt 6.8 | 6.8.2 | UI framework, QML engine | Locked |
| C++17 | gcc 12+ | Core logic | Locked |
| CMake | 3.22+ | Build system | Locked |
| labwc | (Trixie default) | Wayland compositor | Locked -- reused with kiosk config |
| LightDM | (Trixie default) | Display manager | Locked -- reused with autologin override |
| swaybg | (already installed) | Wayland background client | Locked -- already a dependency |
| systemd | (Trixie default) | Service management | Locked |

---

## New Packages

### Install-Time Dependencies (Installer Only)

| Package | Version | Purpose | Why This One |
|---------|---------|---------|-------------|
| `rpi-splash-screen-support` | Latest in RPi apt | Kernel boot splash (`configure-splash` tool) | Official RPi Foundation package. Handles initramfs hooks, cmdline.txt modification, TGA validation. No alternative exists for early kernel-stage splash on Pi. |
| `imagemagick` | 8:7.1.x (Trixie) | Convert splash PNG to TGA format | Required by `configure-splash` for TGA conversion. ImageMagick 7 on Trixie uses `magick` command (not `convert`). The `rpi-splash-screen-support` docs reference `convert` but the installer should use `magick` for forward compatibility. |

### Already Installed (No Change)

| Package | Role in This Milestone |
|---------|----------------------|
| `swaybg` | Compositor-owned Wayland splash surface. Already in installer package list. |
| `inotify-tools` | Wayland socket wait in preflight. Already installed. |

### NOT Adding

| Package | Why Not |
|---------|---------|
| `plymouth` | Plymouth is the default RPi OS boot splash (the spinning dots). We are REPLACING it with the kernel-level `fullscreen_logo` approach via `rpi-splash-screen-support`, which displays earlier and doesn't require userspace init. Plymouth will be disabled, not used. |
| `cage` | Dedicated kiosk compositor. labwc maintainers recommend it for kiosk use, but we already depend on labwc for multi-touch config (`mouseEmulation="no"`) and it is the RPi OS default. Switching compositors adds risk for zero benefit -- labwc works fine as a kiosk when stripped down. |
| `swaylock` / `swayidle` | No lock screen or idle timeout needed in a car head unit. Screen blanking is managed by the app's brightness control (sysfs/ddcutil/software). |
| `wlopm` | DPMS control tool. Not needed -- car head units don't sleep the display. |
| `greetd` | Alternative display manager. LightDM is already installed and working. No reason to switch. |

---

## Feature-by-Feature Stack Breakdown

### 1. RPi Boot Splash (Kernel-Level)

**Package:** `rpi-splash-screen-support`
**Confidence:** MEDIUM -- known open issue with console text overlay

**What it does:** Installs a TGA image into initramfs and sets kernel parameters to display it fullscreen during early boot, replacing the default four-raspberry logo and Plymouth spinner.

**Files modified by `configure-splash`:**
| File | Change |
|------|--------|
| `/lib/firmware/logo.tga` | Splash image (copied from converted PNG) |
| `/etc/initramfs-tools/hooks/splash-screen-hook.sh` | initramfs hook to include logo |
| `/boot/firmware/cmdline.txt` | Adds: `fullscreen_logo=1 fullscreen_logo_name=logo.tga vt.global_cursor_default=0` |
| `/boot/firmware/cmdline.txt` | Removes: `console=tty1`, `plymouth.ignore-serial-consoles`, `quiet` |

**TGA conversion command:**
```bash
# ImageMagick 7 (Trixie) -- use 'magick', not 'convert'
magick assets/splash.png -flip -colors 224 -depth 8 -type TrueColor \
  -alpha off -compress none -define tga:bits-per-sample=8 /tmp/logo.tga
sudo configure-splash /tmp/logo.tga
```

**TGA requirements:**
- 24-bit uncompressed TGA
- Max 1920x1080
- Fewer than 224 colors
- Image is vertically flipped (TGA format stores bottom-to-top)

**Existing splash asset:** `assets/splash.png` (500x500 RGB). Needs scaling to target resolution (1024x600) with black padding or centered on black background.

**Known issue (CRITICAL):** [GitHub raspberrypi/linux#7081](https://github.com/raspberrypi/linux/issues/7081) -- when `configure-splash` removes `console=tty1` from cmdline.txt, kernel boot messages can overlay the splash image. The issue is open and unresolved. Workarounds:
- `loglevel=0` (suppress all kernel messages) -- needs testing
- `console=tty3` (redirect to invisible VT) -- partially works
- Accept brief text flash and rely on speed (boot messages are fast on Pi 4)

**What the installer must do:**
1. Install `rpi-splash-screen-support` and `imagemagick`
2. Convert `assets/splash.png` to TGA with correct parameters
3. Run `sudo configure-splash /tmp/logo.tga`
4. Add `loglevel=0` to cmdline.txt (suppress kernel messages)
5. Disable Plymouth: `sudo systemctl mask plymouth-start.service plymouth-quit.service plymouth-quit-wait.service`
6. Rebuild initramfs: `sudo update-initramfs -u` (configure-splash may do this automatically)

**Current cmdline.txt:** `console=serial0,115200 console=tty1 root=PARTUUID=... rootfstype=ext4 fsck.repair=yes rootwait quiet splash plymouth.ignore-serial-consoles cfg80211.ieee80211_regdom=UM`

**After configure-splash:** `console=serial0,115200 root=PARTUUID=... rootfstype=ext4 fsck.repair=yes rootwait fullscreen_logo=1 fullscreen_logo_name=logo.tga vt.global_cursor_default=0 loglevel=0 cfg80211.ieee80211_regdom=UM`

### 2. Custom LightDM/XDG Kiosk Session

**No new packages.** Pure configuration.
**Confidence:** HIGH -- XDG session format is a stable freedesktop.org standard

**New file:** `/usr/share/wayland-sessions/openauto-kiosk.desktop`
```ini
[Desktop Entry]
Name=OpenAuto Kiosk
Comment=OpenAuto Prodigy kiosk session
Exec=labwc -C /etc/openauto-kiosk/labwc
Type=Application
DesktopNames=labwc
```

**Key design decision:** Use labwc's `-C` flag to point to a completely separate config directory (`/etc/openauto-kiosk/labwc/`) rather than modifying the user's `~/.config/labwc/`. This means:
- The kiosk session and desktop session use different labwc configs
- Switching sessions doesn't require config file swaps
- The user's desktop labwc config remains untouched
- System-level config at `/etc/openauto-kiosk/labwc/` is owned by the installer

**LightDM autologin override:**
```ini
# /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf
[Seat:*]
autologin-user=matt
autologin-session=openauto-kiosk
```

Use a drop-in file in `lightdm.conf.d/` rather than modifying `lightdm.conf` directly. This is idempotent and doesn't clobber RPi OS defaults.

**User must be in autologin group:** `sudo gpasswd -a matt autologin` (installer already handles this or should verify).

**Revert path:** To return to desktop, either:
- Delete the drop-in: `sudo rm /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf`
- Or change `autologin-session=rpd-labwc`
- Or use `dm-tool switch-to-greeter` from within the running session

### 3. Labwc Kiosk Config (No Panel/Taskbar)

**No new packages.** Configuration files only.
**Confidence:** HIGH -- labwc config format documented and stable

**Directory:** `/etc/openauto-kiosk/labwc/` (system-level, owned by installer)

**File 1: `/etc/openauto-kiosk/labwc/rc.xml`**
```xml
<?xml version="1.0"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
  <touch deviceName="" mapToOutput="" mouseEmulation="no"/>
  <core>
    <decoration>server</decoration>
    <gap>0</gap>
  </core>
  <theme>
    <cornerRadius>0</cornerRadius>
    <keepBorder>no</keepBorder>
  </theme>
</openbox_config>
```

Minimal config: no decorations to interfere, no corner radius, no borders. `mouseEmulation="no"` carried forward from existing labwc config (critical for multi-touch).

**File 2: `/etc/openauto-kiosk/labwc/autostart`**
```bash
# Splash: show branded image immediately as first Wayland surface
swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &

# Start the app (systemd user service or direct exec)
# The app will kill swaybg after its first frame renders
systemctl --user start openauto-prodigy.service &
```

No `wf-panel-pi`. No `pcmanfm --desktop`. No `lwrespawn`. No `kanshi` (single display, no rotation needed on 1024x600).

**File 3: `/etc/openauto-kiosk/labwc/environment`**
```bash
# Ensure correct Wayland display for child processes
XDG_CURRENT_DESKTOP=labwc
```

**What this removes vs. default RPi desktop session:**
| Component | Default Session | Kiosk Session |
|-----------|----------------|---------------|
| `wf-panel-pi` | Running (taskbar) | Not started |
| `pcmanfm --desktop` | Running (desktop icons/wallpaper) | Not started |
| `lwrespawn` | Wraps panel/desktop for crash recovery | Not started |
| Window decorations | Titlebars, borders | Suppressed via rc.xml |
| Desktop wallpaper | pcmanfm-managed | swaybg splash (temporary) |

### 4. swaybg as Compositor-Owned Splash Client

**Already installed.** No new package.
**Confidence:** HIGH -- swaybg is simple, well-tested, already used by this project

**How it works in the kiosk session:**
1. labwc starts (from session .desktop `Exec=labwc -C /etc/openauto-kiosk/labwc`)
2. labwc runs autostart script
3. `swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &` starts immediately
4. swaybg uses `wlr-layer-shell` protocol to render as background -- it is the FIRST visual surface
5. Screen shows branded splash within ~100ms of compositor start
6. openauto-prodigy starts behind the splash (invisible until splash is dismissed)
7. App renders first frame, detects via `frameSwapped`, kills swaybg
8. User sees seamless transition from splash to app

**Splash image path:** `/usr/share/openauto-prodigy/splash.png` (installed by installer, same source as `assets/splash.png` but deployed to a system path).

**swaybg scaling modes:**
- `fill` -- scale and crop to fill screen (no letterboxing). Best for branded splash.
- `fit` -- scale to fit, letterbox with solid color. Alternative if aspect ratio matters.
- `center` -- no scaling, centered. For pixel-perfect logos on exact-res displays.

Use `fill` for the splash. The 500x500 source will be center-cropped to 1024x600, which is acceptable for a logo on a dark background.

**Why swaybg and not a custom Wayland client:** swaybg already does exactly this. It uses `wlr-layer-shell` (which labwc supports), renders immediately, and is trivially killable. Writing a custom Wayland client for this would be pointless complexity.

**Lifecycle ownership:** labwc owns swaybg (started in autostart). The app kills it. If the app crashes, swaybg stays visible -- which is better than a bare black screen or a desktop the user shouldn't see.

### 5. Wayland Frame Presentation Signal for Splash Handoff

**No new packages.** ~10 lines of C++ in `main.cpp`.
**Confidence:** HIGH -- `QQuickWindow::frameSwapped()` is documented stable Qt API since Qt 5.x

**Signal:** `QQuickWindow::frameSwapped()`
- Emitted when a frame has been queued for presenting
- On Wayland, this corresponds to the compositor receiving the buffer
- Emitted from the scene graph rendering thread (use `Qt::QueuedConnection` if crossing threads)

**Implementation approach:**
```cpp
// In main.cpp, after engine.load()
QObject::connect(window, &QQuickWindow::frameSwapped, window, [&]() {
    static bool splashDismissed = false;
    if (!splashDismissed) {
        splashDismissed = true;
        // Small delay to ensure the frame is actually visible
        QTimer::singleShot(200, []() {
            QProcess::startDetached("pkill", {"swaybg"});
        });
    }
}, Qt::QueuedConnection);
```

**Why 200ms delay:** The `frameSwapped` signal means the frame is queued, not necessarily scanned out to the display. A small delay ensures the user doesn't see a flash of black between swaybg dying and the app's frame appearing. 200ms is below human perception of "gap" but above the compositor's buffer swap latency.

**Why `pkill swaybg` and not a PID file:** swaybg is the only instance running. `pkill` is simpler than tracking PIDs across process boundaries. The kiosk session runs nothing else that matches "swaybg".

**Alternative considered:** `SIGTERM` via PID from environment variable. More correct but adds complexity (autostart writes PID to file, app reads file, sends signal). Not worth it for a single-process kill.

**What NOT to use:**
- `afterRendering()` -- too early (commands recorded but not submitted to GPU)
- `QTimer` with fixed delay -- fragile, hardware-dependent, no guarantee frame exists
- Wayland `wp_presentation` protocol -- Qt 6.8 doesn't expose this to QML/C++ directly

### 6. Session Switching (Kiosk to Desktop) at Runtime

**No new packages.** Uses existing `dm-tool` from LightDM.
**Confidence:** MEDIUM -- dm-tool with Wayland sessions works but the VT switch can be jarring

**Mechanism:** `dm-tool switch-to-user USERNAME SESSION`

```bash
# From within the kiosk session, switch to desktop:
dm-tool switch-to-user matt rpd-labwc
```

**Integration point:** The 3-finger gesture overlay already exists in the app. Add an "Exit to Desktop" button that:
1. Calls `QProcess::startDetached("dm-tool", {"switch-to-user", username, "rpd-labwc"})`
2. LightDM switches to VT 8 (or next free VT), starts a new rpd-labwc session
3. The kiosk session on VT 7 remains (LightDM manages it)

**Returning to kiosk:** `dm-tool switch-to-user matt openauto-kiosk` from the desktop, or:
- A desktop shortcut (.desktop file) that runs this command
- The existing `openauto-prodigy.desktop` in `pi-config/` could be repurposed

**Alternative: Full session logout**
Instead of `dm-tool switch-to-user` (which keeps both sessions alive), the app could:
```bash
# Kill labwc (this ends the kiosk session, LightDM shows greeter or autologins)
# Then LightDM could be configured to start rpd-labwc as fallback
pkill labwc
```
This is simpler but loses the kiosk session state. For a head unit, state loss is acceptable (the app restarts clean anyway).

**Recommended approach:** `dm-tool switch-to-user` for session switching. It preserves the kiosk session so returning is instant. The desktop session is a maintenance/escape hatch, not the normal flow.

**VT behavior note:** LightDM's greeter runs on VT 7 (X11). Wayland sessions launch on VT 8+. Switching between Wayland sessions may cause a brief black flash as VTs change. This is a LightDM limitation, not fixable at the app level.

---

## Config File Summary

All new files created by the installer:

| File | Purpose | Owner |
|------|---------|-------|
| `/usr/share/wayland-sessions/openauto-kiosk.desktop` | XDG session definition | root (system) |
| `/etc/openauto-kiosk/labwc/rc.xml` | Kiosk labwc config (no panel, no borders) | root (system) |
| `/etc/openauto-kiosk/labwc/autostart` | Splash launch + app start | root (system) |
| `/etc/openauto-kiosk/labwc/environment` | Environment variables for kiosk | root (system) |
| `/etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf` | Autologin into kiosk session | root (system) |
| `/usr/share/openauto-prodigy/splash.png` | Splash image for swaybg | root (system) |
| `/lib/firmware/logo.tga` | Kernel boot splash (via configure-splash) | root (system) |

**Existing files modified:**
| File | Change | By |
|------|--------|---|
| `/boot/firmware/cmdline.txt` | fullscreen_logo params, remove plymouth/console | `configure-splash` tool |

---

## Integration Points with Existing Code

### Installer Changes (install.sh)

| Area | Change |
|------|--------|
| Package list | Add `rpi-splash-screen-support`, `imagemagick` |
| New function | `configure_boot_splash()` -- TGA convert, configure-splash, disable plymouth |
| New function | `create_kiosk_session()` -- write session .desktop, labwc configs, LightDM drop-in |
| Existing function | `configure_labwc()` -- still sets mouseEmulation for desktop session; kiosk rc.xml is separate |
| Existing function | `create_service()` -- systemd service may need adjustment for kiosk autostart launch vs. direct exec |

### App Code Changes (main.cpp)

| Change | Lines | Complexity |
|--------|-------|------------|
| Connect `frameSwapped` signal to kill swaybg | ~8 | Trivial |
| Check environment variable to know if splash is active | ~3 | Trivial |

### Overlay Changes (GestureOverlay.qml)

| Change | Lines | Complexity |
|--------|-------|------------|
| Add "Exit to Desktop" button to 3-finger overlay | ~15 QML | Low |
| `QProcess::startDetached("dm-tool", ...)` call | ~5 C++ | Trivial |

---

## Systemd Service vs. Autostart Launch

**Current model:** systemd user service (`openauto-prodigy.service`) launched by... the service itself being enabled and started. The kiosk session changes the launch point.

**Two options for app launch in kiosk mode:**

| Option | Pros | Cons |
|--------|------|------|
| **A: labwc autostart** (script in autostart file) | Simple, labwc owns the lifecycle, splash handoff is natural | Loses sd_notify watchdog, restart-on-failure, journal logging |
| **B: systemd user service** (started from autostart) | Keeps all existing service features (watchdog, restart, logging) | Extra indirection, need to ensure WAYLAND_DISPLAY is set |

**Recommendation: Option B** -- keep the systemd service. The autostart script starts the service:
```bash
swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &
systemctl --user start openauto-prodigy.service &
```

The service needs `Environment=WAYLAND_DISPLAY=wayland-0` (already set in the current unit). This preserves all existing watchdog, restart, and logging behavior. The only change is that the service isn't auto-started on graphical.target -- it's started by the kiosk autostart script, which ensures labwc and swaybg are ready first.

**ExecStartPre panel kill:** The current service has `ExecStartPre=-/bin/sh -c 'pkill wf-panel-pi'`. In kiosk mode, wf-panel-pi never starts, so this is a harmless no-op. No change needed.

---

## Confidence Assessment

| Area | Confidence | Reason |
|------|------------|--------|
| XDG session file | HIGH | Stable freedesktop.org standard, well-documented |
| labwc `-C` config dir | HIGH | Documented in official labwc man page |
| LightDM autologin-session | HIGH | ArchWiki + Gentoo wiki + official docs agree |
| swaybg as splash | HIGH | Already installed, already understood, trivial usage |
| frameSwapped signal | HIGH | Qt 6 stable API since Qt 5, documented behavior |
| dm-tool session switching | MEDIUM | Works in theory, VT switching with Wayland can be flaky |
| rpi-splash-screen-support | MEDIUM | Package exists and works, but kernel issue #7081 (console text overlay) is unresolved. Need hardware testing. |
| Plymouth disable | MEDIUM | Masking units should work, but Trixie has had Plymouth issues recently (forums report broken splash after updates) |
| cmdline.txt modification | MEDIUM | `configure-splash` handles this, but the tool was designed for Bookworm and may need verification on Trixie |

---

## Risk Register

| Risk | Severity | Mitigation |
|------|----------|------------|
| Kernel boot messages overlay splash (issue #7081) | Medium | Try `loglevel=0` + `console=tty3`. Accept brief text if needed -- boots fast on Pi 4. |
| `configure-splash` not in Trixie apt repos | Low | Package is in the RPi apt repo (not Debian main). Should be available. Fallback: manual cmdline.txt edit + initramfs hook. |
| LightDM VT switching causes black flash | Low | Inherent to display manager architecture. Brief flash during session switch is acceptable for a maintenance escape hatch. |
| swaybg doesn't exit cleanly on pkill | Very Low | swaybg handles SIGTERM gracefully. Well-tested. |
| labwc `-C` doesn't override autostart | Very Low | Documented behavior. Autostart is read from the `-C` directory. |

---

## Sources

### Official / Authoritative
- [rpi-splash-screen-support GitHub](https://github.com/raspberrypi/rpi-splash-screen-support) -- configure-splash tool, TGA requirements (MEDIUM confidence -- repo exists but Trixie testing unclear)
- [rpi-splash-screen-support/configure-splash](https://github.com/raspberrypi/rpi-splash-screen-support/blob/master/configure-splash) -- full script logic with file paths and kernel params (HIGH confidence)
- [labwc configuration man page](https://labwc.github.io/labwc-config.5.html) -- config file locations, format, `-C` flag (HIGH confidence)
- [labwc command-line options](https://labwc.github.io/labwc.1.html) -- `-C`, `-S`, `-s` flags (HIGH confidence)
- [QQuickWindow::frameSwapped() Qt 6 docs](https://doc.qt.io/qt-6/qquickwindow.html) -- signal timing and behavior (HIGH confidence)
- [LightDM ArchWiki](https://wiki.archlinux.org/title/LightDM) -- autologin-session, dm-tool, wayland-sessions directory (HIGH confidence)
- [dm-tool man page](https://www.mankier.com/1/dm-tool) -- switch-to-user, switch-to-greeter commands (HIGH confidence)
- [swaybg man page](https://man.archlinux.org/man/swaybg.1.en) -- image modes, command-line options (HIGH confidence)
- [Debian Trixie imagemagick package](https://packages.debian.org/trixie/imagemagick) -- version 7, `magick` command (HIGH confidence)

### Community / Issue Trackers
- [raspberrypi/linux#7081](https://github.com/raspberrypi/linux/issues/7081) -- fullscreen_logo console text overlay issue (MEDIUM confidence -- open issue, unresolved)
- [labwc kiosk discussion #2301](https://github.com/labwc/labwc/discussions/2301) -- labwc kiosk config patterns, windowRule for fullscreen (MEDIUM confidence)
- [RPi Forum: Kiosk with labwc](https://forums.raspberrypi.com/viewtopic.php?t=378883) -- autostart patterns, disabling panel (MEDIUM confidence)
- [RPi Forum: Autostart with labwc](https://forums.raspberrypi.com/viewtopic.php?t=379321) -- /etc/xdg/labwc/autostart format (MEDIUM confidence)

### Codebase (Primary)
- `install.sh` -- existing installer structure, package list, labwc config, systemd service (HIGH confidence)
- `docs/pi-config/` -- reference configs for Pi deployment (HIGH confidence)
- `assets/splash.png` -- 500x500 RGB splash image source (HIGH confidence)
- `.planning/notes/kiosk-session-splash.md` -- Codex architecture notes (HIGH confidence -- project-internal)

---

*Stack research for: OpenAuto Prodigy v0.7.0 -- Kiosk Session & Boot Experience*
*Researched: 2026-03-22*
