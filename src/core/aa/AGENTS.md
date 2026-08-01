# src/core/aa/ — AA Protocol Rules

Hard-won protocol behavior for the Android Auto runtime. Root `AGENTS.md` holds hard constraints; `src/AGENTS.md` holds general Qt/PipeWire traps.

## Touch events (Android MotionEvent semantics)

- **Touch coordinates must be in the advertised `touch_screen_config` space.**
  That space is the configured AA video mode minus the declared video margins;
  `EvdevTouchReader` maps the detected display viewport into those shared content
  dimensions. Do not substitute physical UI/display dimensions or the full
  encoded-frame dimensions.
- **UI/display dimensions and AA protocol dimensions are separate.** The shell
  uses the detected window size (with a 1024x600 fallback) for layout and evdev
  mapping; it is not a fixed AA protocol invariant.
- **ALL active pointers must be included in every message**, not just the changed finger.
- **`action_index` is the ARRAY INDEX** into the pointers list; **`pointer_id`** is the stable per-finger identifier (we use slot index).
- **UP events include the lifted finger** in the pointer array at its last position.

## Video

- **SPS/PPS arrives as `AV_MEDIA_INDICATION` (no timestamp)** — must be forwarded to the decoder.
- Encoded AA video already carries Annex B start codes — do NOT prepend more.
- **Phone sends `config_index=3` in `AVChannelSetupRequest`** even though our config list has indices 0–1 — it's the phone's internal reference, not an index into our list.
- **FFmpeg `thread_count` must be 1** for real-time decode — multi-threaded decoders buffer frames internally → permanent EAGAIN on phones sending small P-frames.
- **Accept both `AV_PIX_FMT_YUV420P` and `AV_PIX_FMT_YUVJ420P`** — some phones (Moto G Play 2024) output JPEG full-range; rejecting it silently discards frames (black screen).
- **`VideoConfig.margin_width/height` works** — phone renders a centered sub-region with black-bar margins; locked at session start. See `docs/aa-protocol/`.
- **The shipped HU advertises one configured landscape video mode:**
  `VIDEO_800x480`, `VIDEO_1280x720`, or `VIDEO_1920x1080`. The protocol enum
  also defines higher-resolution and portrait modes, but current service
  discovery does not advertise them. At GAL 1.7/4.3, MAIN gets one
  configuration per enabled recognized codec and the legacy CLUSTER policy
  remains unchanged; GAL 5.0+ advertise one codec per enabled display.
- **The shipped decode path supports H.264/AVC and H.265/HEVC.** Service
  discovery's accepted default order is H.265 then H.264, and the decoder
  detects which of the two the phone sends. H.264 remains an explicit fallback.
  Keep advertised codec choices aligned with decoder support.
- **Accepted H.265 is hardware decode, not software fallback.** Pi acceptance
  used FFmpeg `hevc` with a DRM hardware context, `/dev/video19`, V4L2
  stateless request decode, and DMABuf/DRM_PRIME frames.
- **Projected dashboard presentation is local, not service discovery.** When
  the secondary display is enabled, it remains `AUXILIARY` display 1 on
  channels 12/13 and its descriptor always uses `KEYCODE_NAVIGATION`.
  `video.secondary_display_content` is an immediate local setting: `map`
  (the default/fallback) shows the live decoded map, while `turn_card` renders
  the native semantic card from `NavigationProvider` without reconnecting AA
  or changing the descriptor. The decoder remains live in either mode. The
  native card consumes exact navigation-state, complete notification, and
  complete position snapshots. Each successfully parsed modern message
  replaces its stream; omitted optional fields clear instead of inheriting the
  previous value. `REROUTING` hides stale guidance behind `Finding a new route`
  until a fresh notification arrives. The card uses the first ordered action
  cue distinct from the upcoming-road label, may append coarse next-step time
  when it fits, and shows index-zero destination distance, phone-formatted ETA,
  single-destination remaining duration, and an overflow-only address marquee
  when available. Live lane guidance replaces that complete footer and remains
  one continuous roadway band, not a row of button-like cells. The deprecated
  flat turn event remains a compatibility fallback. Do not infer multi-stop
  numeric duration, roundabout detail, current road, or lookahead without new
  recorded delivery evidence.
- **Navigation provider notifications have one owner.**
  `NavigationDataBridge` emits the inherited `INavigationProvider` signals; do
  not redeclare shadow signals in the derived class. This keeps External API v1
  navigation pushes live without changing their payload. Exact rerouting
  intentionally clears stale route fields until fresh guidance arrives.

## Production GAL session policy

- **`connection.gal_version` is durable and session-wide, independent of the
  CLUSTER lab.** Accepted selections are exactly 1.7, 4.3, 5.0, 5.1, and 6.0;
  missing or invalid values resolve to the highest accepted value, currently
  6.0. A real change gracefully reconnects active AA.
- **Requested GAL is the sole local policy authority.** For modern requests, a
  `MATCH` response at or above the requested tuple is compatible but never
  raises local obligations; a lower tuple fails before TLS. Legacy 1.7 retains
  its established status-only admission rule.
- **Thresholds are additive:** 4.3 enables modern display metadata; 5.0 adds
  extended audio-start tolerance, one shared first-recognized codec per
  display, and ackless audio; 5.1 adds typed diagnostic-only audio
  MediaOptions (`0x8014`) and VehicleEnergyForecast (`0x8008`); 6.0 adds typed,
  bounded diagnostic-only extended video start and video MediaOptions.
- **Flow control stays channel-specific.** Phone-to-HU audio is ACKed per frame
  through 4.3 and ackless at 5.0+; video sends one ACK per accepted packet at
  every supported GAL, including 6.0. AVInput is the separate HU-to-phone flow.

## Input routing

- **EVIOCGRAB toggles with AA connection state** — grab on connect (route touch to AA), ungrab on disconnect (return touch to Wayland/libinput). A permanent grab steals touch from the launcher UI.
- **During AA, Navbar touch uses evdev zones registered by `NavbarController`**
  through `EvdevCoordBridge`; EVIOCGRAB prevents the Navbar's QML `MouseArea`s
  from owning Pi touch input.
- **3-finger gesture:** 3 simultaneous touches within 200ms → suppress AA forwarding, emit the overlay signal.
- **Qt's evdevtouch plugin causes duplicate events** — direct evdev with EVIOCGRAB is the only supported touch path (why `EvdevTouchReader` exists).

## Sockets

- **Keep `AndroidAutoOrchestrator`, `TCPTransport`, and `AASession` on their
  shared Qt event-loop thread.** Socket signals plus session and watchdog timers
  rely on their QObject affinity; queue work that crosses into that thread.
- **TCP keepalive alone won't detect dead connections when the Pi IS the AP** — no router sends RST. Poll `tcp_info` via `getsockopt(IPPROTO_TCP, TCP_INFO)` and check `tcpi_backoff >= 3` (~16s detection). `tcpi_retransmits` resets between polls.
- **Only include `<netinet/tcp.h>`** for `tcp_info` — it conflicts with `<linux/tcp.h>`.
- **`QTcpServer` owns the listener lifecycle.** Call `listen()`, check its
  result and `errorString()`, then accept through `newConnection` and
  `nextPendingConnection()` on the server's Qt event-loop thread.
- **A Qt socket descriptor is a borrowed native handle.** Check
  `socketDescriptor() != -1` on every use and never close it directly. Set
  `FD_CLOEXEC` on the listener after `listen()` and on each accepted socket,
  preserving existing descriptor flags with `F_GETFD` before `F_SETFD`.
- **The accepted `QTcpSocket` belongs to `TCPTransport` after `setSocket()`.**
  Apply native TCP options before that handoff; `activeSocket_` is non-owning,
  and transport teardown closes the socket through Qt.

## Logging

- **App-side AA logs belong to the `lcAA` Qt logging category.** Use the
  `qCDebug`/`qCInfo`/`qCWarning`/`qCCritical` family; the central Qt message
  handler owns filtering and output. Keep protobuf diagnostics concise with
  `QString::fromStdString(message.ShortDebugString())`.
