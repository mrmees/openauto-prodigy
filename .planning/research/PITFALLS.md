# Domain Pitfalls

**Domain:** Kiosk session, boot splash, compositor splash handoff, session switching on RPi OS Trixie with labwc
**Researched:** 2026-03-22
**Milestone:** v0.7.0 -- Kiosk Session & Boot Experience
**Confidence:** MEDIUM-HIGH (WebSearch + official docs + project codebase analysis; some areas LOW due to sparse rpi-splash-screen-support documentation)

---

## Critical Pitfalls

Mistakes that cause boot failures, black screens, or force manual SSH recovery.

### Pitfall 1: Black Flash Between Kernel Splash and Compositor Splash

**What goes wrong:** The kernel splash (rpi-splash-screen-support TGA in initramfs) is drawn on the kernel framebuffer. When labwc starts, the DRM/KMS driver reinitializes the display to set up its own framebuffer, erasing the kernel splash. There is a gap -- typically 1-3 seconds on Pi 4 -- between the kernel splash disappearing and the first Wayland surface (swaybg) rendering. During this gap the screen is black.

**Why it happens:** The KMS mode-set is unavoidable. The kernel framebuffer and the compositor's DRM framebuffer are different surfaces. When the compositor takes ownership of the DRM device, the kernel's framebuffer contents are discarded. The swaybg client must then: connect to the compositor's Wayland socket, allocate a wl_buffer, attach it, and commit -- all of which takes nonzero time after the compositor is ready.

**Consequences:** User sees: branded splash -> black -> compositor splash -> app. The black gap defeats the purpose of a seamless boot experience.

**Prevention:**
- Accept the gap exists and minimize it rather than eliminate it. This is the same problem Plymouth solves on desktop Linux, and even Plymouth has a handoff moment.
- Start swaybg as the FIRST command in labwc autostart (before anything else) and background it. This minimizes the gap to the time swaybg takes to connect + render (~200-500ms on Pi 4).
- Use `quiet splash logo.nologo loglevel=0 vt.global_cursor_default=0` in cmdline.txt so the gap shows solid black (no text, no cursor, no kernel messages) rather than noisy output.
- Do NOT attempt to "hold" the kernel framebuffer image with a userspace fbdev tool (fbi, etc.) -- once KMS takes over, fbdev writes are ignored or cause conflicts.
- Phase this as "good enough for v0.7.0" with a note that boot speed optimization is a future milestone.

**Detection:** Boot the Pi and watch the display. If you see kernel messages, boot text, or a cursor between splash and app, the cmdline.txt parameters are wrong.

**Confidence:** HIGH -- this is a well-documented DRM/KMS handoff limitation, confirmed by Raspberry Pi forum reports.

---

### Pitfall 2: raspi-config Overwrites Custom Session Selection

**What goes wrong:** The installer configures LightDM to autologin into `openauto-kiosk` session. Later, the user runs `raspi-config` (System Options -> Boot / Auto Login) and raspi-config rewrites `/etc/lightdm/lightdm.conf` with `autologin-session=rpd-labwc`, silently undoing the kiosk session. Next boot shows the full RPi desktop instead of the kiosk.

**Why it happens:** raspi-config directly modifies `/etc/lightdm/lightdm.conf` and `/var/lib/AccountsService/users/$USER`. It has no awareness of custom sessions. It assumes the session is either `rpd-labwc` (Wayland) or `rpd-x` (X11). It will also overwrite the AccountsService user file's `XSession` and `Session` fields.

**Consequences:** User loses kiosk boot without understanding why. They may not even connect the raspi-config interaction to the symptom (which appears on next boot, not immediately).

**Prevention:**
- Use `/etc/lightdm/lightdm.conf.d/` drop-in files (e.g., `50-openauto-kiosk.conf`) instead of modifying `lightdm.conf` directly. Drop-in files in `conf.d/` are read AFTER the main config and override matching keys. raspi-config modifies the main file but does NOT touch `conf.d/`.
- ALSO set the AccountsService user file at `/var/lib/AccountsService/users/$USER` with `Session=openauto-kiosk` -- but document that raspi-config may clobber this.
- Include a health check in the preflight script or installer that warns if the active LightDM session is not `openauto-kiosk`.
- Document prominently: "Do not use raspi-config to change boot/login settings. Use the Prodigy installer to switch between kiosk and desktop modes."

**Detection:** After any `apt upgrade` or raspi-config use, check `grep autologin-session /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf` and `grep Session /var/lib/AccountsService/users/$USER`.

**Confidence:** HIGH -- confirmed behavior from RPi forum reports where AccountsService user files revert after raspi-config runs.

---

### Pitfall 3: Kernel Update Breaks Boot Splash (initramfs Not Rebuilt)

**What goes wrong:** `apt upgrade` installs a new kernel. The new kernel package triggers `update-initramfs` which rebuilds the initramfs. If the splash hook (`/etc/initramfs-tools/hooks/splash-screen-hook.sh`) is missing, malformed, or fails silently, the new initramfs lacks the TGA image. The kernel boots with `fullscreen_logo=1` but finds no `logo.tga` in initramfs -- result is either no splash (black screen + boot text) or a kernel panic if the firmware path is required.

**Why it happens:** The rpi-splash-screen-support `configure-splash` tool installs a hook script at `/etc/initramfs-tools/hooks/splash-screen-hook.sh` that copies `/lib/firmware/logo.tga` into the initramfs. Kernel updates trigger `update-initramfs -u` which re-runs all hooks. If the hook was accidentally deleted, has wrong permissions, or the TGA file at `/lib/firmware/logo.tga` was removed, the rebuild succeeds but produces an initramfs without the splash image.

**Consequences:** Silent degradation -- boot splash disappears after an apt upgrade. User sees boot text or black screen but the system still boots. Hard to diagnose because "it worked before the update."

**Prevention:**
- Installer must verify the hook script exists AND is executable after running `configure-splash`.
- Installer health check should verify `/lib/firmware/logo.tga` exists and `/etc/initramfs-tools/hooks/splash-screen-hook.sh` exists and is executable.
- Consider adding a dpkg trigger or apt hook that re-runs `configure-splash` after kernel package installation (defense in depth).
- Test the splash by running `sudo update-initramfs -u` manually during installer verification, then rebooting.

**Detection:** After `apt upgrade`, check `lsinitramfs /boot/firmware/initrd.img-$(uname -r) | grep logo.tga`. If empty, the splash image was not included.

**Confidence:** MEDIUM -- rpi-splash-screen-support is relatively new and has 4 open issues. The `update-initramfs -u -k all` command has been confirmed to break splash screens on RPi forums.

---

### Pitfall 4: labwc Autostart Runs Before Dependent Services Are Ready (First Boot Race)

**What goes wrong:** On first boot after power-on, labwc's autostart script executes before D-Bus, PipeWire, or BlueZ are fully initialized. swaybg may fail to connect to the compositor socket (timing), or the main app may start before PipeWire/BlueZ are ready and fail to initialize audio/bluetooth.

**Why it happens:** labwc autostart runs as a shell script immediately after the compositor reads its config. There is no dependency management -- it does not wait for systemd targets or D-Bus services. On first boot, systemd is still bringing up user services in parallel. On subsequent logins (without reboot), these services are already running.

**Consequences:** On first boot: swaybg might not appear (splash missing), app might crash or degrade (no audio, no bluetooth). On subsequent reboots: works fine, making the bug intermittent and hard to reproduce.

**Prevention:**
- swaybg is simple enough that it should work reliably -- it only needs the Wayland socket which labwc guarantees before autostart. Verify this on Pi hardware.
- The existing systemd service (`openauto-prodigy.service`) already has `After=graphical.target hostapd.service bluetooth.target pipewire.service` -- this is CORRECT. Do NOT move app launch from the systemd service to labwc autostart. Keep the service file as the app launcher; use autostart ONLY for the splash (swaybg) and other compositor-level concerns.
- If swaybg occasionally fails on cold boot, add a small retry: `swaybg ... & sleep 0.3 && pgrep swaybg || swaybg ... &`

**Detection:** Cold-boot the Pi 5 times and check if splash appears every time. If it fails even once, there's a race.

**Confidence:** MEDIUM -- confirmed as a general labwc autostart issue in GitHub discussions, but swaybg specifically is lightweight enough that it may not be affected.

---

## Moderate Pitfalls

### Pitfall 5: swaybg Renders on Background Layer -- App Window Covers It Automatically

**What goes wrong:** Developer expects to need explicit splash dismissal logic (kill swaybg) to reveal the app. But swaybg uses wlr-layer-shell's `background` layer, which is the LOWEST z-layer. Any normal Wayland toplevel window (like the Qt app) renders ABOVE the background layer by default. The "splash" is effectively invisible as soon as the app window maps.

**Why this is a pitfall (not a feature):** If the developer writes complex splash-dismissal logic based on frameSwapped timing and SIGTERM signals, they're solving a problem that doesn't exist. BUT -- if the app window is transparent or takes time to render its first frame, the splash WILL be visible underneath during that time, which IS the desired behavior. The pitfall is misunderstanding when explicit dismissal is needed vs. when occlusion handles it.

**Prevention:**
- Understand the wlr-layer-shell layer ordering: background < bottom < top < overlay. swaybg runs at `background` layer.
- The Qt app is a normal toplevel surface and renders above background layer. Once the app window maps with content, swaybg is occluded.
- Still kill swaybg after the app is confirmed visible (to free memory and avoid a zombie process), but this is cleanup, not visual handoff.
- The real visual handoff happens automatically via surface stacking. The question is: how long between labwc start and Qt app's first frame? That's the window where the splash is visible and doing its job.

**Detection:** Run `wlr-randr` or check layer-shell surface list to verify swaybg is on background layer.

**Confidence:** HIGH -- wlr-layer-shell protocol defines layer ordering, and swaybg's source code confirms it uses the background layer.

---

### Pitfall 6: frameSwapped Does Not Mean "Frame Is Visible on Screen"

**What goes wrong:** The architecture notes say "App dismisses splash after first frame is presented (frameSwapped signal)." But `QQuickWindow::frameSwapped` fires when the rendering thread has submitted a frame to the GPU/compositor, NOT when the compositor has actually presented it to the display. On Wayland, there is additional latency between the client committing a frame and the compositor compositing + presenting it at the next vblank.

**Why it happens:** Qt's frameSwapped is a client-side signal. The Wayland protocol's `wl_surface.frame` callback fires when the compositor is READY for the next frame, not when the previous frame was displayed. There is no reliable Wayland client API to know "my frame is now physically on the monitor" (the `wp_presentation` protocol exists but is not universally supported and adds complexity).

**Consequences:** If splash is killed on frameSwapped, there may be a 1-2 frame gap (16-33ms at 60Hz) where the splash is gone but the app's first frame hasn't been presented yet. On Pi 4, this is a brief black flash between splash and app.

**Prevention:**
- Add a small delay after frameSwapped before killing swaybg. The architecture notes mention 300ms -- this is reasonable and conservative.
- Since swaybg is on the background layer (Pitfall 5), it's actually occluded by the app window, so killing it is cosmetic cleanup. The visual handoff happens when the app window maps, not when swaybg dies.
- The practical approach: connect to `frameSwapped`, use `QTimer::singleShot(500, ...)` to kill swaybg. This ensures the app has had multiple frames presented before the background process is cleaned up.
- Do NOT use `QQuickWindow::afterRendering` -- this fires even EARLIER in the pipeline (before the frame is submitted to the compositor).

**Detection:** Known project gotcha already documented in milestone context. Test on Pi hardware at 60Hz -- if there's a flash between splash and app, increase the delay.

**Confidence:** HIGH -- Qt documentation explicitly states frameSwapped fires after "the scene has completed rendering, before swapbuffers is called" (Qt 5) or after buffer swap but before compositor presentation (Qt 6).

---

### Pitfall 7: Kiosk Session Desktop File Syntax Errors Cause Boot to Black Screen

**What goes wrong:** A syntax error in `/usr/share/wayland-sessions/openauto-kiosk.desktop` (missing `Exec=` line, wrong `Type=`, bad `DesktopNames=`) causes LightDM to fail to start the session. With autologin enabled, LightDM attempts to start the session, fails, and either: (a) shows the greeter (if installed and configured), (b) shows a black screen with a cursor, or (c) enters a login loop.

**Why it happens:** XDG desktop files have strict format requirements. Common mistakes: wrong `Type` (must be `Application`), `Exec` pointing to nonexistent binary, missing `DesktopNames` field, or using X11-specific fields for a Wayland session. LightDM does not provide helpful error messages for malformed session files.

**Consequences:** Pi boots to black screen. User cannot get to a desktop. Recovery requires SSH or serial console to fix the desktop file or revert LightDM config.

**Prevention:**
- Validate the desktop file format in the installer BEFORE enabling autologin:
  ```bash
  desktop-file-validate /usr/share/wayland-sessions/openauto-kiosk.desktop
  ```
- Test that `labwc` can actually be launched from the session before making it the default.
- Installer MUST provide a rollback mechanism: if first boot into kiosk fails, have a systemd timer or watchdog that reverts to the default `rpd-labwc` session after N consecutive failures.
- Keep the default `rpd-labwc.desktop` session file intact -- never modify or delete it.
- Session file template:
  ```ini
  [Desktop Entry]
  Name=OpenAuto Kiosk
  Comment=OpenAuto Prodigy Kiosk Session
  Exec=labwc -C /etc/openauto-kiosk/labwc -s /etc/openauto-kiosk/labwc/autostart
  Type=Application
  DesktopNames=wlroots
  ```

**Detection:** `desktop-file-validate` on the session file. Attempt `sudo -u matt labwc -C /path/to/kiosk/config` from SSH to verify it starts.

**Confidence:** HIGH -- standard XDG/LightDM behavior, well-documented in Arch Wiki and Debian Wiki.

---

### Pitfall 8: Kiosk labwc Config Conflicts with User's ~/.config/labwc

**What goes wrong:** The kiosk session uses a stripped labwc config (no panel, no taskbar). But labwc loads config from `~/.config/labwc/` first, then falls back to `/etc/xdg/labwc/`. If the kiosk session writes its config to `~/.config/labwc/`, the normal desktop session (rpd-labwc) ALSO picks up the kiosk config -- no panel, no taskbar on the regular desktop.

**Why it happens:** labwc has a single config search path per user. There is no built-in "profile" or "session-specific config" mechanism. Both the kiosk session and the normal desktop session read from the same `~/.config/labwc/` directory.

**Consequences:** Switching from kiosk to desktop gives a broken desktop (no taskbar, no file manager). User thinks the system is broken.

**Prevention:**
- Do NOT put kiosk config in `~/.config/labwc/`. Use labwc's `-C` flag to specify an alternative config directory:
  ```
  Exec=labwc -C /etc/openauto-kiosk/labwc
  ```
  This tells labwc to look at `/etc/openauto-kiosk/labwc/rc.xml`, `/etc/openauto-kiosk/labwc/autostart`, etc., leaving `~/.config/labwc/` untouched for the normal desktop session.
- The kiosk config directory should contain: `rc.xml` (with mouseEmulation="no", ToggleFullscreen windowRule for openauto), `autostart` (swaybg launch + app service trigger), and `environment` (if needed).
- The existing `~/.config/labwc/rc.xml` with `mouseEmulation="no"` (created by the current installer) should remain -- it's needed for both sessions.

**Detection:** After switching sessions, verify `wf-panel-pi` is running in the desktop session and NOT running in the kiosk session.

**Confidence:** HIGH -- labwc documentation confirms config search path behavior and `-C` override flag.

---

### Pitfall 9: Exit-to-Desktop Requires Full Session Switch, Not Just App Kill

**What goes wrong:** The 3-finger overlay "Exit to Desktop" button kills the app process. But in the kiosk session, there IS no desktop -- labwc was launched with a stripped config (no panel, no pcmanfm, no wallpaper daemon). Killing the app leaves the user staring at a black screen with nothing to interact with.

**Why it happens:** In the normal rpd-labwc session, killing the app reveals the desktop environment underneath (wf-panel-pi, pcmanfm, etc.). In the kiosk session, there is no "underneath" -- the compositor only has swaybg (if still running) and the app. Removing the app removes everything useful.

**Consequences:** User presses "Exit to Desktop" and gets a black screen. Only recovery is SSH or physical reboot.

**Prevention:**
- "Exit to Desktop" must actually switch sessions, not just kill the app. The flow should be:
  1. App requests session switch via a script/command.
  2. Script terminates labwc (which returns control to LightDM).
  3. LightDM starts the `rpd-labwc` session instead.
- Implementation options:
  - **Option A:** Write a helper script that modifies LightDM's autologin-session and then sends SIGTERM to labwc (PID from `$LABWC_PID`). Next login starts the desktop session.
  - **Option B:** Use `loginctl terminate-session` to end the current session, triggering LightDM to restart. Pre-configure the next session to be `rpd-labwc`.
  - **Option C (simplest):** Write the desired session to `/var/lib/AccountsService/users/$USER` (Session=rpd-labwc), then send SIGTERM to labwc. LightDM autologins into the new session.
- Must also have a "Return to Kiosk" mechanism from the desktop (run a command or use raspi-config alternative).

**Detection:** Press Exit to Desktop in kiosk mode. If you see a black screen instead of the desktop, the session switch is broken.

**Confidence:** HIGH -- this is architectural, not implementation-dependent. Labwc's `--startup` option can help: if the Exec line uses `labwc --startup /path/to/app`, labwc exits when the startup command exits, returning to LightDM.

---

### Pitfall 10: systemd Service vs. labwc autostart -- Double Launch or Missing Launch

**What goes wrong:** The app currently launches via a systemd user service (`openauto-prodigy.service`) with `After=graphical.target`. The kiosk session's labwc autostart might ALSO try to launch the app. Result: either the app launches twice (crashes on port/socket conflicts) or neither approach works (systemd service waits for graphical.target which depends on the session which depends on autostart which waits for...).

**Why it happens:** There are two competing launch mechanisms: systemd (service file, dependency ordering, sd_notify) and labwc autostart (shell script, no dependency management). The current installer creates the systemd service. The kiosk session may tempt adding the app to autostart. Using both creates conflicts.

**Consequences:** Double launch: socket bind fails, IPC conflicts, crashes. Or circular dependency: service waits for graphical.target, graphical.target waits for labwc, labwc autostart waits for nothing but the service never starts because graphical.target isn't reached yet.

**Prevention:**
- Keep ONE launch mechanism. The systemd service is the right choice because it provides:
  - Dependency ordering (`After=` hostapd, bluetooth, pipewire)
  - Restart-on-failure
  - WatchdogSec
  - sd_notify readiness
  - Clean ExecStartPre/ExecStopPost hooks
- labwc autostart should launch ONLY compositor-level concerns: swaybg (splash) and potentially kanshi (display config). NOT the app.
- The autostart file should look like:
  ```bash
  swaybg -m fill -i /usr/share/openauto-prodigy/splash.png &
  ```
  That's it. The systemd service handles app launch.
- Verify that `graphical.target` is reached BEFORE the systemd service tries to start. On RPi OS Trixie with LightDM + labwc, `graphical.target` is reached when the display manager starts a session. The service's `After=graphical.target` should work correctly.

**Detection:** `systemctl status openauto-prodigy.service` should show the service running. `pgrep openauto-prodigy` should show exactly ONE process. If zero or two, the launch mechanism is wrong.

**Confidence:** HIGH -- this is a known class of problem. The existing service file already works correctly; the pitfall is introducing a second launch path.

---

### Pitfall 11: wf-panel-pi Lifecycle Breaks on Session Switch

**What goes wrong:** The current systemd service kills wf-panel-pi on start and restores it on clean stop (via ExecStartPre/ExecStopPost). In the kiosk session, wf-panel-pi is never started (no lwrespawn in autostart). The ExecStartPre that kills wf-panel-pi is harmless (nothing to kill), but the ExecStopPost that restores it will try to start wf-panel-pi in a session that has no panel infrastructure -- either failing silently or leaving orphan processes.

**Why it happens:** The ExecStopPost logic assumes the app is running in the default rpd-labwc session where wf-panel-pi should exist. In the kiosk session, restoring wf-panel-pi is wrong.

**Consequences:** Orphan wf-panel-pi process in kiosk session after app restart, or failed restore attempts cluttering journal logs.

**Prevention:**
- Make the panel restore conditional on the current session:
  ```bash
  ExecStopPost=-/bin/sh -c '[ "$SERVICE_RESULT" = "success" ] && \
    [ "$(cat /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf 2>/dev/null | grep autologin-session)" != "autologin-session=openauto-kiosk" ] && \
    systemd-run --user ... /usr/bin/lwrespawn /usr/bin/wf-panel-pi || true'
  ```
- Or simpler: check if the current session is the kiosk session by looking at `$XDG_CURRENT_DESKTOP` or labwc's config path.
- Best approach: move wf-panel-pi suppression entirely to the session definition. In the kiosk session, wf-panel-pi is simply not in autostart. In the desktop session, wf-panel-pi is in autostart. No need for the service to manage it at all -- the kiosk session naturally excludes it.

**Detection:** After app crash/restart in kiosk mode, check `pgrep wf-panel-pi`. Should be zero in kiosk session.

**Confidence:** HIGH -- follows directly from existing codebase analysis.

---

## Minor Pitfalls

### Pitfall 12: TGA Image Conversion Gotchas

**What goes wrong:** The splash image must be: uncompressed TGA, 24-bit (no alpha), less than 224 colors, smaller than 1920x1080. Common failure: PNG with alpha channel -> 32-bit TGA (rejected). Common failure: complex image with >224 colors (quantization artifacts). Common failure: image is not vertically flipped (TGA origin convention).

**Prevention:**
- Use the exact ImageMagick conversion command:
  ```bash
  convert logo.png -flip -colors 224 -depth 8 -type TrueColor \
    -alpha off -compress none -define tga:bits-per-sample=8 logo.tga
  ```
- Note the `-flip` -- TGA format stores images bottom-to-top by default. Without it, the splash appears upside-down.
- Validate with `file logo.tga` (should say "Targa image data") and check dimensions with `identify logo.tga`.
- Bundle a pre-converted TGA in the repo assets. Do NOT rely on the user having ImageMagick installed.

**Confidence:** HIGH -- ImageMagick conversion is documented in rpi-splash-screen-support README.

---

### Pitfall 13: `configure-splash` Removes `quiet` from cmdline.txt

**What goes wrong:** The `configure-splash` script explicitly removes `quiet` and `plymouth.ignore-serial-consoles` from cmdline.txt before adding its own parameters. If the installer had previously set `quiet` for clean boot output, running `configure-splash` removes it. Boot may then show kernel messages before the splash appears.

**Prevention:**
- After running `configure-splash`, re-add `quiet loglevel=0 logo.nologo` to cmdline.txt.
- Or use `configure-splash --no-cmdline` and manage cmdline.txt parameters manually in the installer.
- The full desired cmdline.txt additions: `quiet loglevel=0 logo.nologo vt.global_cursor_default=0 fullscreen_logo=1 fullscreen_logo_name=logo.tga`

**Confidence:** MEDIUM -- confirmed by reading the configure-splash script source. The removal of `quiet` is intentional (the tool assumes it knows best about boot parameters).

---

### Pitfall 14: labwc autostart `&` Requirement for Background Processes

**What goes wrong:** Commands in labwc autostart without `&` block the rest of the autostart script. If swaybg is launched without `&`, the app service never starts (systemd-wise this doesn't matter since the service is separate, but any subsequent autostart commands are blocked).

**Prevention:**
- Always background long-running processes in autostart with `&`.
- swaybg MUST be backgrounded: `swaybg -m fill -i /path/to/splash.png &`
- This is different from the old wayfire behavior where everything was automatically backgrounded.

**Confidence:** HIGH -- documented in labwc autostart docs and confirmed by multiple RPi forum reports.

---

### Pitfall 15: labwc Window Rule app_id Mismatch

**What goes wrong:** The kiosk rc.xml has a windowRule with `identifier="openauto-prodigy"` to auto-fullscreen the app. But Qt's Wayland shell uses the application's `desktopFileName` or a compositor-assigned app_id. If the app doesn't set its app_id explicitly, labwc may see a different identifier and the fullscreen rule won't trigger.

**Prevention:**
- Set the app_id explicitly in the Qt application:
  ```cpp
  app.setDesktopFileName("openauto-prodigy");
  ```
  Or set the `QT_WAYLAND_SHELL_INTEGRATION` or use `QGuiApplication::setDesktopFileName()`.
- Verify the actual app_id by running `labwc` with debug logging or using `wlr-foreign-toplevel-management` to inspect running surfaces.
- labwc windowRule matching is case-insensitive and supports wildcards (`*`), so `identifier="openauto*"` is a safer pattern.

**Confidence:** MEDIUM -- Qt's app_id behavior varies between versions. Needs hardware verification.

---

### Pitfall 16: Installer Config Files Overwritten by apt upgrade of labwc/lightdm

**What goes wrong:** Files in `/etc/xdg/labwc/` are owned by the `labwc` package. If the installer modifies `/etc/xdg/labwc/autostart` or `/etc/xdg/labwc/rc.xml`, a `labwc` package upgrade will prompt to overwrite (or silently overwrite if dpkg is configured that way).

**Prevention:**
- NEVER modify package-owned config files. Use these strategies instead:
  - labwc: Use `-C /etc/openauto-kiosk/labwc` to point at a completely separate config directory owned by the installer.
  - LightDM: Use `/etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf` (drop-in directory is safe from package updates).
  - Session file: `/usr/share/wayland-sessions/openauto-kiosk.desktop` is not owned by any package (we create it), so it survives upgrades.
- The kiosk session's Exec line should point labwc at the custom config dir, not rely on the system-wide config.

**Confidence:** HIGH -- standard dpkg conffile behavior.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Boot splash setup | TGA conversion wrong (Pitfall 12), configure-splash removes quiet (Pitfall 13) | Bundle pre-converted TGA, manage cmdline.txt manually |
| Kiosk session creation | Desktop file syntax error causes black screen (Pitfall 7) | Validate with desktop-file-validate, test before enabling |
| LightDM configuration | raspi-config overwrites session (Pitfall 2) | Use conf.d drop-in files, document limitation |
| labwc kiosk config | Config conflicts with desktop session (Pitfall 8) | Use labwc -C flag for separate config directory |
| Splash-to-app handoff | frameSwapped timing (Pitfall 6), layer misunderstanding (Pitfall 5) | Use timer delay for cleanup, understand layer-shell stacking |
| Boot splash to compositor transition | Black flash (Pitfall 1) | Accept gap, minimize with early swaybg launch |
| Exit-to-desktop | Black screen after app exit (Pitfall 9) | Implement full session switch via LightDM |
| App launch mechanism | Double launch from systemd + autostart (Pitfall 10) | Keep systemd service as sole launch mechanism |
| Service lifecycle | wf-panel-pi restore in kiosk (Pitfall 11) | Session-aware panel management |
| Kernel updates | initramfs splash lost (Pitfall 3) | Verify hook + TGA after updates |
| Autostart ordering | Race on first boot (Pitfall 4), missing & (Pitfall 14) | Lightweight autostart, background everything |
| Window rules | app_id mismatch (Pitfall 15) | Set app_id explicitly, use wildcards |
| Config persistence | apt upgrade overwrites (Pitfall 16) | Own config directory, drop-in files |

## Sources

- [rpi-splash-screen-support GitHub](https://github.com/raspberrypi/rpi-splash-screen-support) -- TGA requirements, configure-splash behavior
- [rpi-splash-screen-support configure-splash source](https://github.com/raspberrypi/rpi-splash-screen-support/blob/master/configure-splash) -- cmdline.txt modifications, image validation
- [Splash Screen Configuration (DeepWiki)](https://deepwiki.com/raspberrypi/rpi-image-gen/7.5-splash-screen-configuration) -- initramfs hook details, file modifications
- [labwc configuration docs](https://labwc.github.io/labwc-config.5.html) -- autostart format, environment variables, windowRules
- [labwc rc.xml.all reference](https://github.com/labwc/labwc/blob/master/docs/rc.xml.all) -- windowRule properties and actions
- [labwc kiosk discussion #2301](https://github.com/labwc/labwc/discussions/2301) -- kiosk mode limitations, cage alternative suggestion
- [labwc autostart not running on first boot (discussion #2515)](https://github.com/labwc/labwc/discussions/2515) -- first-boot race condition
- [labwc man page](https://man.archlinux.org/man/labwc.1.en) -- --startup option, SIGTERM exit behavior, LABWC_PID
- [LightDM ArchWiki](https://wiki.archlinux.org/title/LightDM) -- session discovery, conf.d override mechanism
- [LightDM Debian Wiki](https://wiki.debian.org/LightDM) -- configuration file hierarchy
- [Wayland Protocol Book: Surface lifecycle](https://wayland-book.com/surfaces-in-depth/lifecycle.html) -- surface mapping, first frame requirements
- [Wayland Protocol Book: Frame callbacks](https://wayland-book.com/surfaces-in-depth/frame-callbacks.html) -- frame callback semantics
- [wlr-layer-shell protocol](https://wayland.app/protocols/wlr-layer-shell-unstable-v1) -- layer ordering (background/bottom/top/overlay)
- [swaybg GitHub](https://github.com/swaywm/swaybg) -- layer-shell background usage, process lifecycle
- [QQuickWindow documentation (Qt 6)](https://doc.qt.io/qt-6/qquickwindow.html) -- frameSwapped signal semantics
- [RPi Forums: update-initramfs breaks splash](https://forums.raspberrypi.com/viewtopic.php?t=384811) -- kernel update splash breakage
- [RPi Forums: Kiosk tutorial with labwc](https://forums.raspberrypi.com/viewtopic.php?t=378883) -- kiosk mode limitations
- [RPi Forums: Autostart with labwc](https://forums.raspberrypi.com/viewtopic.php?t=379321) -- autostart ordering, backgrounding
- [RPi Forums: Autologin after boot loader upgrade](https://forums.raspberrypi.com/viewtopic.php?t=340632) -- LightDM autologin breakage
- [RPi Forums: Session selection issues](https://forums.raspberrypi.com/viewtopic.php?t=366649) -- AccountsService behavior
- [Raspberry Pi Configuration docs](https://www.raspberrypi.com/documentation/computers/configuration.html) -- boot parameters
- [Weston splashscreen mailing list](https://lists.freedesktop.org/archives/wayland-devel/2017-January/032612.html) -- compositor splash handoff pattern
