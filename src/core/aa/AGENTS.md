# src/core/aa/ — AA Protocol Rules

Hard-won protocol behavior for the Android Auto runtime. Root `AGENTS.md` holds hard constraints; `src/AGENTS.md` holds general Qt/PipeWire traps.

## Touch events (Android MotionEvent semantics)

- **Touch coordinates must be in VIDEO resolution space** (1280x720), NOT the `touch_screen_config` dimensions. Letterbox-aware mapping for the 1024x600 display.
- **`touch_screen_config` must equal the video resolution** — the phone interprets touch relative to it; mismatch causes touch misalignment.
- **ALL active pointers must be included in every message**, not just the changed finger.
- **`action_index` is the ARRAY INDEX** into the pointers list; **`pointer_id`** is the stable per-finger identifier (we use slot index).
- **UP events include the lifted finger** in the pointer array at its last position.

## Video

- **SPS/PPS arrives as `AV_MEDIA_INDICATION` (no timestamp)** — must be forwarded to the decoder.
- H.264 data already carries AnnexB start codes — do NOT prepend more. *(Flag: may be outdated — verify against open-android-auto before relying on it.)*
- **Phone sends `config_index=3` in `AVChannelSetupRequest`** even though our config list has indices 0–1 — it's the phone's internal reference, not an index into our list.
- **FFmpeg `thread_count` must be 1** for real-time decode — multi-threaded H.264 decoders buffer frames internally → permanent EAGAIN on phones sending small P-frames.
- **Accept both `AV_PIX_FMT_YUV420P` and `AV_PIX_FMT_YUVJ420P`** — some phones (Moto G Play 2024) output JPEG full-range; rejecting it silently discards frames (black screen).
- **`VideoConfig.margin_width/height` works** — phone renders a centered sub-region with black-bar margins; locked at session start. See `docs/aa-protocol/`.
- AA supports **fixed resolutions only**: `_480p` (800x480), `_720p` (1280x720, default), `_1080p` (1920x1080), portrait variants `_720p_p` / `_1080pp`.

## Input routing

- **EVIOCGRAB toggles with AA connection state** — grab on connect (route touch to AA), ungrab on disconnect (return touch to Wayland/libinput). A permanent grab steals touch from the launcher UI.
- **During AA, sidebar touch = evdev hit zones in `EvdevTouchReader`** — EVIOCGRAB steals all touch from Qt, so QML `MouseArea` there is visual-only on the Pi.
- **3-finger gesture:** 3 simultaneous touches within 200ms → suppress AA forwarding, emit the overlay signal.

## Sockets

- **TCP keepalive alone won't detect dead connections when the Pi IS the AP** — no router sends RST. Poll `tcp_info` via `getsockopt(IPPROTO_TCP, TCP_INFO)` and check `tcpi_backoff >= 3` (~16s detection). `tcpi_retransmits` resets between polls.
- **Only include `<netinet/tcp.h>`** for `tcp_info` — it conflicts with `<linux/tcp.h>`.
- **Boost.ASIO sockets don't set SOCK_CLOEXEC** — forked processes inherit the acceptor FD and block port rebind. `fcntl(fd, F_SETFD, FD_CLOEXEC)` after socket open.
- **SO_REUSEADDR must be set before bind** — the 2-arg ASIO acceptor constructor does open+bind+listen in one shot, too late. Use separate open / set_option / bind / listen.

## Logging

- **Boost.Log truncates multiline output** — use protobuf `ShortDebugString()`.
