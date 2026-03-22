# Phase 28: Kiosk Session Infrastructure - Context

**Gathered:** 2026-03-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Create the foundational kiosk session infrastructure: XDG session entry, stripped labwc config directory, LightDM autologin drop-in, and simplified systemd service. Pure config files — no app code changes (splash dismissal, exit-to-desktop button, and installer integration are later phases).

</domain>

<decisions>
## Implementation Decisions

### Exit-to-Desktop Access
- Exit-to-desktop button goes in the **NavbarPopup** (clock long-press popup), NOT the 3-finger overlay
- Labeled as "Exit" or "Desktop" — uses the existing NavbarPopup infrastructure
- This is a deliberate, intentional action path — requires long-pressing the clock and then tapping the button

### Desktop Mode Role
- Desktop mode is an **escape hatch only** — for SSH-free maintenance, package updates, debugging
- Return-to-kiosk is a reboot or terminal command (`dm-tool switch-to-user matt openauto-kiosk`)
- No polish needed on the return path — no desktop shortcut or UI for switching back

### Crash Recovery
- On crash-loop (systemd rate limit hit): **stay on splash, require SSH**
- swaybg splash stays visible (branded logo, not a black screen)
- This is the simplest approach; crash-loops are rare in practice
- No auto-fallback to desktop, no error surface

### Kiosk Mode Opt-In Model
- **Default on, opt-out** — installer prompts "Enable kiosk mode? [Y/n]"
- No existing installs to migrate — this is greenfield
- No upgrade migration logic needed

### Config Drift Handling
- **Document only** — README/installer output warns against using raspi-config for boot settings
- The drop-in file strategy (`lightdm.conf.d/`) already protects against most overwrites
- No preflight detection, no auto-repair

### Service File
- **One service file for both modes** — no panel management lines in either
- Drop the `wf-panel-pi` kill/restore `ExecStartPre`/`ExecStopPost` lines entirely
- Kiosk mode provides the clean experience; desktop mode users accept the panel stays visible
- The panel kill/restore hack is no longer justified now that kiosk mode exists
- Only the session config differs between modes (LightDM drop-in, XDG session, labwc config) — the service file is identical

### Boot Gap Acceptance
- The KMS black gap (~1-3s between kernel splash and compositor splash) is **accepted for v0.7.0**
- Black background on both sides minimizes visual disruption
- Boot speed optimization is a future milestone

### Claude's Discretion
- Exact labwc rc.xml window rule pattern (wildcard vs exact app_id match)
- Whether to set `app.setDesktopFileName()` in main.cpp for the window rule, or use a wildcard
- LightDM drop-in file naming convention (50-openauto-kiosk.conf vs 50-openauto.conf)
- labwc environment file contents beyond QT_QPA_PLATFORM

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Architecture & Patterns
- `.planning/research/ARCHITECTURE.md` — Full kiosk architecture: XDG session, labwc -C flag, drop-in configs, splash lifecycle, service management patterns, anti-patterns
- `.planning/research/PITFALLS.md` — 16 documented pitfalls with prevention strategies: session file syntax, config conflicts, double-launch, panel lifecycle, app_id mismatch

### Existing Config References
- `docs/pi-config/labwc-rc.xml` — Current desktop labwc config (mouseEmulation setting)
- `docs/pi-config/openauto-prodigy.desktop` — Current desktop shortcut file (NOT the session file)
- `docs/pi-config/cmdline.txt` — Current kernel command line parameters

### Splash Asset
- `assets/splash.png` — Prodigy logo (white star on black), used for both compositor splash and TGA boot splash

### Install Script
- `install.sh` — Current service template (lines ~1613-1640), panel management (lines ~1319-1325, ~1631-1634), preflight script

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `install.sh` service template heredoc — base for the simplified service file (remove panel lines)
- `docs/pi-config/` directory — reference configs that document the current Pi setup
- `assets/splash.png` — existing logo asset, ready for TGA conversion
- `openauto-preflight` script in install.sh — stays unchanged, still needed for rfkill/Wayland/SDP checks

### Established Patterns
- Installer uses heredocs for all config file generation (service, hostapd, dnsmasq, etc.)
- Config files are written to system locations with `sudo tee`
- Installer has a step-based TUI with status reporting
- Drop-in pattern already used for hostapd and BlueZ service overrides

### Integration Points
- `install.sh` step flow — new kiosk session step slots after service creation
- LightDM autologin — currently configured by RPi OS default, will be overridden by drop-in
- labwc config — new directory `/etc/openauto-kiosk/labwc/` created by installer, referenced by XDG session `Exec` line
- systemd service template — simplified, same file for both modes
- NavbarPopup — exit button will be added in Phase 31 (not this phase)

</code_context>

<specifics>
## Specific Ideas

- The splash image is already done — white star logo on black at `assets/splash.png`
- Kiosk labwc autostart should contain ONLY `swaybg` — systemd is the sole app launcher (Phase 29 adds the swaybg line)
- For this phase, the autostart file can be empty or contain only a comment — splash is Phase 29's concern
- The service file simplification (dropping panel lines) should happen in this phase since it's part of the infrastructure

</specifics>

<deferred>
## Deferred Ideas

- Boot speed optimization — separate milestone, not v0.7.0 scope
- Auto-fallback to desktop after crash-loops — decided against for now, revisit if users report issues
- Preflight health check for LightDM session config — document-only for now
- Return-to-kiosk desktop shortcut — not needed for escape-hatch-only desktop mode

</deferred>

---

*Phase: 28-kiosk-session-infrastructure*
*Context gathered: 2026-03-22*
