# Session Handoffs

Newest entries first.

---

## 2026-07-27 — GAL 5.1 hardware acceptance

**What changed:** Task 5 accepted implementation HEAD
`ce08f8ff1f057746007a33eae1209df222c2123e`. The Pi ran
`ALPHA-26-07-24-01-109-gce08f8f`, ARM executable SHA-256
`77aa748005212dcb16c1c2d8db9db98c94c9683000c590eff17a18cc25a90409`.

**Status:** ACCEPTED. GAL negotiation requested 5.1 and received 6.0/MATCH,
with one H.264 configuration per display and simultaneous MAIN+CLUSTER
rendering. Active ch4 emitted zero audio ACKs while MAIN and CLUSTER video ACKs
advanced. The full operator live matrix passed, as did three manual reconnect
cycles, an explicit GAL 5.0 regression, and the final GAL 5.1 restoration.
Messages `0x8014` and `0x8008` were not delivered during the live run; their
synthetic coverage passed, so this conditional non-delivery is nonblocking.

**Verification:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and
`./cross-build.sh` passed. Final state retained app PID 12397 with
`NRestarts=0`, boot ID `bf4f1de6-5201-480f-beab-00b91fc2f4a0`, matching
deployed/runtime executable hashes, and explicit GAL 5.1 persisted. BlueZ
retained PID 4413, `NRestarts=0`, and its 2026-07-27 15:28:23 CDT start time.

**Evidence:** capture
`/home/matt/gal-5-1-captures-2026-07-27/20260727T213912Z-ce08f8f`;
rollback
`/var/backups/openauto-prodigy/20260727T213826Z-pre-ce08f8f-gal-5-1`.

**Next 1–3 steps:** Task 6 is GAL 6 implementation.

---

## 2026-07-27 — GAL 5.0 hardware acceptance

**What changed:** implementation and hardware acceptance are complete at
`a2b8aa8`, with the released protocol pin `5ff4aa2`. GAL is now a durable,
session-wide Android Auto setting independent of the CLUSTER lab; selectable
1.7/4.3/5.0 behavior remains available and 5.0 is the accepted default.

**Status:** ACCEPTED. The Pi runs
`ALPHA-26-07-24-01-105-ga2b8aa8`, executable SHA-256
`8ad6da18072ec97af00f8b8272ab99aaa137b0d909cd1367f593a9336e6cb30f`.
A BlueZ service restart rebuilt discovery and AA reconnected automatically
without restarting Prodigy; PID 4171 and `NRestarts=0` stayed unchanged. Three
consecutive manual phone disconnect/reconnect returns also passed. The operator
confirmed ch4/ch5/ch6 audio roles, Assistant mic/response, MAIN touch plus
Back/Home/TEL direct dialer, simultaneous MAIN+CLUSTER video, and exit/return.

**Verification:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and
`./cross-build.sh` passed. The 4.3 regression restored two MAIN codecs and
audio ACKs. Final 5.0 restoration requested 5.0, received 6.0/MATCH, advertised
one H.264 configuration per display, emitted zero audio ACKs on active ch4,
and continued advancing MAIN and CLUSTER video ACKs.

**Evidence:** captures are in
`/home/matt/gal-5-0-captures-2026-07-27-bluez-recovery/20260727T202704Z-a2b8aa8`;
rollback is
`/var/backups/openauto-prodigy/20260727T202704Z-pre-a2b8aa8-bluez-recovery`.
Intermittent brief audio skips correlated with phone/UI activity and were also
seen at GAL 1.7; current evidence does not support a GAL 5.0 regression, a
blocker, or a Pi-side fix claim.

**Next 1–3 steps:** Task 4 is GAL 5.1 typed tolerance. No 5.1 work has begun.

---
