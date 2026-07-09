# Web Surface Strategy — native/web boundary, engine hygiene, streaming enablement

**Date:** 2026-07-07
**Status:** Approved (brainstorm with Matthew, this session)
**Scope:** Three linked decisions following the Phase C2 web-widget ship: (1) whether existing
plugins migrate to the web runtime, (2) browser-engine strategy on the head unit, (3) the path
to DRM streaming (Spotify / YouTube / parked video). Slice 1 is specified here to
implementation level; the WebAppHost arc (Slice 2) is scoped but deliberately deferred to its
own design cycle.

## Background / measured facts

Live measurements from the Pi (2026-07-07, app + one web widget running, AA idle):

- 8GB Pi 4 at **593MB used of 7.8GB**. No memory pressure exists today.
- Web-widget cost: `libQt6WebEngineCore` 171MB on disk; ~100MB RSS per live renderer;
  2–3 zygote helpers (~60MB nominal, heavily page-shared); app RSS 366MB including the
  in-process network service.
- **Two Chromiums are installed.** The app uses QtWebEngine (`libqt6webenginecore6` 6.8.2).
  Desktop Chromium 146 (`chromium`, `chromium-common`, `chromium-l10n`, `chromium-sandbox`,
  `rpi-chromium-mods`, ~250–300MB) ships with stock RPi OS and is used by nothing in prodigy.
- **Widevine is present and loadable.** `libwidevinecdm0` 4.10.2662.3 (arm64) is installed with
  the CDM at `/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so`. Debian's
  Qt6WebEngineCore binary contains the CDM-loading machinery: the `widevine-path` switch, the
  `com.widevine.alpha` key system, and gmp-style CDM search paths. Widevine on a Pi is
  L3 (software) only — video services cap at ~SD/720p, which is moot on a 1024×600 panel.

## Decision 1 — No existing plugin migrates to the web runtime

Audited: android_auto, phone, bt_audio, equalizer, settings, home, and the upcoming media
player. **All stay native QML.** Rationale:

1. **WebEngine is an optional dependency** (`HAS_WEBENGINE`; the app builds and runs without
   qt6-webengine). Core functions must never require a 171MB browser stack to operate.
2. **Crash/latency tolerance.** Renderers die (WebWidgetHost has 3-retry crash recovery for a
   reason). A weather tile reloading in 2s is fine; an incoming-call overlay is not. Anything
   used while driving needs native guarantees.
3. **No payoff.** Native versions exist and work with direct C++ bindings. A rewrite adds a
   ~100MB-per-renderer tax and a WebSocket hop for data, and buys no user-visible value.

**Decision rule (codify in `docs/design-philosophy.md`):**
> Native QML for anything core, driving-relevant, or latency-sensitive. The web runtime is the
> extension surface: glanceable dashboard content, optional add-ons, and third-party/community
> work that shouldn't require touching C++.

Corollaries: the media player arc (roadmap Now) is native (Qt Multimedia backend feeding
MediaStatusService, native QML UI). The EQ UI stays native.

## Decision 2 — Keep QtWebEngine; leave desktop Chromium alone

No engine swap, no fork, no purge:

- QtWebEngine already is the "de-Googled trimmed Chromium": Qt's embed strips sync, sign-in,
  Safe Browsing, Google API keys, and variations/field-trial telemetry; our hosts add
  locked-down settings on top. The Google-service bloat Matthew objects to lives in the
  desktop Chromium — which prodigy never launches, so it costs disk, not runtime.
- Desktop Chromium stays installed (Matthew's call, 2026-07-07): it's the user's system, and
  they may want a real browser when not using prodigy. The installer must not remove it.
- A self-maintained Chromium fork is not lighter at runtime and forfeits Debian security
  updates — a liability on a car computer.
- WPE WebKit (the one genuinely lighter engine with a real DRM story) has no Qt 6 integration
  in Debian, routes DRM through Thunder/OpenCDM (set-top-box-scale integration), and would
  orphan the entire Phase C2 runtime built on QtWebEngine APIs. Rejected.

## Decision 3 — Streaming becomes a fullscreen "Web Apps" surface (own arc)

DRM streaming cannot ride the widget runtime — WebWidgetHost deliberately enforces the
opposite policy (same-origin `prodigy://widgets/` navigation only, no persistent login
storage, fullscreen denied). Streaming needs a sibling host with inverted policy:

- **WebAppHost**: manifest-driven app packages under `~/.openauto/webapps/` (name, URL, icon,
  UA override), delivered like webwidgets; each app surfaces as a launcher tile.
- **Persistent per-app WebEngine profile** (cookies/localStorage survive reboots — log into
  Spotify once).
- **Widevine** via the CDM wiring from Slice 1.
- **TV user-agents where they help**: YouTube leanback is touch-friendly at 1024×600 and uses
  code-pairing login, sidestepping the on-screen-keyboard problem for the hardest case.

Questions the WebAppHost design arc must answer (scope boundary, not requirements here):
keyboard story for non-code logins (Qt VirtualKeyboard vs. pair-from-phone via web-config);
audio-focus policy (background playback under the AA view implies keeping a renderer alive);
resource cap (one app alive at a time?); whether librespot (Spotify Connect target — lighter,
DRM-free, Premium-only, no on-HU browse) complements the web player. Entry goes to
`docs/wishlist.md` pending promotion; the media player remains the committed Now arc.

## Slice 1 — Widevine enablement (implement now)

Day-scale, de-risks Decision 3 before anything is designed around it.

### 1. Installer: CDM guarantee (`install.sh`)

In `install_dependencies()`:

- **Ensure the CDM:** add `libwidevinecdm0` to the install set (available from the RPi OS
  archive; already present on the current unit, where the chromium stack pulled it in). On
  Trixie Lite bases without chromium this is what provides the CDM. If the package is
  unavailable (non-RPi repos), warn and continue — Widevine is an enhancement, not a
  dependency.
- Desktop Chromium, where present, is left untouched (Decision 2).

### 2. App: Widevine CDM wiring (`src/main.cpp`)

Before `QtWebEngineQuick::initialize()` (main.cpp:176):

- Probe known CDM locations, first match wins:
  1. `/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so` (RPi layout, confirmed)
  2. `/opt/WidevineCdm/_platform_specific/linux_arm64/libwidevinecdm.so` (chromium component
     layout; verify existence on device during implementation)
- If found, append `--widevine-path=<path>` to `QTWEBENGINE_CHROMIUM_FLAGS` via `qputenv` —
  **unless** the user's environment already sets a `widevine-path` (respect overrides).
- Log one info line either way (path used, or "no Widevine CDM found — DRM content
  unavailable"). No config key; auto-detection with env override is sufficient (YAGNI).
- `#ifdef HAS_WEBENGINE` guarded like the rest of the WebEngine init.

### 3. Verification protocol (the spike)

- **EME probe:** a small QML file (WebEngineView, loads the Shaka Player demo with
  Widevine test content, or bitmovin's DRM demo) run on the Pi via the qml runtime.
  Committed under `tools/` — it stays useful for the WebAppHost arc. Success =
  DRM-protected video+audio plays.
- **Codec check (same probe):** confirm H.264/AAC playback (Debian builds QtWebEngine against
  system FFmpeg, expected to work — verify, don't assume).
- **Real-service check (needs Matthew):** Spotify Web Player login + playback in the probe.
  ~10 min bench time; also exercises persistent-profile assumptions for the future arc.
- **Regression:** existing dashboard web widget still loads post-change (flag append must not
  disturb the widget runtime); full test suite still passes.

### 4. Documentation deliverables

- Decision rule added to `docs/design-philosophy.md` (Decision 1 wording above).
- WebAppHost arc captured in `docs/wishlist.md` with a pointer to this spec.
- Roadmap: Slice 1 recorded in Done on completion; wishlist entry noted under Later
  alongside the existing streaming-adjacent items.

## Explicitly out of scope

- Any migration of existing plugins to web widgets (Decision 1).
- Engine replacement or forking (Decision 2).
- WebAppHost implementation, manifest format, login/keyboard UX, audio-focus policy
  (Decision 3 arc — future design doc).
- Driving-lockout policy for parked video (no vehicle-speed source exists; user
  responsibility for now — revisit if a speed signal ever lands).
