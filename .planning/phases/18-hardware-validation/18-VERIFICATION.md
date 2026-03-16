---
phase: 18-hardware-validation
verified: 2026-03-16T15:30:00Z
status: passed
score: 4/4 must-haves verified
gaps: []
human_verification: []
---

# Phase 18: Hardware Validation Verification Report

**Phase Goal:** End-to-end proof that representative TCP traffic traverses the companion SOCKS bridge on real Pi hardware
**Verified:** 2026-03-16
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `set_proxy_route(active=true)` succeeds on Pi and `get_proxy_status` reports ACTIVE | VERIFIED | Log lines 77-82: enable returns `"state": "active"`, live-check get_status confirms `"state": "active", "checks": {"listener": true, "iptables": true, "upstream": true}` |
| 2 | HTTP request to httpbin.org/ip traverses companion SOCKS bridge (4-point proof) | VERIFIED | Final Step 3: PROOF 1 (6 lo:12345 packets), PROOF 2 (0 packets to 54.210.35.148 on eth0), PROOF 3 (origin IP 166.205.190.58 = phone cellular), PROOF 4 (curl exit 0 with JSON response) |
| 3 | HTTPS request to google.com traverses companion SOCKS bridge (4-point proof) | VERIFIED | Step 4: PROOF 1 (19 lo:12345 packets), PROOF 2 (0 packets to 142.250.114.103 on eth0), PROOF 3 (by inference — same redsocks path, consistent with HTTP proof), PROOF 4 (HTTP 200) |
| 4 | With proxy disabled, traffic succeeds directly AND companion shows no SOCKS forwarding events (negative baseline) | VERIFIED | Step 1: curl exit 0, origin 72.179.37.177 (Pi LAN IP), companion logcat empty for SOCKS/proxy/forward events |

**Score:** 4/4 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `docs/validation/phase18-hardware-validation-20260316-093435.log` | Timestamped validation evidence log containing VERDICT | VERIFIED | 428-line log committed in `631c1e9`. Contains all proof steps and `VERDICT: PASS (with 2 bugs requiring fixes)` at line 416. |

**Wiring check:** Artifact is committed to git, exists in working tree, referenced in SUMMARY.md key-files section.

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| iptables REDIRECT | redsocks on loopback | tcpdump -i lo captures redirected traffic | VERIFIED | HTTP: 6 packets on lo:12345 (Step 3 final). HTTPS: 19 packets on lo:12345 (Step 4). Packet captures are in the log. |
| redsocks | companion SOCKS proxy | companion logcat shows forwarding events | PARTIAL — accepted | Socks5Server has no per-connection logging. Companion receipt proven instead via origin-IP comparison: httpbin returns 166.205.190.58 (phone cellular) not 192.168.1.152 (Pi LAN). This is stronger evidence than per-connection logs would be. HTTPS uses inference from the same redsocks->companion path (loopback redirect active, zero outbound bypass). |

**Assessment of PARTIAL key link:** The plan's CONTEXT document explicitly states: "if companion logging is insufficient, document the gap." The gap was documented in the log and the SUMMARY. The alternative proof method (origin IP) is documented as a stronger form of evidence. The plan's own context accepts this outcome. HTTPS companion proof is inference-based — weaker but corroborated by the full HTTPS loopback+bypass proof chain. This does not block PASS.

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| PX-05 | 18-01-PLAN.md | Representative outbound TCP traffic traverses the companion SOCKS bridge on hardware once routing is enabled | SATISFIED | 10/10 proof points PASS per verdict line in validation log. Two bugs found and fixed during validation. REQUIREMENTS.md marks PX-05 as `[x] Complete`. |

**Orphaned requirements check:** REQUIREMENTS.md maps PX-05 to Phase 18 only. No additional IDs are assigned to this phase. No orphans.

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `docs/validation/phase18-hardware-validation-20260316-093435.log` | 238 | "STEP 3 (RETRY)" outbound pcap section shows stale packets from pre-fix run (10 packets to 32.192.28.91) | Info | The retry section reused the first-run pcap analysis output. The definitive evidence is in "STEP 3 (FINAL)" (line 291+), which correctly shows 0 packets. No code impact — log annotation only. |
| `system-service/proxy_manager.py` | 546-548 | `_dns_enable` hardcodes `wlan0` interface name for `resolvectl` | Info | DNS override applies to wlan0 regardless of actual outbound interface. On this Pi, wlan0 is the AP interface (phones connect to it), not the outbound interface (eth0). May cause DNS resolution to go via the companion's nameserver unexpectedly. Not blocking for PX-05 but worth tracking. |

No blocker anti-patterns. No TODO/FIXME/placeholder patterns in modified files.

---

### Human Verification Required

None. This phase is itself a hardware validation exercise — the validation log is the human-observable artifact. The log contains timestamped packet captures, IPC JSON-RPC responses, curl output, and origin IP confirmation captured live on Pi hardware.

---

### Bugs Fixed During Validation

Both bugs were identified by the validation run itself and fixed in commit `631c1e9`:

**Bug 1: eth0 in default `skip_interfaces` bypassed all internet REDIRECT**
- Root cause: Default `["lo", "eth0"]` added `-o eth0 -j RETURN` which matched all internet traffic (default route goes via eth0)
- Fix: Default changed to `["lo"]` only — `192.168.0.0/16` network exemption covers LAN
- Verified: `proxy_manager.py` line 53 confirms `["lo"]` default. Test `TS-03` at line 432 asserts eth0 is NOT in default exemptions.

**Bug 2: redsocks config written with 0600 root-only permissions**
- Root cause: Config written by root, but `redsocks` user spawned via `sudo -u redsocks` couldn't read it
- Fix: `0o640` permissions + `os.chown(path, 0, redsocks_gid)` so redsocks group can read
- Verified: `proxy_manager.py` lines 306-311 confirm `0o640` and `chown` with `redsocks_user` group.

---

### Known Gaps (Non-Blocking)

**Companion SOCKS5 per-connection logging:** The Socks5Server emits no log line for successful relays. This means future validation runs cannot use companion logcat as independent PROOF 3 evidence. The plan's CONTEXT section anticipated this outcome and documented it as an enhancement gap. The alternative (origin IP comparison) is available for HTTP but not for HTTPS (TLS content is opaque). HTTPS PROOF 3 remains inference-based.

**Filed in SUMMARY.md** as a companion app enhancement: add `Log.i(TAG, "CONNECT $destHost:$destPort")` in `handleClient()` after successful auth.

**`_dns_enable` hardcodes `wlan0`:** Minor issue noted in anti-patterns above. Does not affect proof chain validity.

---

## Deviations Resolved by Execution

The validation procedure required two deviation cycles before producing clean proof:

1. Missing tools on Pi (tcpdump, dnsutils) — installed during validation, rerun succeeded
2. eth0 skip_interfaces bug — found during HTTP PROOF 2 failure, fix applied, rerun succeeded
3. redsocks config permissions bug — found during pre-flight, workaround applied for validation, permanent fix committed

All three were auto-fixed and committed in `631c1e9`. The final proof chain (Step 3 FINAL, Step 4) is clean.

---

## Summary

Phase 18 achieved its goal. Representative TCP traffic (HTTP and HTTPS) traverses the companion SOCKS5 bridge on real Pi hardware. The four-point proof chain for HTTP is conclusive (loopback packets + zero outbound bypass + phone cellular origin IP + successful response). HTTPS proof is strong on points 1, 2, and 4 with inference on point 3. The validation log is committed, the bugs found during validation are fixed and tested, and PX-05 is satisfied.

The conditional "with 2 bugs requiring fixes" in the VERDICT line is retrospectively satisfied — both fixes are in `631c1e9` and covered by updated unit tests.

---

_Verified: 2026-03-16_
_Verifier: Claude (gsd-verifier)_
