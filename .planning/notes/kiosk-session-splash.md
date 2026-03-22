# Kiosk Session & Splash Screen

**Status:** Planned for future milestone
**Context:** Codex architecture recommendation from 2026-03-22

## Problem

User sees bare desktop (labwc panel/taskbar) for ~3 seconds between compositor ready and app first frame. Makes the product feel unfinished.

## Solution: Dedicated Kiosk Session

Replace the default RPi OS desktop session with a purpose-built kiosk session.

### Architecture

1. **LightDM** autologins into custom `openauto-kiosk` XDG session (not rpd-labwc)
2. **labwc** starts with stripped config — no panel, no taskbar, no wallpaper daemon, solid black background
3. **Splash client** (swaybg) launches from labwc autostart as first Wayland surface — covers screen immediately
4. **openauto-prodigy** launches right after splash — initializes behind it
5. **App dismisses splash** after first frame is presented (frameSwapped signal, not timer)
6. **Failure mode:** If app crashes, splash stays visible (branded) instead of bare desktop

### What Needs to Change

- Create `/usr/share/wayland-sessions/openauto-kiosk.desktop` (XDG session file)
- Create kiosk-specific labwc config (`rc.xml` + `autostart` — no wf-panel-pi, no lwrespawn)
- Update LightDM autologin to use the kiosk session
- Move splash launch from app process to labwc autostart (proper lifecycle owner)
- App keeps the frameSwapped + terminate approach for splash dismissal
- Installer creates all of the above
- Installer should offer to revert to normal desktop session if needed

### Current State (temporary)

The app currently launches swaybg as a child process before engine.load() and terminates it after frameSwapped + 300ms. This is a workaround — systemd kills ExecStartPre children, and the app-owned process has timing issues. The kiosk session approach solves this properly by having labwc own the splash lifecycle.

### References

- Codex recommendation: dedicated kiosk session, compositor-owned splash, app-owned handoff
- Common pattern in automotive/embedded: layered handoff (boot splash → compositor splash → app)
- swaybg works as the splash client (already installed, simple, reliable)
