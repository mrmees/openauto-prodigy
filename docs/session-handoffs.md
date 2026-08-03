# Session Handoffs

Newest entries first.

---

## 2026-08-02 — External data-provider API contract approved and reviewed

**What changed:** added the ACTIVE external data-provider API design and linked
it from the documentation index. The contract is source-agnostic: one
authenticated session owns one live provider namespace, declares arbitrary
typed-scalar channels, and publishes at backend-selected cadence. Consumers
discover the live catalog and subscribe by exact provider/channel identity;
subscriptions wait through provider absence and resume automatically. Prodigy
retains only current values, performs no semantic arbitration, conversion,
rate selection, history, or command routing, and exposes web widgets only
through the same additive protobuf API.

**Why:** the original OBD framing was too narrow. The intended boundary must be
usable by independently developed OBD, CAN, GPIO, MQTT, simulator, or other
backends and by Gauge Studio without making Prodigy a source-specific policy or
performance gatekeeper.

**Status:** DESIGN APPROVED; the Prodigy implementation plan is written and the
work is promoted in the current roadmap. No production code, protobuf, Gauge
Studio, or backend source changed in this session.

**Review:** the requested read-only Opus 4.6 pass returned `APPROVE WITH
CHANGES`: BLOCKER 0, MAJOR 2, MINOR 6. All eight findings were confirmed and
incorporated. The two majors added snapshot-guarded/reentrancy-safe fan-out and
an explicit `Ack`-before-catalog-snapshot contract. The minors clarified
request-scoped declaration rejection, empty scalar presence, namespace-switch
rejection, metadata-change ordering, JavaScript timestamp conversion, and
reconnect-gap request behavior. Matthew then supplied a Fable follow-up review;
its five actionable tightenings were confirmed and incorporated. Staleness now
uses a local monotonic receipt deadline rather than wall clocks, half-open TCP
behavior is explicitly dispositioned, duplicate publications are normalized
before forwarding, request-ID misuse has deterministic handling, and catalog
watch amplification is an accepted trusted-provider risk. Fable's namespace
squatting observation required no edit because Section 18 already covers it.

**Verification:** `python3 scripts/check-doc-links.py`, tracked and untracked
Markdown whitespace checks, placeholder/ambiguity searches, and live-schema
allocation inspection passed. Opus independently confirmed that capability
field 3 and envelope fields 80–94 are available at the grounded commit and that
the proposed proto3 presence/oneof shapes are legal. Application builds and
CTest were not run because this change is documentation only. The tracked-live
link check, `git diff --check`, and targeted stale/request-ID ambiguity search
were rerun after the Fable remediation edits. The implementation plan was
self-reviewed against the approved contract for schema, lifecycle, ordering,
clock, backpressure, JavaScript, and cross-repository boundaries; its
placeholder scan and tracked-live link check passed.

**Planning:** the executable Prodigy plan implements the public contract first,
then requires a separate Gauge Studio plan against `window.prodigy.data`. The
`obd-plugin` path is currently an empty, non-git directory, so provider language,
packaging, and source-hardware ownership remain a later backend-repository
decision rather than being smuggled into the generic protocol.

**Next 1–3 steps:** execute the Prodigy implementation plan; write and execute
the Gauge Studio integration plan against the finished public shim; bootstrap a
provider repository and live-test its deterministic fixture with an exported
gauge on the Pi before declaring the cross-repository feature complete.

---

## 2026-08-01 — Fresh-Pi sysfs backlight startup crash

**What changed:** fixed a deterministic `DisplayService` construction-order
bug exposed by a fresh Raspberry Pi with a real sysfs backlight. The constructor
previously called `detectBackend()` while initializing `backend_`; that method
assigned `sysfsPath_` before its `QString` lifetime began. Backend detection now
runs in the constructor body after all members are constructed. No brightness
policy or public API changed.

**Status:** FIXED AND LIVE-VERIFIED on `prodigy2.local` at commit `c46bac8`.
The original service failure reproduced five times as `SIGBUS`; GDB traced it
through `QString::operator=` to `DisplayService::detectBackend()`. The original
Pi has no sysfs backlight, while `prodigy2` exposes `10-0045`, confirming why
only the fresh device entered the invalid path. The reviewed ARM binary is
deployed; the pre-fix binary remains recoverable as
`build/src/openauto-prodigy.pre-display-fix`.

**Verification:** the focused display test, native build, explicit
`openauto-prodigy` target, complete offscreen CTest suite, and
`./cross-build.sh` passed. The deployed ARM binary matched SHA-256
`73374ca446b5010fbdb435caed5ce85aacc8a4862355536c59c3ab887d542146`.
A condition-driven manual launch on `prodigy2` detected
`/sys/class/backlight/10-0045`, selected backend `sysfs`, reached
`sd_notify: READY=1 sent`, and shut down cleanly. The node reports max 255 and
is writable by `matt` through group `video`.

**Review:** the standard Opus review covered immutable range
`f022a9d..c46bac8` and reported BLOCKER=0, MAJOR=0, MINOR=3. The small-range
floor and write-permission concerns were dismissed for the supported live
target using direct hardware evidence; synthetic sysfs coverage and generic
fallback behavior were deferred to the engineering backlog. No remediation
pass was needed.

**Next 1–3 steps:** reset the service start limit and start it from an
interactive sudo session; confirm systemd reaches active/running; push the
local installer and display fixes only after explicit user authorization.

---

## 2026-08-01 — Fresh source install protocol-submodule recovery

**What changed:** corrected `install.sh` so a normal initialized protocol
submodule (whose `.git` is a file) is never mistaken for an absent checkout.
The installer now uses a dedicated helper to fetch only the OAA `dist` branch,
normalize its fetchspec, retain conventional gitfile ownership, and check out
the exact gitlink pinned by the Prodigy source tree. A real temporary-repository
regression covers both fresh initialization and an immediate initialized rerun.
The hands-off protocol content was not edited.

**Status:** VERIFIED LOCALLY AND ON `prodigy2.local`; full installer resumption
awaits the operator entering the local sudo password. The reviewed patch is at
`9e934faccc89b39f2d387bc4fc6e9a2d9da34fe3` and remains two local commits ahead
of `origin/dev`. The release checkout on `prodigy2` is intentionally patched
with `install.sh` plus the new helper; its superproject remains `cd4b6b0` and
its protocol checkout remains the v1.5 pin `5ff4aa2` with a `dist`-only
fetchspec.

**Verification:** the regression was observed failing against both the
original clone-over-gitfile path and the first default-branch submodule fix.
`python3 tests/test_install_source_lifecycle.py`,
`python3 tests/test_ops_install_contracts.py`,
`python3 tests/test_install_list_prebuilt.py`,
`python3 tests/test_installer_hardware_contracts.py`, `bash -n install.sh
scripts/initialize-protocol-submodule.sh`, and `git diff --check` passed. The
helper also completed directly on `prodigy2` and preserved both exact SHAs.

**Review:** Opus pass 1 reported BLOCKER=0, MAJOR=1, MINOR=2. The branch-scope,
pin-integrity, shallow-clone, and inert-test concerns were remediated. Pass 2
reported BLOCKER=0, MAJOR=1, MINOR=4. The orphaned-gitdir recovery edge, direct
SHA fallback, tracked-URL resync, and related migration coverage were confirmed
or retained as nonblocking follow-up; the full `dist` history is an intentional
pin-integrity tradeoff. Those leads are recorded in the engineering backlog.

**Next 1–3 steps:** rerun `bash install.sh --mode source` locally on
`prodigy2`; confirm the source build and service installation complete; obtain
explicit user authorization before pushing the two installer commits.
