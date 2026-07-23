# Session Handoffs

Newest entries first.

---

## 2026-07-23 — Android Auto protocol crypto/flow remediation COMPLETE

**What changed:** OpenSSL setup and established-session I/O now use checked,
transactional results with bounded diagnostics and fail-closed TLS record
handling. Fragmented-message assembly retains FIRST's declared total, enforces
16 MiB per-message and 32 MiB aggregate bounds, validates continuation flags
and exact completion, and surfaces one terminal protocol error. Audio returns
one receive permit per accepted frame, session liveness uses the configured
pong deadline with echoed-timestamp correlation, and navigation remains active
through REROUTING. The implementation is recorded in `fb54c37`, `c6d98aa`,
`a2ee3d7`, review fixes `5392092`, `714e0e5`, and `9ee3200`.

**Why:** unchecked or misclassified OpenSSL failures could crash, hang, or
silently poison a session; malformed fragmented input could grow without a
bound or deliver inconsistent payloads. Audio exhausted the advertised permit
window at every tenth frame, liveness ignored its timeout setting and accepted
uncorrelated pongs, and rerouting incorrectly cleared active guidance.

**Status:** COMPLETE on `agent/aa-protocol-crypto-flow-remediation`, based on
merged PR #30. The reviewed aarch64 binary at `9ee3200` was deployed after the
prior binary was retained at
`/var/backups/openauto-prodigy/20260723T123621Z`. One exact application process
(PID `250566`) owns responsive IPC and reports the deployed version. The Pixel
automatically reconnected, every AA service channel opened, all three audio
channels negotiated PCM, and H.265 hardware projection decoded its first
800x480 frame. No AA media stream was supplied during the bounded check, so
audible media was not exercised. Hostapd PID `46989` and Bluetooth PID `672`
were unchanged with zero restarts. The Pi's pre-existing dirty QML/submodule
state was preserved without pull, reset, clean, daemon restart, or re-pairing.

**Review gate:** the initial pass returned four findings, the required rerun
returned three, and the final candidate pass returned two. All nine were
confirmed and fixed; none were dismissed or left unadjudicated. The fixes cover
handshake-stage failure closure, initial ping ordering, short-message rejection,
gate command chaining, complete TLS-record consumption, raw fragmentation
metadata and size validation, safe liveness configuration, pong correlation,
and a valid-but-mismatched credential test.

**Verification:** focused protocol, cryptor, framing, messenger, session,
audio, navigation, and bridge tests passed. `cmake --build . -j$(nproc)`, the
explicit `openauto-prodigy` target, and `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation links and `git diff --check` passed.
`./cross-build.sh` produced the deployed aarch64 binary. Live validation covered
binary identity, exact process ownership, IPC, wireless discovery and session
establishment, all channel opens, PCM negotiation, H.265 first-frame decode,
deadline stability, and unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) publish this branch as a draft PR targeting `main`;
(2) review and merge it as an independent wave; (3) revalidate the next bounded
subsystem wave before activating another public plan.

---
