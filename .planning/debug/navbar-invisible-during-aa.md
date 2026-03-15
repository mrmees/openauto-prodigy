---
status: resolved
trigger: "Navbar is not visible during Android Auto sessions, even though it is functionally present (touch zones work, popups appear)"
created: 2026-03-03T00:00:00Z
updated: 2026-03-03T00:00:00Z
---

## Current Focus

hypothesis: AndroidAutoPlugin.wantsFullscreen() returns true, which causes Navbar.qml to set visible=false
test: Read Navbar.qml visibility binding and AndroidAutoPlugin.hpp wantsFullscreen override
expecting: Confirmed — navbar hidden because fullscreen flag is set by the AA plugin
next_action: COMPLETE — root cause identified, research-only mode

## Symptoms

expected: Navbar renders visually during AA sessions when show_during_aa config is true
actual: Navbar responds to evdev touches and popups appear, but navbar is not rendered
errors: None — silent visual omission
reproduction: Start an AA session with navbar.show_during_aa = true
started: Unknown — likely from initial navbar implementation

## Eliminated

- hypothesis: z-order — AA video or content area is painted on top of navbar
  evidence: Navbar has z:100 in Shell.qml (line 82), ColumnLayout content area has no explicit z so defaults to 0. VideoOutput is inside pluginContentHost which is inside the ColumnLayout — no z contest possible.
  timestamp: 2026-03-03

- hypothesis: AndroidAutoMenu.qml fills the full window and covers the navbar
  evidence: AndroidAutoMenu.qml is loaded inside pluginContentHost (Shell.qml line 28-62). That Item is inside a ColumnLayout that already has margins applied for the navbar edge. The video only fills its parent (pluginContentHost), not the window.
  timestamp: 2026-03-03

- hypothesis: NavbarController hides or opacity-zeroes the navbar during AA
  evidence: NavbarController has no visibility logic. It manages gesture state, popup state, and zone registration — no AA-awareness, no show/hide signals.
  timestamp: 2026-03-03

## Evidence

- timestamp: 2026-03-03
  checked: Navbar.qml line 14
  found: visible: !PluginModel.activePluginFullscreen
  implication: Navbar visibility is directly bound to the inverse of the fullscreen flag

- timestamp: 2026-03-03
  checked: AndroidAutoPlugin.hpp line 68
  found: bool wantsFullscreen() const override { return true; }
  implication: AA plugin unconditionally declares it wants fullscreen

- timestamp: 2026-03-03
  checked: PluginModel.cpp lines 75-79
  found: bool PluginModel::activePluginFullscreen() const { auto* p = activePlugin(); return p ? p->wantsFullscreen() : false; }
  implication: PluginModel.activePluginFullscreen reads directly from wantsFullscreen() with no config override

- timestamp: 2026-03-03
  checked: Shell.qml lines 10-25
  found: fullscreenMode: PluginModel.activePluginFullscreen — hides TopBar, zeroes NavStrip height
  implication: The fullscreen flag also hides the TopBar and NavStrip — the intended config path (show_during_aa) is never consulted by the QML rendering path

- timestamp: 2026-03-03
  checked: AndroidAutoPlugin.cpp lines 127-137
  found: show_during_aa config IS read, but only to configure evdev touch zones (touchReader_->setNavbar)
  implication: The config affects touch zones only — it never reaches the QML visibility binding, which is hardwired to wantsFullscreen()

## Resolution

root_cause: |
  AndroidAutoPlugin::wantsFullscreen() unconditionally returns true (AndroidAutoPlugin.hpp line 68).
  Navbar.qml binds its visibility directly to !PluginModel.activePluginFullscreen (Navbar.qml line 14).
  Shell.qml's fullscreenMode (which hides TopBar and zeroes NavStrip) is also driven by the same flag.

  The show_during_aa config option is read in AndroidAutoPlugin::initialize() (cpp lines 127-137), but
  it only controls evdev touch zone registration. It has NO effect on the QML rendering path. The
  navbar.visible binding in Navbar.qml never consults this config — it only sees the fullscreen flag,
  which is always true for AA.

  Result: navbar is invisible (QML rendering suppressed) but its evdev touch zones ARE registered,
  so touch hits work and popups fire — the exact symptoms reported.

fix: Not applied (research-only mode). Two-part fix required:
  1. AndroidAutoPlugin.hpp — wantsFullscreen() should return false (or become config-driven)
  2. Navbar.qml — visibility logic needs to account for the show_during_aa config, OR
     Shell.qml should not apply fullscreenMode to the navbar when show_during_aa is true

verification: N/A — research only
files_changed: []
