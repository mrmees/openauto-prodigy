# Milestone 5: AV Optimization & System Service (Feb 25-26, 2026)

## What Was Built

### Audio Pipeline

- **Per-stream ring buffer sizing.** Replaced the hardcoded 100ms ring buffer with configurable per-stream sizes: media=50ms, speech/system=35ms. Configured via `audio.buffer_ms` in YAML.
- **Silence insertion at water-mark.** PipeWire RT callback now always outputs full periods (`d.chunk->size = maxSize`) and silence-fills any gap after the ring buffer read. Prevents partial-period reads that caused audio glitches and tempo wobble.
- **Adaptive buffer growth.** Monitors PipeWire xrun (underrun) count per stream. If 2+ xruns occur in a check interval, buffer grows by 10ms up to the original 100ms cap. Resets to configured default on next AA session. Enabled via `audio.adaptive: true`.

### Video Pipeline — Transport Layer

- **Circular buffer frame parser.** Replaced `QByteArray` with `append()`/`remove(0,N)` (O(n) memmove every frame) with a proper circular buffer: O(1) consume via cursor advance, auto-grow on overflow, linearization only at wrap boundary. Initial capacity 64KB.
- **Zero-copy message ID extraction.** Changed `IChannelHandler::onMessage()` to accept `{payload, dataOffset}` instead of `payload.mid(2)` which allocated a new QByteArray for every AA message. All 11 channel handlers updated.

### Video Pipeline — Decode Path

- **Timestamp fix.** Replaced AA protocol timestamps (phone clock, usec since session start) with `steady_clock::now()` at the emit site. Previous timestamps produced nonsense queue metrics (~101 billion ms).
- **Zero-copy payload forwarding.** Changed `videoFrameData` signal from `QByteArray` to `std::shared_ptr<const QByteArray>`. The `Qt::QueuedConnection` now copies a shared_ptr (cheap refcount bump) instead of deep-copying 50-150KB keyframes.
- **Bounded decode queue with keyframe awareness.** Capped decode queue at 2 frames. On overflow: drop oldest non-keyframe first. If forced to drop a keyframe, set `awaitingKeyframe_` flag, discard all non-IDR frames, flush decoder references, resume on next IDR. Video ACKs still sent for dropped frames (phone flow controller requires them).
- **Latest-frame-wins display delivery.** Replaced per-frame `QMetaObject::invokeMethod(Qt::QueuedConnection)` with a mutex-guarded latest-frame slot. A QTimer on the main thread reads the slot at ~60Hz. Prevents frame queue buildup in Qt's event loop when main thread is busy.
- **Telemetry split.** Split single perf metric into queue wait, decode time, upload/submit time, and total. Added drop count. Format: `[Perf] Video: queue=0.3ms decode=3.2ms upload=0.1ms total=3.6ms (max=5.1ms) | 29.8 fps drops=0`.

### Video Pipeline — Codec & Hardware Decode

- **Codec capability detection.** Probes FFmpeg at startup for available decoders per codec (H.264, H.265, VP9, AV1). Each codec checked against ordered list of hw then sw decoders via `avcodec_find_decoder_by_name()`. Results stored in YAML `video.capabilities` section.
- **Multi-codec ServiceDiscovery.** ServiceDiscoveryBuilder reads enabled codecs from `video.codecs` config instead of hardcoding H.264 + H.265. Maps codec names to protobuf enum values dynamically.
- **Video Settings UI.** Per-codec enable/disable checkboxes (H.264 always on), hardware/software toggle (disabled if no hw decoders found), decoder dropdown. Backed by `CodecCapabilityModel` (QAbstractListModel).
- **FFmpeg hardware decoder selection with 3-tier fallback.** Decoder init tries: (1) user-configured decoder, (2) auto-probed hw decoders in priority order, (3) software fallback. If `avcodec_open2()` fails with hw decoder, automatically retries with software. First-frame fallback: if hw decoder opens but first `avcodec_receive_frame()` fails, tear down and reinitialize with software.
- **HW accelerated decoder selection (named + hwaccel).** Two fundamentally different hw decode paths unified:
  - **Named decoder** (e.g. `h264_v4l2m2m`): standalone decoder via `avcodec_find_decoder_by_name()`
  - **Hwaccel** (e.g. DRM): standard software decoder with `hw_device_ctx` attached via `av_hwdevice_ctx_create()`
  - Default fallback chains per codec: named v4l2m2m -> DRM hwaccel -> VAAPI -> software
  - Config supports `"auto"`, `"drm"`, `"vaapi"`, or any named decoder string
- **DRM hwaccel for Pi 4 HEVC.** Pi 4's HEVC decoder (`rpi-hevc-dec`, `/dev/video19`) uses the stateless V4L2 request API. `hevc_v4l2m2m` fails ("Could not find a valid device") because bcm2835-codec only does stateful H.264. The fix: `av_hwdevice_ctx_create(AV_HWDEVICE_TYPE_DRM, renderNode)` + `get_format` callback requesting `AV_PIX_FMT_DRM_PRIME`. Requires FFmpeg 7.1.3+rpt1 with `--enable-v4l2-request` on kernel 6.12.
- **DmaBufVideoBuffer zero-copy (Qt 6.8+).** Wraps DRM PRIME fd from hardware decode output. Implements `textureHandle()` for Qt RHI path (EGL image import from dmabuf). Falls back to `map()` with `av_hwframe_transfer_data()` for CPU copy when RHI unavailable. Compile-time gated with `#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)`.
- **VideoFramePool with actual recycling.** Pre-allocated ring of QVideoFrames for software decode path. Round-robin cycling with pool size 3-4. DmaBuf frames excluded (tied to FFmpeg buffer lifetime). Eliminates ~1.4MB/frame allocation at 720p.

### Companion App + QR Pairing

- **QR code pairing.** Vendored qrcodegen library (nayuki, MIT, header + source). When user generates pairing PIN, also encodes `openauto://pair?pin=XXXXXX&host=10.0.0.1&port=9876` into QR code rendered as SVG, exposed to QML as base64 data URI. PIN text kept as fallback with hint "Scan the QR code, or enter this PIN manually."

### System Service Daemon

- **openauto-system.** Python asyncio daemon running as root via systemd. Single process, three internal components:
  - **IPC Server:** Unix socket at `/run/openauto/system.sock`, newline-delimited JSON request/response. Methods: `get_health`, `get_status`, `apply_config`, `restart_service`, `set_proxy_route`, `get_proxy_status`.
  - **Health Monitor:** 5-second polling of hostapd, systemd-networkd, bluetooth.service. Functional checks beyond `is-active` (wlan0 has correct IP, hci0 is UP RUNNING). Auto-recovery with 3 fast retries (5s backoff), then slow retry (60s). Stability reset after 120s healthy.
  - **Config Applier:** Reads app's `~/.openauto/config.yaml`, validates values (SSID 1-32 chars, WPA2 >= 8 chars, valid IP), rewrites system config files from Python templates (hostapd.conf, systemd-networkd), restarts affected services. Atomic writes (write .tmp, os.rename).
- **BT profile registration moved to daemon.** HFP AG, HSP HS, and AA RFCOMM profiles registered via `dbus-next` (async D-Bus) calling `ProfileManager1.RegisterProfile()`. Re-registered automatically when bluetooth.service restarts.
- **SystemServiceClient (C++ Qt side).** `QLocalSocket`-based client connecting to daemon socket. Q_INVOKABLE methods for QML. Auto-reconnect on disconnect (5s retry). Q_PROPERTY for `health`, `routeState`, `routeError`.

### SOCKS5 Transparent Proxy Routing

- **ProxyManager.** Python class managing redsocks + iptables for transparent SOCKS5 routing. States: DISABLED, ACTIVE, DEGRADED, FAILED.
  - Writes redsocks config (SOCKS5 credentials), starts redsocks process, applies iptables nat rules
  - Custom iptables chain `OPENAUTO_PROXY` with exclusions: loopback (127.0.0.0/8), AP subnet (10.0.0.0/24), LAN (192.168.0.0/16)
  - Health probe every 30s: TCP connect to proxy host:port, 3 failures -> DEGRADED state, success restores ACTIVE
  - systemd `ExecStopPost` unconditionally cleans iptables rules (with `-` prefix for ignore-errors)
- **CompanionListenerService integration.** Tracks `socks5.active` state changes from companion's 5-second status stream. Only calls `setProxyRoute()` on transitions (not every status packet). Disables proxy on companion disconnect.
- **Proxy route state reconciliation.** On system service reconnect, re-applies proxy route if companion reports active SOCKS5. Validates payload (host required, port in bounds, password required). Clears applied state on disconnect.
- **UI indicator in CompanionSettings.** Status dot: green (active/routing via phone), amber (degraded/retrying), grey (inactive). Visible when companion connected and internet available.

## Key Design Decisions

### Video Pipeline Architecture

- **Never drop compressed video packets.** Dropping any P-frame breaks the decoder's reference chain, causing persistent visual corruption until the next keyframe. Load-shedding happens at two safe points: (1) bounded decode queue drops oldest frame with keyframe awareness and decoder flush, (2) latest-frame-wins at display delivery.
- **`AVDISCARD_NONREF` is safe, `AVDISCARD_NONKEY` is NOT.** `NONKEY` skips P-frames, which are reference frames. Only B-frames (non-reference) can be safely discarded at the FFmpeg level.
- **H.264 software decode is faster than hw on Pi 4 (kernel 6.12).** `h264_v4l2m2m` (bcm2835-codec) has M2M plumbing overhead. NEON-optimized software decoder wins on throughput. DRM hwaccel is the win for HEVC specifically.
- **Frame path inferred at runtime, not declared per-candidate.** After first successful decode, check `frame->format`: DRM_PRIME -> DmaBuf path, YUV420P/YUVJ420P -> frame pool, anything else -> `av_hwframe_transfer_data()` to CPU then pool.
- **SBC portability via data-driven candidate lists.** Adding a new SBC (Rockchip, Amlogic, Intel) = reorder entries in per-codec fallback chain. No new classes or code paths. Pi 4 H.264 uses stateful v4l2m2m, HEVC uses stateless DRM hwaccel. Rockchip would use DRM hwaccel for everything.

### System Service Architecture

- **Python for the daemon, not C++.** Already on the Pi (Flask web panel), natural for system scripting, asyncio for non-blocking subprocess calls. Single-user embedded system, so root privilege avoids polkit/sudo complexity.
- **Daemon owns system config files exclusively.** Template-based rewriting (not sed/regex). App writes to config.yaml, daemon reads and applies. Single source of truth, no config drift.
- **Functional health checks, not just is-active.** A service can be "active" but broken (hostapd running but wlan0 has wrong IP, bluetooth running but hci0 not UP). Functional checks catch these cases.
- **BT profiles moved from app to daemon.** Daemon is the right owner because it runs as root, persists across app restarts, and can re-register profiles when bluetooth.service restarts.

### Proxy Routing

- **Companion state-change driven, not polling.** Only calls `setProxyRoute()` when `socks5.active` transitions, not on every 5-second status packet. Avoids unnecessary iptables reconfiguration.
- **Custom iptables chain for clean teardown.** `OPENAUTO_PROXY` chain flushed and deleted on disable. `ExecStopPost` in systemd unit provides unconditional cleanup even on crash.
- **Pi uses NetworkManager, not systemd-resolved.** `resolvectl` unavailable. proxy_manager handles this gracefully (skips DNS override, LAN DNS works via eth0 exclusion from iptables rules).

### QR Pairing

- **SVG data URI, not PNG.** SVG scales cleanly at any resolution, QML `Image` renders it natively. (Note: SVG data URIs were later found to not work on Qt/Pi — must use PNG via QImage. See MEMORY.md.)
- **Custom URI scheme `openauto://pair`** for deep-linking into companion app. Carries PIN, host, and port in one scannable code.

## Lessons Learned

### Video Pipeline

- **fmt=178 is `AV_PIX_FMT_DRM_PRIME`** — the DMA buffer pixel format from V4L2 request hwaccel. Not documented in obvious places.
- **Pi 4 V4L2 devices:** `/dev/video10` = bcm2835-codec (H.264 stateful), `/dev/video19` = rpi-hevc-dec (HEVC stateless). `hevc_v4l2m2m` tries bcm2835-codec which doesn't do HEVC.
- **RPi FFmpeg `--enable-sand`** handles Sand/NC12 pixel format conversion in `av_hwframe_transfer_data()`. Without it, DRM PRIME frame transfer to CPU produces garbled output.
- **Samsung S25 Ultra codec preference:** Highest resolution, H.265 > H.264. VP9/AV1 ignored. H.265 1080p software decode: ~26ms/frame. 480p: ~6ms/frame.
- **1080p H.265 software decode baseline on Pi 4:** 125% CPU (1.25 of 4 cores), 22-29ms/frame decode, 4.2ms YUV memcpy, 28-33fps, 62.3C no throttling. Expected post-optimization with DRM hwaccel: 15-25% CPU, 2-5ms decode, <0.5ms zero-copy upload.
- **`FFmpeg thread_count` must be 1 for real-time AA decode.** Multi-threaded H.264 decoders buffer frames internally before producing output, causing permanent EAGAIN on phones that send small P-frames.

### System Service

- **Configuration vs YamlConfig sync.** `Configuration` (INI-based) has hardcoded defaults. `YamlConfig` reads YAML. BT service uses `Configuration`. Must sync YAML -> Configuration in main.cpp or BT service gets stale defaults (caused WPA MIC failure with wrong WiFi password).
- **`bt.close()` hangs in BtProfileManager.** `systemctl restart openauto-system` hangs indefinitely. Workaround: `sudo pkill -9 -f openauto_system.py` then restart. Needs proper fix with timeout or async close.
- **Pi OS Trixie missing packages.** `iptables` and `redsocks` not installed by default. Both added to `install.sh`.
- **AA APK uses WiFi password verbatim.** No PBKDF2, no hex, no base64. The `key` field from WifiSecurityResponse goes straight to `WifiNetworkSpecifier.setWpa2Passphrase()`. Quote-stripping only.

### Proxy Routing

- **TCP dead peer detection on local AP:** `tcp_info.tcpi_backoff >= 3` is the reliable check. `tcpi_retransmits` resets between polls.
- **SOCKS5 password is first 8 hex chars of `~/.openauto/companion.key`.** Confirmed by companion app developer.
- **redsocks Debian package auto-starts with default config.** Must `systemctl disable --now redsocks` after install.

## Expected Performance Results

| Metric | Before (SW decode) | After (DRM hwaccel, estimated) |
|--------|--------------------|---------------------------------|
| CPU (1080p H.265) | 125% | 15-25% |
| Decode time | 22-29ms/frame | 2-5ms/frame |
| Copy/upload time | 4.2ms/frame | <0.5ms (zero-copy) |
| Worst-case latency | Unbounded | ~66ms (2-frame queue cap) |
| Memory per frame | malloc each | Recycled pool |
| Payload copy | 50-150KB deep copy | Shared pointer refcount |
| Audio latency | 100ms fixed | 35-50ms configurable |

## SBC Portability Matrix

| SBC | H.264 path | H.265 path | Notes |
|-----|-----------|-----------|-------|
| Pi 4 | `h264_v4l2m2m` (stateful) | DRM hwaccel (stateless v4l2request) | hevc_v4l2m2m fails on Pi 4 |
| Pi 5 | DRM hwaccel (likely) | DRM hwaccel | VideoCore VII, same driver family |
| Rockchip RK3588 | DRM hwaccel (rkvdec2) | DRM hwaccel (rkvdec2) | Stateless for all codecs |
| Amlogic S905X3/4 | Named v4l2m2m (stateful) | Named v4l2m2m (stateful) | Stateful for all codecs |
| Intel/AMD x86 | VAAPI | VAAPI | Standard desktop path |

## Source Documents

- `2026-02-25-av-pipeline-optimization-design.md` — 5-layer AV pipeline optimization architecture (codec detection, decoder selection, hybrid QVideoFrame, transport copy reduction, audio latency)
- `2026-02-25-av-pipeline-optimization-plan.md` — 12-task implementation plan for AV pipeline (audio buffer sizing, silence insertion, adaptive growth, circular buffer, zero-copy messenger, codec capability, ServiceDiscovery, settings UI, frame pool, hw decoder, DmaBuf, integration)
- `2026-02-25-hwaccel-decoder-selection-design.md` — Named decoder vs hwaccel unification design (DecoderCandidate struct, fallback chains, Pi 4 HEVC stateless discovery)
- `2026-02-26-video-pipeline-optimization-design.md` — 7-task video pipeline design (timestamp fix, zero-copy payload, bounded queue, latest-frame-wins, DRM hwaccel + DmaBuf, frame pool recycling, telemetry split)
- `2026-02-26-video-pipeline-optimization-plan.md` — Detailed implementation plan for video pipeline tasks with file paths, code snippets, and Pi deployment instructions
- `2026-02-26-proxy-reapply-resilience-plan.md` — SOCKS5 proxy state reconciliation on system service reconnect, daemon-side payload validation
- `2026-02-26-proxy-routing-plan.md` — 7-task plan for transparent SOCKS5 proxy routing (ProxyManager, IPC wiring, redsocks install, SystemServiceClient, CompanionListener integration, QML indicator, Pi deployment)
- `2026-02-26-qr-pairing-design.md` — QR code pairing design (qrcodegen library, SVG rendering, openauto:// URI scheme, CompanionSettings QML integration)
- `2026-02-26-qr-pairing-implementation.md` — 4-task QR pairing implementation (vendor library, CompanionListenerService QR generation, QML display, Pi testing)
- `2026-02-26-system-service-design.md` — System service daemon architecture (IPC protocol, health monitor design, config applier, BT profile registration, systemd lifecycle)
- `2026-02-26-system-service-implementation.md` — 7-task implementation plan (IPC server, health monitor, config applier, BT profiles, systemd wiring, SystemServiceClient C++, Pi deployment)
