# Architecture Patterns

**Domain:** Kiosk session, boot splash, and exit-to-desktop for embedded Qt 6 application on Raspberry Pi
**Researched:** 2026-03-22
**Confidence:** HIGH (direct codebase analysis + official documentation for all components)

## Recommended Architecture

Three-layer boot handoff with a dedicated XDG session as the switching mechanism:

```
Layer 1: Kernel boot splash (rpi-splash-screen-support)
   |  Prodigy logo replaces default raspberries. Visible from first video frame.
   v
Layer 2: Compositor splash (swaybg in labwc autostart)
   |  Same Prodigy logo. Replaces kernel splash seamlessly when labwc starts.
   v
Layer 3: Application (openauto-prodigy)
   |  App initializes behind splash, kills swaybg after first rendered frame.
   v
Running state: App owns display. Splash is dead.
```

Exit-to-desktop is a runtime LightDM session switch using `dm-tool switch-to-user`. The kiosk session terminates cleanly and LightDM starts the standard `rpd-labwc` desktop session for the same user.

### Component Boundaries

| Component | Responsibility | Status | Communicates With |
|-----------|---------------|--------|-------------------|
| **openauto-kiosk.desktop** | XDG session entry for LightDM | NEW | LightDM (session selection) |
| **kiosk labwc config directory** | Stripped labwc config: no panel, no desktop, splash in autostart | NEW | labwc (via `-C` flag) |
| **rpi-splash-screen-support** | Kernel-level boot splash with Prodigy TGA image | NEW (pkg install + config) | Kernel framebuffer |
| **swaybg (splash role)** | Wayland surface covering screen immediately after compositor starts | REPURPOSED (from app child to autostart-owned) | labwc (layer-shell), app (SIGTERM) |
| **ApplicationController** | Gains `exitToDesktop()` method | MODIFY (add one method) | dm-tool (via QProcess) |
| **ActionRegistry** | Register `app.exitToDesktop` action | MODIFY (add one registration) | ApplicationController |
| **GestureOverlay.qml** | Gains "Exit" button in quick controls | MODIFY (add one button) | ActionRegistry |
| **openauto-prodigy.service** | Simplified: no wf-panel-pi kill/restore, no ExecStopPost panel dance | MODIFY | systemd |
| **openauto-preflight** | Unchanged: rfkill, Wayland socket, SDP checks still needed | NO CHANGE | systemd (ExecStartPre) |
| **install.sh** | Creates kiosk session, boot splash, modified service, LightDM config | MODIFY | All of the above |
| **main.cpp** | Splash dismissal via `pkill swaybg` after first frame | MODIFY (add frameSwapped handler) | swaybg process |

### New Files Created by Installer

| File | Location | Purpose |
|------|----------|---------|
| `openauto-kiosk.desktop` | `/usr/share/wayland-sessions/` | XDG session entry |
| `rc.xml` | `/etc/openauto-kiosk/labwc/` | Kiosk labwc config (touch, no decorations) |
| `autostart` | `/etc/openauto-kiosk/labwc/` | Splash + app launch |
| `environment` | `/etc/openauto-kiosk/labwc/` | Wayland env vars |
| `splash.tga` | `/usr/share/openauto-prodigy/` | Boot splash image (24-bit TGA, max 1920x1080) |
| `splash.png` | `/usr/share/openauto-prodigy/` | Wayland splash image (PNG for swaybg) |
| `lightdm.conf.d/50-openauto.conf` | `/etc/lightdm/` | Autologin to kiosk session |

### Files Modified by Installer

| File | Change |
|------|--------|
| `openauto-prodigy.service` | Remove wf-panel-pi kill/restore ExecStartPre/StopPost lines. Keep preflight, keep BindsTo=hostapd. |
| `~/.config/labwc/rc.xml` | Unchanged (normal desktop session still uses this) |

## Integration Analysis

### 1. Dedicated XDG Session (Core Mechanism)

**Current:** LightDM autologins into `rpd-labwc`, which starts labwc with the default config at `~/.config/labwc/`. This loads wf-panel-pi, pcmanfm desktop, and other desktop infrastructure. The systemd service then kills wf-panel-pi before starting the app, and tries to restore it on clean exit.

**New:** LightDM autologins into `openauto-kiosk`, which starts labwc with a purpose-built kiosk config. No panel, no desktop, no file manager. Just splash + app.

**The session file:**
```ini
# /usr/share/wayland-sessions/openauto-kiosk.desktop
[Desktop Entry]
Name=OpenAuto Kiosk
Comment=OpenAuto Prodigy kiosk session
Exec=/usr/bin/labwc -C /etc/openauto-kiosk/labwc
Type=Application
DesktopNames=labwc
```

The `-C` flag tells labwc to use a completely separate config directory. This is the key architectural decision: instead of modifying the user's labwc config or fighting with wf-panel-pi lifecycle, we sidestep the problem entirely with an independent config root.

**Why `-C` and not `XDG_CONFIG_DIRS`:** The `-C` flag is a clean override. Setting `XDG_CONFIG_DIRS` replaces ALL XDG config lookups, not just labwc's. `-C` is surgical and only affects labwc config file search.

**Why `/etc/openauto-kiosk/labwc/` and not `~/.config/openauto-kiosk/`:** System-level config. The kiosk session is a system-wide installation artifact, not a per-user preference. Keeping it in `/etc/` also avoids the user accidentally modifying it while editing their normal labwc config.

### 2. Kiosk labwc Configuration

Three files in `/etc/openauto-kiosk/labwc/`:

**rc.xml** -- Minimal compositor config:
```xml
<?xml version="1.0"?>
<labwc_config>
  <touch deviceName="" mapToOutput="" mouseEmulation="no"/>
  <core>
    <decoration>server</decoration>
  </core>
  <windowRules>
    <windowRule identifier="openauto-prodigy" serverDecoration="no">
      <action name="ToggleFullscreen"/>
    </windowRule>
  </windowRules>
</labwc_config>
```

Key settings:
- `mouseEmulation="no"` -- critical for multi-touch (existing requirement)
- `serverDecoration="no"` for the app -- no title bar
- `ToggleFullscreen` rule -- app starts fullscreen automatically
- No margins, no keybinds beyond essentials

**autostart** -- Launch splash then app:
```bash
# Splash screen: covers display immediately (runs before app initialization)
swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &

# Launch the app (blocks this script until app exits)
# The app dismisses the splash after its first frame renders.
/home/matt/openauto-prodigy/build/src/openauto-prodigy
```

Critical detail: the app command does NOT have `&` at the end. This means labwc's autostart script blocks on the app. When the app exits, the script finishes, and labwc's session ends. This is the natural lifecycle: app exit = session exit = LightDM takes over.

**environment** -- Required env vars:
```
QT_QPA_PLATFORM=wayland
```

labwc automatically sets `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, and `DISPLAY`. Only the Qt platform hint needs explicit setting.

### 3. Splash Lifecycle (Compositor-Owned, App-Dismissed)

**Current state (temporary workaround in main.cpp):** The app spawns swaybg as a child process before engine.load() and kills it after frameSwapped + 300ms delay. Problems: systemd may kill child processes on ExecStartPre timeout; timing is fragile; app crash = black screen.

**New architecture:** swaybg is launched by labwc autostart, not by the app. The app discovers and kills it after rendering its first frame.

**App-side splash dismissal in main.cpp:**

```cpp
// After engine.load() succeeds and QML window exists:
auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
if (window) {
    QObject::connect(window, &QQuickWindow::frameSwapped, &app, [&app]() {
        // First frame has been presented to the display server.
        // Kill the splash with a brief grace period for the compositor
        // to actually composite our frame over the splash.
        QTimer::singleShot(200, &app, []() {
            QProcess::execute("pkill", {"-f", "swaybg.*splash"});
        });
        // Disconnect after first fire (one-shot)
        QObject::disconnect(window, &QQuickWindow::frameSwapped, &app, nullptr);
    });
}
```

**Why `pkill -f "swaybg.*splash"` and not `pkill swaybg`:** If the user has a normal desktop session on another VT with swaybg for wallpaper, we must not kill that. The splash swaybg is launched with the splash image path, so the `-f` pattern matches only the kiosk splash instance.

**Failure mode:** If the app crashes before dismissing splash, swaybg stays visible showing the branded Prodigy logo. This is intentionally better than a bare black screen or bare desktop. systemd will restart the app (Restart=on-failure), and the new instance will dismiss the existing splash.

**Edge case -- app restart:** If the service restarts (watchdog, crash), swaybg is already dead from the first run. The new app instance starts without splash. This is fine -- the restart is sub-second and the user already saw the app.

### 4. RPi Boot Splash (rpi-splash-screen-support)

The `rpi-splash-screen-support` package replaces the kernel's default four-raspberry-logo framebuffer image with a custom TGA.

**Installation and configuration:**
```bash
sudo apt install rpi-splash-screen-support imagemagick
# Convert PNG to required TGA format
convert splash.png -flip -colors 224 -depth 8 -type TrueColor \
    -alpha off -compress none -define tga:bits-per-sample=8 /tmp/splash.tga
sudo configure-splash /tmp/splash.tga
```

**Kernel cmdline requirements:** The existing `cmdline.txt` already has `quiet splash` which are the necessary flags. No changes needed.

**Image requirements:**
- Format: 24-bit TGA, uncompressed
- Max resolution: 1920x1080
- Max colors: 224 (ImageMagick `-colors 224` handles this)
- Image must be flipped vertically (TGA convention, `-flip` handles it)

**Visual continuity:** The boot splash PNG and the swaybg splash PNG should be the same image. The transition from kernel framebuffer to Wayland compositor is a seamless handoff -- same image, same position.

### 5. Exit-to-Desktop (Runtime Session Switch)

**Mechanism:** `dm-tool switch-to-user USERNAME rpd-labwc`

This tells LightDM to switch to the standard desktop session for the same user. LightDM will:
1. Start a new `rpd-labwc` session on a new VT
2. Switch the display to that VT
3. The kiosk session continues running in the background (or terminates naturally)

**App-side implementation:**

```cpp
// ApplicationController.cpp
void ApplicationController::exitToDesktop()
{
    qInfo() << "[App] Exit to desktop requested";

    // Tell LightDM to switch to the normal desktop session
    QProcess::startDetached("dm-tool", {"switch-to-user",
        qEnvironmentVariable("USER", "matt"), "rpd-labwc"});

    // Quit cleanly so the kiosk session terminates
    QTimer::singleShot(500, []() {
        QCoreApplication::quit();
    });
}
```

**Why 500ms delay before quit:** Give dm-tool time to communicate with LightDM. If the app quits immediately, labwc exits, and LightDM might start the default session (which could be the kiosk session again if autologin is configured). The delay ensures the session switch request is registered before the kiosk session ends.

**ActionRegistry integration:**
```cpp
actionRegistry->registerAction("app.exitToDesktop", [appController](const QVariant&) {
    appController->exitToDesktop();
});
```

**GestureOverlay.qml addition:**
```qml
ElevatedButton {
    objectName: "overlayExitBtn"
    text: "Desktop"
    iconCode: "\uef6e"  // desktop_windows
    buttonEnabled: overlay.acceptInput
    implicitWidth: UiMetrics.overlayBtnW
    implicitHeight: UiMetrics.overlayBtnH
    onClicked: {
        ActionRegistry.dispatch("app.exitToDesktop")
        overlay.dismiss()
    }
}
```

This adds a fourth button to the 3-finger overlay: Home, Day/Night, Desktop, Close.

**Returning to kiosk from desktop:** User can switch back via LightDM greeter or by running `dm-tool switch-to-user matt openauto-kiosk`. For a car head unit, this is an admin/debug escape hatch, not a frequent action. No UI shortcut is needed in the desktop session.

### 6. systemd Service Changes

**Current service has wf-panel-pi lifecycle management:**
```ini
ExecStartPre=-/bin/sh -c 'systemctl --user stop wf-panel-restore.service 2>/dev/null; pkill -f "lwrespawn.*wf-panel-pi"; pkill wf-panel-pi; true'
ExecStopPost=-/bin/sh -c '[ "$SERVICE_RESULT" = "success" ] && systemd-run --user --unit=wf-panel-restore --setenv=WAYLAND_DISPLAY=wayland-0 /usr/bin/lwrespawn /usr/bin/wf-panel-pi || true'
```

**In kiosk mode, this is completely unnecessary.** The kiosk session never starts wf-panel-pi in the first place. The service simplifies to:

```ini
[Unit]
Description=OpenAuto Prodigy
After=graphical.target hostapd.service bluetooth.target pipewire.service
Wants=openauto-system.service
BindsTo=hostapd.service
StartLimitBurst=5
StartLimitIntervalSec=60

[Service]
Type=notify
User=matt
Environment=XDG_RUNTIME_DIR=/run/user/1000
Environment=WAYLAND_DISPLAY=wayland-0
Environment=QT_QPA_PLATFORM=wayland
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
WorkingDirectory=/home/matt/openauto-prodigy
ExecStartPre=+/usr/local/bin/openauto-preflight
ExecStart=/home/matt/openauto-prodigy/build/src/openauto-prodigy
ExecStopPost=-/bin/sh -c '[ "$SERVICE_RESULT" = "success" ] && timeout 5 /usr/bin/bluetoothctl disconnect || true'
Restart=on-failure
RestartSec=3
WatchdogSec=30
NotifyAccess=main

[Install]
WantedBy=graphical.target
```

**However** -- there is an architectural decision here about WHO launches the app.

**Option A: systemd service (current, keep it)**
- App is managed by systemd: restart, watchdog, journald logging
- labwc autostart script only does splash
- autostart runs `systemctl --user start openauto-prodigy` (or the system service starts via WantedBy=graphical.target)

**Option B: labwc autostart launches app directly (recommended)**
- No systemd service needed for app lifecycle in kiosk mode
- labwc autostart: `swaybg ... &` then `exec openauto-prodigy`
- App exit = session exit = natural lifecycle
- Restart handled by adding `Restart=on-failure` to the session? No -- labwc has no restart logic for autostart commands.
- Lose: watchdog, structured restarts, journald integration

**Recommendation: Hybrid approach.** labwc autostart launches splash + starts the systemd service, then waits for session termination. Or more simply: keep the systemd service, but remove the wf-panel-pi lines. The kiosk autostart does `swaybg ... &` and nothing else. The systemd service starts via `WantedBy=graphical.target` as it does now. The app runs under systemd with all its restart/watchdog benefits.

The labwc autostart for the kiosk session becomes:
```bash
# Splash: immediate visual coverage
swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &
```

That's it. The app still launches via systemd, exactly as it does now. The only difference is that the kiosk labwc session provides a clean canvas (no panel, no desktop) for the systemd-managed app to render into.

### 7. LightDM Configuration

**Current:** Autologin configured by RPi OS default setup (usually via raspi-config).

**New:** LightDM drop-in config to set autologin session to kiosk:

```ini
# /etc/lightdm/lightdm.conf.d/50-openauto.conf
[Seat:*]
autologin-user=matt
autologin-session=openauto-kiosk
```

**Important:** This is a drop-in file in `lightdm.conf.d/`, not a modification to the main `lightdm.conf`. Drop-in files override specific keys without touching the base config. If the user later wants to revert, they just delete this file.

**Reverting to desktop session:**
```bash
sudo rm /etc/lightdm/lightdm.conf.d/50-openauto.conf
sudo systemctl restart lightdm
```

Or change the `autologin-session` value to `rpd-labwc`.

### 8. Installer Changes

**New installer step: "Configure kiosk session"**

This step runs after the existing service creation and before system optimization. It:

1. **Creates kiosk labwc config directory:**
   ```bash
   sudo mkdir -p /etc/openauto-kiosk/labwc
   ```

2. **Writes kiosk rc.xml, autostart, environment** (heredocs in installer)

3. **Creates XDG session file:**
   ```bash
   sudo tee /usr/share/wayland-sessions/openauto-kiosk.desktop > /dev/null << DESKTOP
   [Desktop Entry]
   Name=OpenAuto Kiosk
   Comment=OpenAuto Prodigy kiosk session
   Exec=/usr/bin/labwc -C /etc/openauto-kiosk/labwc
   Type=Application
   DesktopNames=labwc
   DESKTOP
   ```

4. **Installs splash images:**
   ```bash
   sudo mkdir -p /usr/share/openauto-prodigy
   sudo cp "$INSTALL_DIR/assets/splash.png" /usr/share/openauto-prodigy/
   # Convert to TGA for kernel boot splash
   convert "$INSTALL_DIR/assets/splash.png" -flip -colors 224 -depth 8 \
       -type TrueColor -alpha off -compress none \
       -define tga:bits-per-sample=8 /tmp/splash.tga
   sudo configure-splash /tmp/splash.tga
   ```

5. **Writes LightDM drop-in:**
   ```bash
   sudo mkdir -p /etc/lightdm/lightdm.conf.d
   sudo tee /etc/lightdm/lightdm.conf.d/50-openauto.conf > /dev/null << LIGHTDM
   [Seat:*]
   autologin-user=$USER
   autologin-session=openauto-kiosk
   LIGHTDM
   ```

6. **Simplifies systemd service** (removes wf-panel-pi lines from service template)

7. **Adds `rpi-splash-screen-support` and `imagemagick` to install deps**

**Installer must also handle:**
- **Upgrades:** If the kiosk session files already exist, overwrite them (idempotent)
- **Revert option:** Installer final summary mentions `sudo rm /etc/lightdm/lightdm.conf.d/50-openauto.conf` to revert to desktop
- **Non-kiosk installs:** The installer's AUTOSTART prompt should become a kiosk mode prompt. If user declines, skip session creation and keep the existing systemd-only approach.

## Data Flow

### Boot Sequence (power-on to app)

```
Power on
  -> Kernel loads
  -> rpi-splash-screen-support draws splash.tga to framebuffer
  -> LightDM starts
  -> LightDM reads 50-openauto.conf: autologin-session=openauto-kiosk
  -> LightDM autologins user, starts openauto-kiosk session
  -> labwc starts with -C /etc/openauto-kiosk/labwc
  -> labwc runs autostart: swaybg -i splash.png -m fill &
  -> swaybg draws splash.png over framebuffer (seamless transition)
  -> graphical.target reached
  -> systemd starts openauto-prodigy.service
  -> ExecStartPre: openauto-preflight (rfkill, Wayland, SDP)
  -> ExecStart: openauto-prodigy launches
  -> Qt/QML engine initializes, loads QML
  -> First frame rendered -> frameSwapped signal
  -> QTimer::singleShot(200ms) -> pkill swaybg.*splash
  -> sd_notify(READY=1) (already in current code)
  -> App running, splash gone
```

### Exit-to-Desktop Sequence

```
User triggers 3-finger gesture overlay
  -> Taps "Desktop" button
  -> ActionRegistry.dispatch("app.exitToDesktop")
  -> ApplicationController.exitToDesktop()
  -> QProcess::startDetached("dm-tool", ["switch-to-user", "matt", "rpd-labwc"])
  -> LightDM starts rpd-labwc session on new VT
  -> LightDM switches display to new VT
  -> QTimer::singleShot(500ms) -> QCoreApplication::quit()
  -> systemd sees clean exit (exit code 0), does NOT restart
  -> labwc kiosk session stays alive briefly, then exits naturally
  -> User is now on standard RPi desktop
```

### Return-to-Kiosk Sequence

```
User opens terminal on desktop
  -> Runs: dm-tool switch-to-user matt openauto-kiosk
  -> OR: reboots (LightDM autologin kicks in)
  -> Kiosk session starts fresh (same boot sequence minus kernel splash)
```

## Patterns to Follow

### Pattern 1: Drop-in Configuration (not base file modification)
**What:** Use `/etc/lightdm/lightdm.conf.d/50-openauto.conf` instead of modifying `/etc/lightdm/lightdm.conf`. Use `/etc/openauto-kiosk/labwc/` instead of modifying `~/.config/labwc/`.
**When:** Any system config that needs to be installed/uninstalled cleanly.
**Why:** Drop-in files can be removed without restoring backups. The base config remains untouched. Upgrades don't conflict. Reverting is `rm` not `sed -i`.

### Pattern 2: Compositor-Owned Splash with App-Owned Dismissal
**What:** The compositor (labwc) owns the splash process lifecycle. The app merely kills it when ready.
**When:** Any layered boot handoff where the splash must survive app crashes.
**Why:** If the app owns the splash (current temporary state), a crash kills the splash and exposes the bare compositor. If the compositor owns it, a crash leaves the branded splash visible -- better UX and better failure mode.

### Pattern 3: Session Switch via dm-tool (not logout/login)
**What:** Use `dm-tool switch-to-user` to switch between kiosk and desktop sessions.
**When:** Exit-to-desktop functionality.
**Why:** No authentication prompt required for the same user. No logout -- the switch is seamless. LightDM handles VT management, session lifecycle, and display ownership. Avoids reinventing session management.

### Pattern 4: Hybrid Service Management
**What:** labwc autostart handles splash only; systemd handles the app.
**When:** The app needs restart, watchdog, and structured logging.
**Why:** labwc autostart has no process management features (no restart-on-failure, no watchdog, no journald). systemd provides all of these. The kiosk session provides the canvas; systemd provides the lifecycle.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Modifying User's labwc Config for Kiosk
**What:** Changing `~/.config/labwc/autostart` or `~/.config/labwc/rc.xml` to remove wf-panel-pi for kiosk mode.
**Why bad:** The user's desktop config is destroyed. Switching to desktop session gets a broken desktop. Any labwc update that regenerates defaults will conflict.
**Instead:** Separate config directory via labwc `-C` flag. User config and kiosk config are completely independent.

### Anti-Pattern 2: App Launching swaybg as Child Process
**What:** Current temporary approach -- app spawns swaybg before engine.load().
**Why bad:** systemd may kill child processes on ExecStartPre timeout. App crash kills splash (exposes bare compositor). Timing is fragile.
**Instead:** labwc autostart owns swaybg. App just kills it when ready.

### Anti-Pattern 3: logout + login for Session Switch
**What:** Calling `loginctl terminate-session` and hoping LightDM autologins with a different session.
**Why bad:** Destroys the current session abruptly. No guarantee LightDM will switch to the desired session type. Race condition between session termination and autologin.
**Instead:** `dm-tool switch-to-user` does an atomic session switch. Clean, documented, reliable.

### Anti-Pattern 4: Removing the systemd Service Entirely
**What:** Having labwc autostart `exec` the app directly, relying on labwc session for restart.
**Why bad:** labwc autostart has zero process management. No restart on crash. No watchdog. No journald structured logging. No sd_notify integration.
**Instead:** Keep systemd service. It starts via WantedBy=graphical.target just as it does now. labwc autostart only does splash.

### Anti-Pattern 5: Embedding Splash Image in the App Binary
**What:** Using Qt resources (qrc) for the splash image displayed via QSplashScreen.
**Why bad:** QSplashScreen requires the Qt event loop and QML engine to be running. The whole point of the splash is to cover the 2-3 seconds BEFORE the engine is ready. Also doesn't help with compositor-level coverage.
**Instead:** swaybg runs before the app even starts. Zero dependency on Qt initialization.

## Suggested Build Order

Build order follows the dependency chain from infrastructure to UI.

### Phase 1: Kiosk Session Infrastructure
**What:** Create XDG session file, kiosk labwc config directory with rc.xml + autostart + environment, and LightDM drop-in. Test manually on Pi.
**Scope:**
- Create the 3 labwc config files (rc.xml, autostart, environment)
- Create the XDG session .desktop file
- Create the LightDM drop-in
- Simplify the systemd service (remove wf-panel-pi lines)
- Test: switch to kiosk session via `dm-tool switch-to-user matt openauto-kiosk`, verify labwc starts with correct config, verify app launches via systemd, verify no panel/taskbar visible
**Dependencies:** None (infrastructure only)
**Testable:** Kiosk session works end-to-end, app runs fullscreen without desktop chrome.

### Phase 2: Compositor Splash
**What:** Move splash from app child process to labwc autostart. Implement frameSwapped-based dismissal in main.cpp.
**Scope:**
- Add `swaybg -i splash.png -m fill &` to kiosk autostart
- Add frameSwapped handler in main.cpp that does `pkill -f "swaybg.*splash"` after 200ms
- Remove any existing app-owned swaybg launch code
- Create/place splash PNG at `/usr/share/openauto-prodigy/splash.png`
**Dependencies:** Phase 1 (kiosk session must exist)
**Testable:** Reboot Pi, see splash image from labwc start until app first frame, then splash disappears. Kill app, splash stays gone (swaybg already dead). Restart app via systemd, no splash on restart (correct -- splash is only for cold boot).

### Phase 3: RPi Boot Splash
**What:** Install rpi-splash-screen-support, convert splash image to TGA, configure kernel boot splash.
**Scope:**
- Add `rpi-splash-screen-support` and `imagemagick` to installer deps
- Convert splash.png to splash.tga with correct parameters
- Run `sudo configure-splash splash.tga`
- Verify `cmdline.txt` has `quiet splash` (it already does)
**Dependencies:** Splash image from Phase 2 (same artwork)
**Testable:** Cold reboot shows Prodigy logo from kernel start, transitions seamlessly to swaybg splash (same image), then to app.

### Phase 4: Exit-to-Desktop
**What:** ApplicationController gains exitToDesktop(). ActionRegistry registers the action. GestureOverlay gets "Desktop" button.
**Scope:**
- ApplicationController: add `exitToDesktop()` method with dm-tool + delayed quit
- ActionRegistry: register `app.exitToDesktop`
- GestureOverlay.qml: add "Desktop" ElevatedButton
- Test: 3-finger gesture -> tap Desktop -> lands on RPi desktop. Run `dm-tool switch-to-user matt openauto-kiosk` to return.
**Dependencies:** Phase 1 (both sessions must exist)
**Testable:** Full round-trip: kiosk -> desktop -> kiosk.

### Phase 5: Installer Integration
**What:** install.sh creates all kiosk infrastructure as part of normal install flow.
**Scope:**
- New installer step after service creation
- Prompt: "Enable kiosk mode? [Y/n]" (replaces AUTOSTART boolean)
- If yes: create session files, splash, LightDM config, modify service
- If no: keep current desktop-mode service (with wf-panel-pi management)
- Handle upgrades: overwrite existing kiosk config (idempotent)
- Final summary: mention revert instructions
- Add splash assets to repo (assets/splash.png)
- install-prebuilt.sh: same kiosk setup logic
**Dependencies:** All previous phases (installer must create everything)
**Testable:** Fresh install with kiosk enabled -> boots straight to app with splash. Fresh install without kiosk -> boots to desktop, manual `systemctl start`.

## Scalability Considerations

| Concern | Impact | Notes |
|---------|--------|-------|
| Boot time | Not affected by kiosk session -- same labwc, fewer autostart processes (no panel) | Marginally faster than desktop session |
| Memory | Saves ~30MB by not loading wf-panel-pi and pcmanfm | Slight improvement |
| Multiple users | dm-tool handles multi-user switching natively | Not relevant for single-user Pi head unit |
| Session crash recovery | systemd Restart=on-failure still works | App restarts within same kiosk session |
| Display resolution changes | Splash uses swaybg `fill` mode -- works at any resolution | TGA boot splash limited to 1920x1080 max |

## Sources

- [labwc configuration manual](https://labwc.github.io/labwc-config.5.html) -- `-C` flag, rc.xml format, autostart, environment files, window rules, XDG search order (HIGH confidence)
- [labwc kiosk discussion](https://github.com/labwc/labwc/discussions/2301) -- autostart kiosk patterns, window rules for fullscreen (HIGH confidence)
- [dm-tool man page](https://manpages.debian.org/testing/lightdm/dm-tool.1.en.html) -- switch-to-user, switch-to-greeter commands (HIGH confidence)
- [LightDM ArchWiki](https://wiki.archlinux.org/title/LightDM) -- autologin-session configuration, wayland-sessions directory (HIGH confidence)
- [rpi-splash-screen-support GitHub](https://github.com/raspberrypi/rpi-splash-screen-support) -- TGA format, configure-splash usage, image requirements (HIGH confidence)
- [swaybg GitHub](https://github.com/swaywm/swaybg) -- image modes (fill/stretch/fit/center/tile), wlr-layer-shell protocol (HIGH confidence)
- [RPi Forums: Autostart with labwc](https://forums.raspberrypi.com/viewtopic.php?t=379321) -- default /etc/xdg/labwc/autostart contents, lwrespawn, wf-panel-pi (MEDIUM confidence)
- Existing codebase analysis:
  - `install.sh` -- current service creation, labwc config, panel suppression
  - `src/main.cpp` -- sd_notify integration, QML engine lifecycle, signal handling
  - `src/ui/ApplicationController.cpp` -- quit(), restart(), minimize()
  - `qml/components/GestureOverlay.qml` -- 3-finger overlay button layout
  - `docs/pi-config/` -- current Pi configuration reference files
  - `.planning/notes/kiosk-session-splash.md` -- architecture notes and Codex recommendation
