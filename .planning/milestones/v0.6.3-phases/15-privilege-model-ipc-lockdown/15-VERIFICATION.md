---
phase: 15-privilege-model-ipc-lockdown
verified: 2026-03-16T02:30:00Z
status: passed
score: 11/11 must-haves verified
re_verification: false
human_verification:
  - test: "Post-install Qt client connectivity"
    expected: "openauto-prodigy service connects to /run/openauto/system.sock without permission denied after usermod -aG openauto + reboot"
    why_human: "usermod group changes do not take effect in the current session; requires logout/login or reboot before automated verification is meaningful"
---

# Phase 15: Privilege Model & IPC Lockdown — Verification Report

**Phase Goal:** Establish daemon privilege model (root) and IPC socket lockdown (openauto group). Create service template, add peer credential auth, update installers.
**Verified:** 2026-03-16T02:30:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Service template specifies User=root with minimal hardening directives | VERIFIED | `openauto-system.service.in` line 8: `User=root`; lines 19-23: PrivateTmp, ProtectHome, RestrictSUIDSGID, RestrictRealtime, LockPersonality |
| 2 | IPC socket created with mode 0660 owned by root:openauto | VERIFIED | `ipc_server.py` line 95: `os.chmod(self._socket_path, 0o660)`; lines 98-108: `os.chown(..., 0, oap_gid)` with KeyError/PermissionError guards |
| 3 | Peer credentials checked on every new connection — root or openauto group members accepted | VERIFIED | `ipc_server.py` `_handle_client()` lines 124-139: calls `self._authorizer(sock)` before any message processing; `check_peer_authorized()` implements the full policy |
| 4 | Unauthorized callers receive a JSON error and are disconnected | VERIFIED | `ipc_server.py` lines 127-139: sends `{"error": {"code": -32600, "message": "permission denied"}}` + newline, then returns (writer closed in finally block) |
| 5 | Tests prove authorization policy deterministically via DI — root passes, openauto member passes, non-member fails — without relying on test runner identity | VERIFIED | `test_ipc_auth.py`: 10 tests across 3 classes; all mock OS calls; 18/18 pass |
| 6 | PR-03 proven at policy/permission-model level; end-to-end client connectivity deferred to manual post-install | VERIFIED | Automated: socket mode 0660, authorizer accepts openauto members; manual boundary documented in plans and human_verification section above |
| 7 | Fresh install.sh run produces systemd unit matching service template | VERIFIED | `install.sh` lines 1762-1764: sed substitution of `@@PYTHON_PATH@@` and `@@SYS_DIR@@` from template; inline fallback with `User=root` also present |
| 8 | Fresh install-prebuilt.sh run produces systemd unit matching service template | VERIFIED | `install-prebuilt.sh` lines 589-591: identical sed rendering from template; same fallback with `User=root` |
| 9 | Upgrading from old User=$USER unit auto-migrates to User=root with notice | VERIFIED | Both installers: `sed -n 's/^User=//p'` detection, stop service, clean iptables, replace unit; migration notice printed |
| 10 | openauto group created and installing user added | VERIFIED | Both installers: `groupadd openauto` + `usermod -aG openauto "$USER"` with `getent group` / `id -nG` guards |
| 11 | Stale iptables proxy state cleaned during migration; no /run/openauto installer prep | VERIFIED | Both installers: `iptables -t nat -D/-F/-X OPENAUTO_PROXY`; no `mkdir/chown/chmod /run/openauto` found in either |

**Score:** 11/11 truths verified

---

### Required Artifacts

#### Plan 15-01 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `system-service/openauto-system.service.in` | Canonical service template with placeholders | VERIFIED | Exists, 27 lines; contains `User=root`, `@@PYTHON_PATH@@`, `@@SYS_DIR@@`, all 5 hardening directives |
| `system-service/ipc_server.py` | IPC server with injectable peer credential validation | VERIFIED | Exists, 222 lines; contains `SO_PEERCRED`, `authorizer` DI seam, `0o660`, `check_peer_authorized()` |
| `system-service/tests/test_ipc_auth.py` | Auth policy tests via mocked caller identities, min 60 lines | VERIFIED | Exists, 218 lines; 10 tests across 3 classes covering all 5 caller scenarios |
| `system-service/openauto-system.service` (deleted) | Old file must be gone | VERIFIED | File not present — `git rm` confirmed |

#### Plan 15-02 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `install.sh` | create_system_service with migration + template rendering | VERIFIED | `groupadd openauto`, `OPENAUTO_PROXY` cleanup, `@@PYTHON_PATH@@` sed rendering, session refresh warn |
| `install-prebuilt.sh` | Identical migration logic with adjusted template path | VERIFIED | Same patterns; template path uses `$PAYLOAD_DIR` fallback; no drift from install.sh logic |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `ipc_server.py` | socket at /run/openauto/system.sock | `os.chmod(0o660)` after bind | VERIFIED | Line 95: `os.chmod(self._socket_path, 0o660)` |
| `ipc_server.py` | peer credential check | `SO_PEERCRED` on accepted connection via injectable authorizer | VERIFIED | Lines 124-125: `sock = writer.get_extra_info("socket")` + `self._authorizer(sock)` |
| `install.sh` | `system-service/openauto-system.service.in` | sed `@@PYTHON_PATH@@` substitution | VERIFIED | Lines 1762-1764: `sed -e "s|@@PYTHON_PATH@@|$PYTHON_PATH|g" -e "s|@@SYS_DIR@@|$SYS_DIR|g" "$TEMPLATE"` |
| `install-prebuilt.sh` | `system-service/openauto-system.service.in` | sed `@@PYTHON_PATH@@` substitution | VERIFIED | Lines 589-591: same sed pattern with `$PAYLOAD_DIR` template path fallback |
| `test_ipc_auth.py` | `ipc_server.check_peer_authorized` | direct import + mock patching | VERIFIED | `from ipc_server import IpcServer, check_peer_authorized`; `@patch("ipc_server.grp")` etc. |
| `test_ipc_server.py` | `IpcServer` with permissive authorizer | `_always_authorized` injected in setUp | VERIFIED | Line 21: `IpcServer(self.socket_path, authorizer=_always_authorized)`; 0o660 assertion on line 42 |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| PR-01 | 15-01 | `openauto-system` uses privilege model sufficient for all system-level operations | SATISFIED | `User=root` in service template; `ExecStopPost` iptables cleanup works under root |
| PR-02 | 15-01 | IPC socket restricted from world-writable; only intended local clients call privileged methods | SATISFIED | Mode 0660 (not 0666) + SO_PEERCRED auth rejects non-openauto callers with structured error |
| PR-03 | 15-01 | Intended Qt client can still connect after IPC permissions tightened | SATISFIED (policy level) | Authorization logic accepts openauto group members; socket 0660 allows group filesystem access; end-to-end requires manual post-install (see human_verification) |
| PR-04 | 15-02 | Installers and checked-in reference unit updated consistently | SATISFIED | Both installers render from `openauto-system.service.in`; old `.service` deleted; both create openauto group |
| TS-05 | 15-01 | Tests cover restricted IPC permissions while preserving access for intended client | SATISFIED | 10 tests: socket mode 0660, authorized path (message exchange), unauthorized path (error+disconnect), 5 policy unit tests via mocked identities |

No orphaned requirements — all 5 IDs appear in plan frontmatter and are accounted for. REQUIREMENTS.md tracks all as Complete.

---

### Anti-Patterns Found

None. Scanned `ipc_server.py`, `test_ipc_auth.py`, and `openauto-system.service.in` — no TODO/FIXME/placeholder comments, no empty implementations, no stub returns.

The `test_ipc_server.py` test named `test_socket_permissions_allow_non_root_clients` asserts `0o660` (not the old `0o666`), confirming the migration from permissive to restrictive permissions is reflected in both the implementation and the legacy tests.

---

### Human Verification Required

#### 1. Post-install Qt client connectivity (PR-03 end-to-end)

**Test:** On the Pi, run `install.sh` (or `install-prebuilt.sh`). Reboot or logout/login to pick up the new `openauto` group membership. Start `openauto-prodigy.service`. Check `journalctl -u openauto-prodigy -u openauto-system --since "5 min ago"` for any "permission denied" socket errors from `SystemServiceClient`.

**Expected:** No permission denied errors. Qt client connects to `/run/openauto/system.sock` successfully. `openauto-system.service` running as root; `openauto-prodigy.service` running as the user (who is now in the openauto group).

**Why human:** `usermod -aG openauto $USER` does not take effect in the current login session. Group membership requires a session refresh (logout/login or reboot) before the Qt process inherits the new supplementary GID. This cannot be tested in the same CI/test runner session where `usermod` runs. The authorization policy itself (which accepts openauto group members) is fully proven by automated tests.

---

### Gaps Summary

None. All 11 observable truths verified, all artifacts exist and are substantive, all key links wired. Requirements PR-01 through PR-04 and TS-05 are satisfied. The one human verification item (PR-03 end-to-end connectivity) is explicitly out of scope for automated testing per the phase plan's documented boundary — it is a post-install manual check, not a gap.

---

## Commit Verification

All four task commits confirmed in git history:

| Commit | Description |
|--------|-------------|
| `57a2498` | feat(15-01): service template, socket lockdown, and injectable peer credential auth |
| `796324a` | test(15-01): IPC authorization policy tests via dependency injection (TS-05) |
| `2b3fb9a` | feat(15-02): update install.sh with privilege model migration and template rendering |
| `aed3b76` | feat(15-02): update install-prebuilt.sh with identical migration and template rendering |

---

_Verified: 2026-03-16T02:30:00Z_
_Verifier: Claude (gsd-verifier)_
