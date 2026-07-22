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
  discovery does not advertise them. Each enabled codec gets a config for the
  selected mode.
- **The shipped decode path supports H.264/AVC and H.265/HEVC.** Service
  discovery defaults to those codecs, and the decoder detects which of the two
  the phone sends. Keep advertised codec choices aligned with decoder support.

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
