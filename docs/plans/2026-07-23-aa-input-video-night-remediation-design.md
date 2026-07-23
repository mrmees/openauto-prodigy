# Android Auto Input, Video, and Night Remediation — Design

Date: 2026-07-23
Status: ACTIVE
Base: `origin/main` at `8ec8aeab9b19a03fedce36761bd3be2a91bd1475`

## Goal

Close one consolidated Android Auto correctness wave at the boundaries that own
the affected state: decoder stream lifetime, evdev reader-thread state, shared
day/night policy, and wireless-session admission/configuration. The change must
preserve the existing wireless-only protocol, UI coordinate contract, frozen
External API, HFP role, audio routing, and service lifetimes.

## Current-State Revalidation

| Area | Attempted refutation | Result |
|---|---|---|
| Decoder lifetime | Traced decoder construction, session teardown, stream signals, codec initialization, worker queue, and registered tests | Accepted. The process-long decoder retains codec/parser/detection state across sessions, while the queue test exercises a deleted local-copy algorithm |
| Evdev touch | Traced zone claims, same-report transitions, mapping setters, grab/ungrab, poll/read failure, setup ordering, and every registered touch test | Accepted. Phone-visible and locally claimed pointers are conflated; final-report arrays are reused for sequential transitions; mutable reader state crosses threads; failures can spin; current mapping coverage is orphaned |
| Night behavior | Compared the completed initial-state remediation with provider lifetime, shell theme calls, timed tests, and GPIO setup/recovery | Accepted. Initial AA state is now correct, but the provider is session-scoped, the shell remains one-shot, timed branches are not deterministically asserted, and GPIO setup failure is swallowed |
| Wireless lifecycle/config | Rechecked merged session teardown, listener admission, shutdown ordering, RFCOMM status, TCP-port readers, discovery descriptors, and setup responses | Accepted. Active sessions remain preemptible before authentication; shutdown can observe a deleted session; redundant discovery can clobber watchdog state; port and video-config counts have divergent owners |

No QML or companion-app change is required. Existing consumers already expect
the correct coordinate and day/night contracts.

## Design

### 1. Put decoder reset in the worker's ordered stream

`VideoDecoder` gains an explicit new-stream command handled by its decode
worker. Enqueuing that command removes queued packets from the prior stream;
when the worker reaches it, it clears parser/codec/frame/fallback state and
reinitializes the default H.264 probe state. Frames queued after the command
cannot overtake it. The orchestrator issues the command before wiring the new
session's video delivery.

The copied keyframe test is replaced by a test that drives the real worker
queue and verifies ordered reset plus H.264/H.265 re-detection. No compressed
packet shedding or wire behavior changes.

### 2. Make the evdev reader the sole mutable-state owner

All device, slot, phone-visible pointer, mapping, and grab state is mutated on
the reader thread after it starts. Setup completes the initial Navbar/content
configuration before thread start. Later mapping and grab requests enter a
small synchronized command snapshot consumed by the reader.

Phone-visible membership is tracked separately from raw evdev activity and
`TouchRouter` claims. A locally claimed DOWN never enters the AA pointer set.
For multiple transitions in one `SYN_REPORT`, each DOWN is applied before its
message and each UP after its message, producing DOWN then POINTER_DOWN and
POINTER_UP then UP with the required complete pointer arrays.

Poll error/hangup/invalid-fd and terminal read failures close the device and
enter a paced reopen loop; they never become a zero-delay continue loop. The
desired grab and current mapping survive a successful reopen. Narrow device-I/O
and event-processing seams exercise this without a physical daemon/device.

### 3. Give day/night state one application-lifetime owner

A core `NightModeService` owns the configured time/GPIO provider for the
application lifetime. It publishes one validity-gated state to both
`ThemeService` and the Android Auto sensor cache. `main.cpp` removes its
duplicate one-shot time calculation; the orchestrator consumes the shared
service and no longer creates or destroys providers with individual sessions.

`TimedNightMode` receives an injectable clock seam. `GpioNightMode` receives an
injectable sysfs root and treats export/direction/value failures as invalid
state while continuing a paced recovery attempt. Signals remain change-gated,
except the first valid sample publishes even when it equals default storage.

The existing `none` selection retains its present fallback behavior in this
wave; defining a new manual/ambient policy is out of scope.

### 4. Validate wireless admission and share negotiated values

An accepted TCP socket cannot replace an Active/Backgrounded session; it is
closed before any existing session changes. Pre-active replacement remains
available for failed negotiations. Redundant `phoneWillConnect` notification
is ignored while projection is active so the established watchdog state stays
intact.

Service shutdown first closes new admission, observes the current session with
a guarded connection, and only then requests graceful stop. A synchronous
disconnect skips the nested wait; no listener callback can re-enter it.

One validated TCP-port resolver is used to bind. Bluetooth discovery receives
the listener's effective port rather than rereading raw configuration, so even
an explicitly supported ephemeral test bind advertises the actual port.
Invalid textual or out-of-range production values fall back to 5277 with one
diagnostic.

`ServiceDiscoveryBuilder` exposes the number of video configs produced by its
same recognized-codec resolver. `VideoChannelHandler` receives that count
instead of a hard-coded two.

## Failure and Notification Semantics

- Decoder reset is ordered and silent; subsequent first-packet detection logs
  and behaves like a fresh process.
- Claimed pointers never gain AA membership. Device loss produces paced logs
  and recovery attempts without duplicate touch messages.
- Invalid night sources do not overwrite the last authoritative state. First
  valid recovery publishes once; unchanged samples remain silent.
- An extra TCP connection during active projection is rejected without state,
  watchdog, handler, or socket-owner changes.
- Invalid port configuration falls back consistently; descriptor and setup
  response always agree on the video-config count.

## Out of Scope

- Any protobuf edit, including the hands-off community proto directory and the
  frozen External API.
- USB AA, TLS/authentication policy, wireless discovery credentials, firewall
  rules, HFP, ofono, PipeWire, Bluetooth audio, EQ, logging, or companion app.
- VP9/AV1 decoder implementation, video-frame rendering policy, new touch
  gestures, QML layout, or new night-source semantics.
- Bluetooth/hostapd restart, phone re-pairing, GPIO wiring, HFP calls, or release
  tagging.

## Verification and Live Acceptance Matrix

| Check | Level | Acceptance |
|---|---|---|
| Decoder boundary | Required local | Ordered reset clears old work/state and permits H.264→H.265→H.264 detection through the real worker path |
| Touch transitions and claims | Required local | Claimed slots never appear; simultaneous down/up arrays and indices follow MotionEvent rules; mapping commands are coherent |
| Evdev failure | Required local | Error/hangup/short-read paths pace reopen and stop cleanly without spinning |
| Night providers/service | Required local | Normal/inverted time boundaries, first-valid/change-only signals, shell+AA propagation, GPIO setup failure and recovery are deterministic |
| Wireless lifecycle/config | Required local | Active preemption is rejected; pre-active replacement remains; shutdown does not stall/re-enter; port and video counts have one contract |
| Repository gates | Required local | Focused tests, full build, explicit app target, full CTest, docs links, diff check, and fully adjudicated review pass |
| aarch64 application | Required before deploy | `./cross-build.sh` succeeds from the reviewed commit |
| Pi health | Required live | Snapshot old binary; one responsive process; hostapd/Bluetooth PIDs and restart counts unchanged |
| Projection/video | Required live | Wireless reconnect succeeds; forced H.264 then H.265 sessions both decode first frame without application restart |
| Admission | Required live | A second local TCP client during active projection is rejected without interrupting projection or IPC |
| Night/touch | Required live | Temporarily forced day/night values reach shell and AA, restored config is exact, and normal projection/Navbar touch mapping is confirmed |
| GPIO hardware, second phone, daemon restart | Optional/not required | Unit sysfs seam is sufficient; no Bluetooth/hostapd restart, re-pair, HFP, or unrelated service test |

Matthew's standing authorization covers the scoped Pi snapshot, temporary
configuration, binary deployment, application restart, and restoration.
