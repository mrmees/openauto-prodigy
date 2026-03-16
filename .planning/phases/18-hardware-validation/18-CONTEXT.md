# Phase 18: Hardware Validation - Context

**Gathered:** 2026-03-16
**Status:** Ready for planning

<domain>
## Phase Boundary

End-to-end proof that representative TCP traffic traverses the companion SOCKS bridge on real Pi hardware with routing enabled. This is a correctness proof, not a load test or resilience check. Phase 15 (privilege model), Phase 16 (routing correctness), and Phase 17 (status reporting) are prerequisites.

</domain>

<decisions>
## Implementation Decisions

### Validation procedure
- Hybrid checkpoint pattern (same as Phase 16):
  - **Matt:** phone connected, companion app running, Pi deployed/restarted with latest code
  - **Claude over SSH:** enable route, run representative traffic, capture packet/log evidence, verify `get_proxy_status`
  - **Matt:** confirm visible client-side behavior if needed
- Non-autonomous checkpoint — Matt must confirm physical setup is ready before Claude runs SSH checks

### Traffic verification — three-layer proof chain
Every proof point requires evidence from all three layers:
1. **Pi-side redirect proof (loopback):** `tcpdump -i lo port {redirect_port}` — traffic to external destinations appears on loopback at the redirect port, proving iptables REDIRECT is feeding traffic into redsocks
2. **Pi-side bypass proof (outbound interface):** `tcpdump -i {active_outbound_interface}` filtered for test destination — no direct connection from Pi to test destination when proxy is enabled. Use active outbound interface (wlan0/eth0 as applicable), don't hardcode wlan0
3. **Companion-side receipt:** Check companion app logs (logcat or app-level logging) for SOCKS connection/forwarding events. **Must first verify what the companion actually logs** — if companion-side observability is insufficient, note it as a gap rather than fabricating evidence
4. **Request success:** curl returns expected response content

### Companion evidence source
- First inspect existing companion logs / logcat while a test SOCKS request runs
- If companion already emits a clear connection/forwarding signal, use it
- If not, document that companion-side observability is insufficient for end-to-end proof — do not pretend evidence exists

### Test targets (explicit, reproducible)
- **HTTP:** `http://httpbin.org/ip` — simple response, easy to correlate
- **HTTPS:** `https://www.google.com/` — ubiquitous, stable, realistic TLS traffic
- Keep exact URLs in the plan for reproducibility

### Pass/fail criteria
- **Negative case (proxy disabled):** One HTTP request (`httpbin.org/ip`) — verify it does NOT show companion-side traversal. Establishes baseline.
- **Positive case (proxy enabled):** One HTTP request + one HTTPS request, each with full 3-layer evidence chain (Pi redirect + companion receipt + request success)
- **Overall verdict:** PASS only if all required proof points succeed. FAIL if any proof point fails.
- This is a correctness proof, not resilience or soak testing

### Failure handling
- Continue collecting evidence on step failure — don't stop early
- Record pass/fail per step with captured output
- Full picture helps diagnose whether break is Pi redirect, companion SOCKS, or request target
- Overall verdict is FAIL if any required proof point fails

### Validation artifact
- Capture SSH-side commands and output to timestamped log on Pi: `/tmp/phase18-validation-YYYYMMDD-HHMMSS.log`
- Copy to repo: `docs/validation/2026-03-16-phase18-hardware-validation.log`
- Repo copy is the canonical artifact, referenced from VERIFICATION.md
- Chat summary of verdict, structured report deferred unless this becomes a repeatable regression gate

### Claude's Discretion
- Exact tcpdump filter expressions and capture duration
- How to detect the active outbound interface programmatically
- Log parsing/correlation approach for companion evidence
- Exact SSH command sequencing and timing between capture start and curl execution
- Validation log format and level of detail

### Not At Claude's Discretion
- Three-layer proof chain is mandatory (Pi redirect + companion receipt + request success)
- Negative case must be run before positive case
- Both HTTP and HTTPS in positive case
- Continue on failure (don't stop early)
- Verify companion logging capability before relying on it
- Artifact captured on Pi AND copied to repo

</decisions>

<specifics>
## Specific Ideas

- "tcpdump alone proves the Pi redirected locally, but not that the companion actually received and forwarded the traffic" — both Pi-side and companion-side evidence are needed
- "don't pretend chat history is evidence" — capture to a real file, commit to repo
- "stopping early saves a few minutes and often costs a whole extra validation cycle" — always collect full evidence
- "one request per type is enough to satisfy PX-05 cleanly" — don't bake a mini load test into this

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `proxy_manager.py`: enable/disable/get_status with verified ACTIVE state — ready for hardware test
- `openauto_system.py`: IPC handlers for set_proxy_route and get_proxy_status — the interface Claude calls over SSH
- Phase 16 checkpoint pattern: hybrid Matt-does/Claude-SSHs model, proven workflow

### Established Patterns
- SSH from claude-dev VM to Pi at 192.168.1.152 (direct, works through VMware NAT)
- ADB available on claude-dev VM via platform-tools/adb
- systemctl restart/status for service management
- journalctl for service logs

### Integration Points
- `openauto-system.service` on Pi — daemon that handles proxy routing
- Companion app on phone — SOCKS proxy server, accessible via ADB logcat
- Pi IPC socket at `/run/openauto/system.sock` — Claude can interact via Python client or direct JSON-RPC

</code_context>

<deferred>
## Deferred Ideas

- Structured test report format — graduate to this if Phase 18 becomes a repeatable regression gate
- Exemption spot-checks on hardware — Phase 16 unit/integration tests cover this
- Sustained/soak traffic testing — separate phase if resilience concerns arise
- Companion-side logging improvements — if current logging is insufficient, file as companion app enhancement

</deferred>

---

*Phase: 18-hardware-validation*
*Context gathered: 2026-03-16*
