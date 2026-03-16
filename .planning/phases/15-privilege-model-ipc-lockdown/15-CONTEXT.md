# Phase 15: Privilege Model & IPC Lockdown - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

The system daemon (`openauto-system.service`) gains sufficient privilege for all system-level operations it performs (iptables, systemctl, rfkill/sysfs, SDP permission fixes), and the IPC socket is locked down so only intended clients can invoke privileged methods. Both installers handle upgrade migration from the old unprivileged model.

</domain>

<decisions>
## Implementation Decisions

### Privilege elevation approach
- Run `openauto-system.service` as root — no capabilities-only approach, no daemon splitting
- Rationale: the daemon performs iptables, service restarts, rfkill/sysfs writes, BT recovery, and SDP permission fixes — too broad for capabilities without splitting, and splitting is out of scope
- The UI/Qt client remains unprivileged
- Finer-grained privilege (split helper) deferred to post-v1.0 if ever needed

### Systemd unit hardening
- Minimal, low-risk hardening only — not an enterprise sandbox exercise
- Add: `PrivateTmp=yes`, `ProtectHome=yes`, `RestrictSUIDSGID=yes`, `RestrictRealtime=yes`, `LockPersonality=yes`
- Skip invasive directives: `ProtectSystem`, `ProtectKernelTunables`, `ProtectControlGroups`, `MemoryDenyWriteExecute`, syscall filtering
- Do not use `NoNewPrivileges=yes` if it interferes with actual daemon responsibilities
- Keep `RuntimeDirectory=openauto`

### ExecStopPost safety net
- Keep ExecStopPost iptables cleanup lines — now they actually work under root
- Treat as belt-and-suspenders, not the primary cleanup path
- Daemon's own `disable()` is the normal cleanup source of truth
- If proxy mutations expand beyond iptables (DNS etc.), either extend ExecStopPost or document it as "iptables stale-rule cleanup only" with startup self-heal for the rest

### IPC access control
- Socket mode `0660` (not `0666`)
- Dedicated `openauto` group — not the installing user's primary group
- Socket owned by `root:openauto`
- Peer credential validation (SO_PEERCRED) as defense-in-depth on every connection:
  - Accept connection
  - Inspect peer creds (uid/gid/pid)
  - If unauthorized: return structured JSON error `{"error": {"code": -32600, "message": "permission denied"}}`, log at warning/info with uid/gid/pid, close connection
  - If authorized: proceed normally
- Group gate is the practical access mechanism; peer creds catch misconfiguration

### Group management
- Create/use dedicated `openauto` group
- Installer handles group creation (`groupadd`) and user membership (`usermod -aG openauto $USER`)
- Qt client user must be a member of `openauto` to connect to the socket

### Unit file template
- `system-service/openauto-system.service` becomes the canonical template with simple placeholders
- Placeholders for machine-specific values only: Python path, repo/system-service path
- Both `install.sh` and `install-prebuilt.sh` render from this same template via `sed` substitution
- No separate hand-maintained "reference" file — one source of truth
- Keep the template readable — simple placeholders, not a mini-templating system

### Upgrade/migration path
- Auto-migrate with notice — detect old `User=$USER` unit, print what's changing, proceed automatically
- No interactive prompt for the migration step
- Migration sequence:
  1. Stop the old service cleanly
  2. Clean stale iptables proxy state (delete OUTPUT→OPENAUTO_PROXY jump, flush chain, delete chain — ignore "not found" errors)
  3. Create `openauto` group if needed, add user to it
  4. Replace/update the unit file from template
  5. Correct socket directory and socket permissions
  6. Daemon-reload + restart under new privilege model
- Both `install.sh` and `install-prebuilt.sh` get identical migration logic
- Stale rule cleanup in installer is explicit — does not depend on Phase 16's startup self-heal
- Shared helper functions between installers where practical

### Claude's Discretion
- Exact peer credential validation implementation details (Python asyncio SO_PEERCRED specifics)
- Whether to validate UID membership in `openauto` group or just check GID directly
- Exact template placeholder syntax (e.g., `{{PYTHON_PATH}}` vs `@PYTHON_PATH@`)
- Test structure and granularity for TS-05 (IPC permission tests)

</decisions>

<specifics>
## Specific Ideas

- "Don't turn this into a systemd-hardening fetish exercise. The goal is not 'maximum lockdown,' it's 'don't run a root daemon like an idiot.'"
- "Capabilities-only is the wrong answer unless you also split the daemon" — explicit rejection of the common reflex
- Peer creds: "the real security comes from socket permissions and peer credential checks, not from pretending silence is meaningful protection"
- Migration: "I do not want 'works on fresh install' while upgrade leaves stale socket perms, stale rules, or mismatched service state"
- Installer drift: "one upgrade story, not two slightly different ones waiting to bite you later"
- Template: "one source of truth beats 'reference file plus separately generated unit' every time"

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `system-service/ipc_server.py`: Async Unix socket server — needs socket permission change (line 42: `os.chmod(path, 0o666)`) and peer credential validation added
- `system-service/openauto_system.py`: Daemon entry point — shutdown sequence, method dispatch already structured
- `src/core/services/SystemServiceClient.cpp`: Qt client — uses `QLocalSocket`, should work unchanged after group membership is correct
- `system-service/openauto-system.service`: Reference unit — becomes template, needs `User=root` + hardening directives + placeholder conversion

### Established Patterns
- JSON-RPC style request/response over Unix socket (newline-delimited JSON)
- `sd_notify(READY=1)` for systemd Type=notify readiness
- ExecStopPost with `-` prefix for error-tolerant cleanup
- install.sh step-based TUI with status reporting

### Integration Points
- `install.sh` Step 8 (lines 1656-1711): system service setup — needs migration logic, group creation, template rendering
- `install-prebuilt.sh`: needs same migration logic as install.sh
- `CompanionListenerService.cpp`: calls `setProxyRoute()` — no changes needed, client-side is unaffected
- `main.cpp`: creates `SystemServiceClient` — no changes needed

</code_context>

<deferred>
## Deferred Ideas

- Splitting privileged operations into a narrower root helper with the main daemon unprivileged — post-v1.0 architecture decision
- Startup self-heal for stale proxy state — Phase 16 (Routing Correctness & Idempotency)
- Extending ExecStopPost to cover DNS/non-iptables mutations — evaluate during Phase 16

</deferred>

---

*Phase: 15-privilege-model-ipc-lockdown*
*Context gathered: 2026-03-15*
